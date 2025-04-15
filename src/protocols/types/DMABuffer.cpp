#include "DMABuffer.hpp"
#include "WLBuffer.hpp"
#include "../../desktop/LayerSurface.hpp"
#include "../../render/Renderer.hpp"
#include "../../helpers/Format.hpp"

#if defined(__linux__)
#include <linux/dma-buf.h>
#include <linux/sync_file.h>
#include <sys/ioctl.h>
#endif

using namespace Hyprutils::OS;

CDMABuffer::CDMABuffer(uint32_t id, wl_client* client, Aquamarine::SDMABUFAttrs const& attrs_) : attrs(attrs_) {
    g_pHyprRenderer->makeEGLCurrent();

    listeners.resourceDestroy = events.destroy.registerListener([this](std::any d) {
        closeFDs();
        listeners.resourceDestroy.reset();
    });

    size     = attrs.size;
    resource = CWLBufferResource::create(makeShared<CWlBuffer>(client, 1, id));

    auto eglImage = g_pHyprOpenGL->createEGLImage(attrs);

    if UNLIKELY (!eglImage) {
        Debug::log(ERR, "CDMABuffer: failed to import EGLImage, retrying as implicit");
        attrs.modifier = DRM_FORMAT_MOD_INVALID;
        eglImage       = g_pHyprOpenGL->createEGLImage(attrs);
        if UNLIKELY (!eglImage) {
            Debug::log(ERR, "CDMABuffer: failed to import EGLImage");
            return;
        }
    }

    texture = makeShared<CTexture>(attrs, eglImage); // texture takes ownership of the eglImage
    opaque  = NFormatUtils::isFormatOpaque(attrs.format);
    success = texture->m_iTexID;

    if UNLIKELY (!success)
        Debug::log(ERR, "Failed to create a dmabuf: texture is null");
}

CDMABuffer::~CDMABuffer() {
    if (resource)
        resource->sendRelease();

    closeFDs();
}

Aquamarine::eBufferCapability CDMABuffer::caps() {
    return Aquamarine::eBufferCapability::BUFFER_CAPABILITY_DATAPTR;
}

Aquamarine::eBufferType CDMABuffer::type() {
    return Aquamarine::eBufferType::BUFFER_TYPE_DMABUF;
}

void CDMABuffer::update(const CRegion& damage) {
    ;
}

bool CDMABuffer::isSynchronous() {
    return false;
}

Aquamarine::SDMABUFAttrs CDMABuffer::dmabuf() {
    return attrs;
}

std::tuple<uint8_t*, uint32_t, size_t> CDMABuffer::beginDataPtr(uint32_t flags) {
    // FIXME:
    return {nullptr, 0, 0};
}

void CDMABuffer::endDataPtr() {
    // FIXME:
}

bool CDMABuffer::good() {
    return success;
}

void CDMABuffer::closeFDs() {
    for (int i = 0; i < attrs.planes; ++i) {
        if (attrs.fds[i] == -1)
            continue;
        close(attrs.fds[i]);
        attrs.fds[i] = -1;
    }
    attrs.planes = 0;
}

static int doIoctl(int fd, unsigned long request, void* arg) {
    int ret;

    do {
        ret = ioctl(fd, request, arg);
    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));
    return ret;
}

// https://www.kernel.org/doc/html/latest/driver-api/dma-buf.html#c.dma_buf_export_sync_file
// returns a sync file that will be signalled when dmabuf is ready to be read
CFileDescriptor CDMABuffer::exportSyncFile() {
    if (!good())
        return {};

#if !defined(__linux__)
    return {};
#else
    std::vector<CFileDescriptor> syncFds(attrs.fds.size());
    for (const auto& fd : attrs.fds) {
        if (fd == -1)
            continue;

        dma_buf_export_sync_file request{
            .flags = DMA_BUF_SYNC_READ,
            .fd    = -1,
        };

        if (doIoctl(fd, DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &request) == 0)
            syncFds.emplace_back(request.fd);
    }

    if (syncFds.empty())
        return {};

    CFileDescriptor syncFd;
    for (auto& fd : syncFds) {
        if (!syncFd.isValid()) {
            syncFd = std::move(fd);
            continue;
        }

        struct sync_merge_data data{
            .name  = "merged release fence",
            .fd2   = fd.get(),
            .fence = -1,
        };

        if (doIoctl(syncFd.get(), SYNC_IOC_MERGE, &data) == 0)
            syncFd = CFileDescriptor(data.fence);
        else
            syncFd = {};
    }

    return syncFd;
#endif
}
