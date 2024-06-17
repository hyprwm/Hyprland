#include "DMABuffer.hpp"
#include "WLBuffer.hpp"
#include "../../render/Renderer.hpp"
#include "../../helpers/Format.hpp"

CDMABuffer::CDMABuffer(uint32_t id, wl_client* client, SDMABUFAttrs const& attrs_) : attrs(attrs_) {
    g_pHyprRenderer->makeEGLCurrent();

    listeners.resourceDestroy = events.destroy.registerListener([this](std::any d) {
        closeFDs();
        listeners.resourceDestroy.reset();
    });

    size     = attrs.size;
    resource = CWLBufferResource::create(makeShared<CWlBuffer>(client, 1, id));

    auto eglImage = g_pHyprOpenGL->createEGLImage(attrs);

    if (!eglImage)
        return;

    texture = makeShared<CTexture>(attrs, eglImage); // texture takes ownership of the eglImage
    opaque  = FormatUtils::isFormatOpaque(attrs.format);
    success = texture->m_iTexID;

    if (!success)
        Debug::log(ERR, "Failed to create a dmabuf: texture is null");
}

CDMABuffer::~CDMABuffer() {
    closeFDs();
}

eBufferCapability CDMABuffer::caps() {
    return BUFFER_CAPABILITY_DATAPTR;
}

eBufferType CDMABuffer::type() {
    return BUFFER_TYPE_DMABUF;
}

void CDMABuffer::update(const CRegion& damage) {
    ;
}

bool CDMABuffer::isSynchronous() {
    return false;
}

SDMABUFAttrs CDMABuffer::dmabuf() {
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

void CDMABuffer::updateTexture() {
    ;
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