#include "DMABuffer.hpp"
#include "WLBuffer.hpp"
#include "../../desktop/view/LayerSurface.hpp"
#include "../../render/Renderer.hpp"
#include "../../helpers/Format.hpp"
#include <hyprgraphics/egl/Egl.hpp>
#include "../../helpers/Drm.hpp"

using namespace Hyprutils::OS;
using namespace Hyprgraphics::Egl;

CDMABuffer::CDMABuffer(uint32_t id, wl_client* client, Aquamarine::SDMABUFAttrs const& attrs_) : m_attrs(attrs_) {
    m_listeners.resourceDestroy = events.destroy.listen([this] {
        closeFDs();
        m_listeners.resourceDestroy.reset();
    });

    size       = m_attrs.size;
    m_resource = CWLBufferResource::create(makeShared<CWlBuffer>(client, 1, id));
    m_opaque   = isDrmFormatOpaque(m_attrs.format);
    m_texture  = g_pHyprRenderer->createTexture(m_attrs, m_opaque); // texture takes ownership of the eglImage

    if UNLIKELY (!m_texture) {
        Log::logger->log(Log::ERR, "CDMABuffer: failed to import EGLImage, retrying as implicit");
        m_attrs.modifier = DRM_FORMAT_MOD_INVALID;
        m_texture        = g_pHyprRenderer->createTexture(m_attrs, m_opaque);

        if UNLIKELY (!m_texture) {
            Log::logger->log(Log::ERR, "CDMABuffer: failed to import EGLImage");
            return;
        }
    }

    m_success = m_texture->ok();

    if UNLIKELY (!m_success)
        Log::logger->log(Log::ERR, "Failed to create a dmabuf: texture is null");
}

CDMABuffer::~CDMABuffer() {
    if (m_resource)
        m_resource->sendRelease();

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
    return m_attrs;
}

std::tuple<uint8_t*, uint32_t, size_t> CDMABuffer::beginDataPtr(uint32_t flags) {
    // FIXME:
    return {nullptr, 0, 0};
}

void CDMABuffer::endDataPtr() {
    // FIXME:
}

bool CDMABuffer::good() {
    return m_success;
}

void CDMABuffer::closeFDs() {
    for (int i = 0; i < m_attrs.planes; ++i) {
        if (m_attrs.fds[i] == -1)
            continue;
        close(m_attrs.fds[i]);
        m_attrs.fds[i] = -1;
    }
    m_attrs.planes = 0;
}

// https://www.kernel.org/doc/html/latest/driver-api/dma-buf.html#c.dma_buf_export_sync_file
// returns a sync file that will be signalled when dmabuf is ready to be read
CFileDescriptor CDMABuffer::exportSyncFile() {
    if (!good())
        return {};

#if !defined(__linux__)
    return {};
#else
    std::vector<CFileDescriptor> syncFds;
    syncFds.reserve(m_attrs.fds.size());

    for (const auto& fd : m_attrs.fds) {
        if (fd == -1)
            continue;

        // buffer readability checks are rather slow on some Intel laptops
        // see https://gitlab.freedesktop.org/drm/intel/-/issues/9415
        if (g_pHyprRenderer && !g_pHyprRenderer->isIntel()) {
            if (CFileDescriptor::isReadable(fd))
                continue;
        }

        auto fence = DRM::exportFence(fd);

        if (fence.isValid())
            syncFds.emplace_back(std::move(fence));
    }

    if (syncFds.empty())
        return {};

    CFileDescriptor syncFd;
    for (auto& fd : syncFds) {
        if (!syncFd.isValid()) {
            syncFd = std::move(fd);
            continue;
        }

        syncFd = DRM::mergeFence(syncFd.take(), fd.take());
    }

    return syncFd;
#endif
}
