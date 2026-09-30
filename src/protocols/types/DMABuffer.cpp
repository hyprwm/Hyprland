#include "DMABuffer.hpp"
#include "WLBuffer.hpp"
#include "../../desktop/view/LayerSurface.hpp"
#include "../../render/Renderer.hpp"
#include "../../helpers/Format.hpp"
#include "helpers/Drm.hpp"
#include <hyprgraphics/egl/Egl.hpp>

using namespace Hyprutils::OS;
using namespace Hyprgraphics::Egl;

CDMABuffer::CDMABuffer(uint32_t id, wl_client* client, const Aquamarine::SDMABUFAttrs& attrs_, std::array<CFileDescriptor, 4> fds) : m_attrs(attrs_), m_fds(std::move(fds)) {
    for (size_t i = 0; i < m_fds.size(); ++i)
        m_attrs.fds[i] = m_fds[i].get();

    m_listeners.resourceDestroy = events.destroy.listen([this] {
        closeFDs();
        m_listeners.resourceDestroy.reset();
    });

    size       = m_attrs.size;
    m_resource = CWLBufferResource::create(makeShared<CWlBuffer>(client, 1, id));
    if UNLIKELY (!m_resource->good())
        return;

    m_opaque  = isDrmFormatOpaque(m_attrs.format);
    m_texture = g_pHyprRenderer->createTexture(m_attrs, m_opaque); // texture takes ownership of the eglImage

    if UNLIKELY (!m_texture) {
        LOG(Log::ERR, "CDMABuffer: failed to import EGLImage, retrying as implicit");
        m_attrs.modifier = DRM_FORMAT_MOD_INVALID;
        m_texture        = g_pHyprRenderer->createTexture(m_attrs, m_opaque);

        if UNLIKELY (!m_texture) {
            LOG(Log::ERR, "CDMABuffer: failed to import EGLImage");
            return;
        }
    }

    m_success = m_texture->ok();

    if UNLIKELY (!m_success)
        LOG(Log::ERR, "Failed to create a dmabuf: texture is null");
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
    for (auto& fd : m_fds)
        fd.reset();

    m_attrs.fds.fill(-1);
    m_attrs.planes = 0;
}

std::vector<CFileDescriptor> CDMABuffer::exportSyncFiles() {
    if (!good())
        return {};

#ifndef __linux__
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

        CFileDescriptor fence = DRM::exportFence(fd);
        if (fence.isValid())
            syncFds.emplace_back(std::move(fence));
    }

    return syncFds;
#endif
}
