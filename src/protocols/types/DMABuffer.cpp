#include "DMABuffer.hpp"
#include "WLBuffer.hpp"
#include "../../desktop/LayerSurface.hpp"
#include "../../render/Renderer.hpp"
#include "../../helpers/Format.hpp"

CDMABuffer::CDMABuffer(uint32_t id, wl_client* client, Aquamarine::SDMABUFAttrs&& attrs_) : attrs(std::move(attrs_)) {
    g_pHyprRenderer->makeEGLCurrent();

    listeners.resourceDestroy = events.destroy.registerListener([this](std::any d) { listeners.resourceDestroy.reset(); });

    size     = attrs.size;
    resource = CWLBufferResource::create(makeShared<CWlBuffer>(client, 1, id));

    auto eglImage = g_pHyprOpenGL->createEGLImage(attrs);

    if UNLIKELY (!eglImage) {
        implicit = {.success  = attrs.success,
                    .size     = attrs.size,
                    .format   = attrs.format,
                    .modifier = DRM_FORMAT_MOD_INVALID,
                    .planes   = attrs.planes,
                    .offsets  = attrs.offsets,
                    .strides  = attrs.strides};
        for (auto i = 0; i < attrs.planes; i++)
            implicit.fds.at(i) = attrs.fds.at(i).duplicate();

        eglImage = g_pHyprOpenGL->createEGLImage(implicit);
        if UNLIKELY (!eglImage) {
            Debug::log(ERR, "CDMABuffer: failed to import EGLImage");
            return;
        }

        texture = makeShared<CTexture>(implicit, eglImage); // texture takes ownership of the eglImage
        opaque  = NFormatUtils::isFormatOpaque(implicit.format);
    } else {
        texture = makeShared<CTexture>(attrs, eglImage); // texture takes ownership of the eglImage
        opaque  = NFormatUtils::isFormatOpaque(attrs.format);
    }

    success = texture->m_iTexID;
    if UNLIKELY (!success)
        Debug::log(ERR, "Failed to create a dmabuf: texture is null");
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

const Aquamarine::SDMABUFAttrs& CDMABuffer::dmabuf() const {
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
