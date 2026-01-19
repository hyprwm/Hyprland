#include "DMABuffer.hpp"
#include "WLBuffer.hpp"
#include "../../desktop/view/LayerSurface.hpp"
#include "../../render/Renderer.hpp"
#include "../../helpers/Format.hpp"

using namespace Hyprutils::OS;

CDMABuffer::CDMABuffer(uint32_t id, wl_client* client, Aquamarine::SDMABUFAttrs const& attrs_) : m_attrs(attrs_) {
    g_pHyprRenderer->makeEGLCurrent();

    m_listeners.resourceDestroy = events.destroy.listen([this] {
        closeFDs();
        m_listeners.resourceDestroy.reset();
    });

    size          = m_attrs.size;
    m_resource    = CWLBufferResource::create(makeShared<CWlBuffer>(client, 1, id));
    auto eglImage = g_pHyprOpenGL->createEGLImage(m_attrs);

    if UNLIKELY (!eglImage) {
        Log::logger->log(Log::ERR, "CDMABuffer: failed to import EGLImage, retrying as implicit");
        m_attrs.modifier = DRM_FORMAT_MOD_INVALID;
        eglImage         = g_pHyprOpenGL->createEGLImage(m_attrs);

        if UNLIKELY (!eglImage) {
            Log::logger->log(Log::ERR, "CDMABuffer: failed to import EGLImage");
            return;
        }
    }

    m_texture = makeShared<CTexture>(m_attrs, eglImage); // texture takes ownership of the eglImage
    m_opaque  = NFormatUtils::isFormatOpaque(m_attrs.format);
    m_success = m_texture->m_texID;

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

CFence CDMABuffer::exportFence() {
    return CFence(m_attrs.fds);
}
