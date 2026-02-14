#include "Framebuffer.hpp"

bool IFramebuffer::alloc(int w, int h, uint32_t format) {
    RASSERT((w > 0 && h > 0), "cannot alloc a FB with negative / zero size! (attempted {}x{})", w, h);

    const bool sizeChanged   = (m_size != Vector2D(w, h));
    const bool formatChanged = (format != m_drmFormat);

    if (m_fbAllocated && !sizeChanged && !formatChanged)
        return true;

    m_size        = {w, h};
    m_drmFormat   = format;
    m_fbAllocated = internalAlloc(w, h, format);
    return m_fbAllocated;
}

bool IFramebuffer::isAllocated() {
    return m_fbAllocated && m_tex;
}

SP<ITexture> IFramebuffer::getTexture() {
    return m_tex;
}

SP<ITexture> IFramebuffer::getStencilTex() {
    return m_stencilTex;
}
