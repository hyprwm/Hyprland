#include "Texture.hpp"
#include "Renderer.hpp"
#include "../Compositor.hpp"
#include "../protocols/types/Buffer.hpp"
#include "../helpers/Format.hpp"
#include <cstring>

CTexture::CTexture() = default;

CTexture::~CTexture() {
    if (!g_pCompositor || g_pCompositor->m_bIsShuttingDown || !g_pHyprRenderer)
        return;

    g_pHyprRenderer->makeEGLCurrent();
    destroyTexture();
}

CTexture::CTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size_, bool keepDataCopy) : m_iDrmFormat(drmFormat), m_bKeepDataCopy(keepDataCopy) {
    createFromShm(drmFormat, pixels, stride, size_);
}

CTexture::CTexture(const Aquamarine::SDMABUFAttrs& attrs, void* image) {
    createFromDma(attrs, image);
}

CTexture::CTexture(const SP<Aquamarine::IBuffer> BUFFER, bool keepDataCopy) : m_bKeepDataCopy(keepDataCopy) {
    if (!BUFFER)
        return;

    m_bOpaque = BUFFER->opaque;

    auto attrs = BUFFER->dmabuf();

    if (!attrs.success) {
        // attempt shm
        auto shm = BUFFER->shm();

        if (!shm.success) {
            NDebug::log(ERR, "Cannot create a texture: BUFFER has no dmabuf or shm");
            return;
        }

        auto [pixelData, fmt, bufLen] = BUFFER->beginDataPtr(0);

        m_iDrmFormat = fmt;

        createFromShm(fmt, pixelData, bufLen, shm.size);
        return;
    }

    auto image = g_pHyprOpenGL->createEGLImage(BUFFER->dmabuf());

    if (!image) {
        NDebug::log(ERR, "Cannot create a texture: failed to create an EGLImage");
        return;
    }

    createFromDma(attrs, image);
}

void CTexture::createFromShm(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size_) {
    g_pHyprRenderer->makeEGLCurrent();

    const auto FORMAT = NFormatUtils::getPixelFormatFromDRM(drmFormat);
    ASSERT(FORMAT);

    m_iType = FORMAT->withAlpha ? TEXTURE_RGBA : TEXTURE_RGBX;
    m_vSize = size_;
    allocate();

    GLCALL(glBindTexture(GL_TEXTURE_2D, m_iTexID));
    GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
#ifndef GLES2
    if (FORMAT->flipRB) {
        GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE));
        GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED));
    }
#endif
    GLCALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, stride / FORMAT->bytesPerBlock));
    GLCALL(glTexImage2D(GL_TEXTURE_2D, 0, FORMAT->glInternalFormat ? FORMAT->glInternalFormat : FORMAT->glFormat, size_.x, size_.y, 0, FORMAT->glFormat, FORMAT->glType, pixels));
    GLCALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0));
    GLCALL(glBindTexture(GL_TEXTURE_2D, 0));

    if (m_bKeepDataCopy) {
        m_vDataCopy.resize(stride * size_.y);
        memcpy(m_vDataCopy.data(), pixels, stride * size_.y);
    }
}

void CTexture::createFromDma(const Aquamarine::SDMABUFAttrs& attrs, void* image) {
    if (!g_pHyprOpenGL->m_sProc.glEGLImageTargetTexture2DOES) {
        NDebug::log(ERR, "Cannot create a dmabuf texture: no glEGLImageTargetTexture2DOES");
        return;
    }

    m_bOpaque = NFormatUtils::isFormatOpaque(attrs.FORMAT);
    m_iTarget = GL_TEXTURE_2D;
    m_iType   = TEXTURE_RGBA;
    m_vSize   = attrs.size;
    m_iType   = NFormatUtils::isFormatOpaque(attrs.FORMAT) ? TEXTURE_RGBX : TEXTURE_RGBA;
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

    const auto FORMAT = NFormatUtils::getPixelFormatFromDRM(drmFormat);
    ASSERT(FORMAT);

    glBindTexture(GL_TEXTURE_2D, m_iTexID);

    auto rects = damage.copy().intersect(CBox{{}, m_vSize}).getRects();

#ifndef GLES2
    if (FORMAT->flipRB) {
        GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE));
        GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED));
    }
#endif

    for (auto const& rect : rects) {
        GLCALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, stride / FORMAT->bytesPerBlock));
        GLCALL(glPixelStorei(GL_UNPACK_SKIP_PIXELS_EXT, rect.x1));
        GLCALL(glPixelStorei(GL_UNPACK_SKIP_ROWS_EXT, rect.y1));

        int width  = rect.x2 - rect.x1;
        int height = rect.y2 - rect.y1;
        GLCALL(glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x1, rect.y1, width, height, FORMAT->glFormat, FORMAT->glType, pixels));
    }

    GLCALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0));
    GLCALL(glPixelStorei(GL_UNPACK_SKIP_PIXELS_EXT, 0));
    GLCALL(glPixelStorei(GL_UNPACK_SKIP_ROWS_EXT, 0));

    glBindTexture(GL_TEXTURE_2D, 0);

    if (m_bKeepDataCopy) {
        m_vDataCopy.resize(stride * m_vSize.y);
        memcpy(m_vDataCopy.data(), pixels, stride * m_vSize.y);
    }
}

void CTexture::destroyTexture() {
    if (m_iTexID) {
        GLCALL(glDeleteTextures(1, &m_iTexID));
        m_iTexID = 0;
    }

    if (m_pEglImage)
        g_pHyprOpenGL->m_sProc.eglDestroyImageKHR(g_pHyprOpenGL->m_pEglDisplay, m_pEglImage);
    m_pEglImage = nullptr;
}

void CTexture::allocate() {
    if (!m_iTexID)
        GLCALL(glGenTextures(1, &m_iTexID));
}

const std::vector<uint8_t>& CTexture::dataCopy() {
    return m_vDataCopy;
}
