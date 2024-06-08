#include "Texture.hpp"
#include "Renderer.hpp"
#include "../Compositor.hpp"
#include "../protocols/types/Buffer.hpp"
#include "../helpers/Format.hpp"

CTexture::CTexture() {
    // naffin'
}

CTexture::~CTexture() {
    if (m_bNonOwning)
        return;

    g_pHyprRenderer->makeEGLCurrent();
    destroyTexture();
}

CTexture::CTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size_) {
    g_pHyprRenderer->makeEGLCurrent();

    const auto format = FormatUtils::getPixelFormatFromDRM(drmFormat);
    ASSERT(format);

    m_iType = format->withAlpha ? TEXTURE_RGBA : TEXTURE_RGBX;
    m_vSize = size_;
    allocate();

    GLCALL(glBindTexture(GL_TEXTURE_2D, m_iTexID));
    GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
#ifndef GLES2
    if (format->flipRB) {
        GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE));
        GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED));
    }
#endif
    GLCALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, stride / format->bytesPerBlock));
    GLCALL(glTexImage2D(GL_TEXTURE_2D, 0, format->glInternalFormat ? format->glInternalFormat : format->glFormat, size_.x, size_.y, 0, format->glFormat, format->glType, pixels));
    GLCALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0));
    GLCALL(glBindTexture(GL_TEXTURE_2D, 0));
}

CTexture::CTexture(wlr_texture* tex) {
    RASSERT(wlr_texture_is_gles2(tex), "wlr_texture provided to CTexture that isn't GLES2!");
    wlr_gles2_texture_attribs attrs;
    wlr_gles2_texture_get_attribs(tex, &attrs);

    m_iTarget    = attrs.target;
    m_iTexID     = attrs.tex;
    m_bNonOwning = true;

    if (m_iTarget == GL_TEXTURE_2D)
        m_iType = attrs.has_alpha ? TEXTURE_RGBA : TEXTURE_RGBX;
    else
        m_iType = TEXTURE_EXTERNAL;

    m_vSize = Vector2D(tex->width, tex->height);
}

CTexture::CTexture(const SDMABUFAttrs& attrs, void* image) {
    if (!g_pHyprOpenGL->m_sProc.glEGLImageTargetTexture2DOES) {
        Debug::log(ERR, "Cannot create a dmabuf texture: no glEGLImageTargetTexture2DOES");
        return;
    }

    m_iTarget = GL_TEXTURE_2D;
    m_iType   = TEXTURE_RGBA;
    m_vSize   = attrs.size;
    m_iType   = FormatUtils::isFormatOpaque(attrs.format) ? TEXTURE_RGBX : TEXTURE_RGBA;
    allocate();
    m_pEglImage = image;

    GLCALL(glBindTexture(GL_TEXTURE_2D, m_iTexID));
    GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GLCALL(g_pHyprOpenGL->m_sProc.glEGLImageTargetTexture2DOES(m_iTarget, image));
    GLCALL(glBindTexture(GL_TEXTURE_2D, 0));
}

void CTexture::update(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const CRegion& damage) {
    g_pHyprRenderer->makeEGLCurrent();

    const auto format = FormatUtils::getPixelFormatFromDRM(drmFormat);
    ASSERT(format);

    glBindTexture(GL_TEXTURE_2D, m_iTexID);

    auto rects = damage.copy().intersect(CBox{{}, m_vSize}).getRects();

#ifndef GLES2
    if (format->flipRB) {
        GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE));
        GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED));
    }
#endif

    for (auto& rect : rects) {
        GLCALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, stride / format->bytesPerBlock));
        GLCALL(glPixelStorei(GL_UNPACK_SKIP_PIXELS_EXT, rect.x1));
        GLCALL(glPixelStorei(GL_UNPACK_SKIP_ROWS_EXT, rect.y1));

        int width  = rect.x2 - rect.x1;
        int height = rect.y2 - rect.y1;
        GLCALL(glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x1, rect.y1, width, height, format->glFormat, format->glType, pixels));
    }

    GLCALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0));
    GLCALL(glPixelStorei(GL_UNPACK_SKIP_PIXELS_EXT, 0));
    GLCALL(glPixelStorei(GL_UNPACK_SKIP_ROWS_EXT, 0));

    glBindTexture(GL_TEXTURE_2D, 0);
}

void CTexture::destroyTexture() {
    if (m_iTexID) {
        GLCALL(glDeleteTextures(1, &m_iTexID));
        m_iTexID = 0;
    }

    if (m_pEglImage)
        g_pHyprOpenGL->m_sProc.eglDestroyImageKHR(wlr_egl_get_display(g_pCompositor->m_sWLREGL), m_pEglImage);
    m_pEglImage = nullptr;
}

void CTexture::allocate() {
    if (!m_iTexID)
        GLCALL(glGenTextures(1, &m_iTexID));
}
