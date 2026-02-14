#include "GLTexture.hpp"
#include "../Renderer.hpp"
#include "../../Compositor.hpp"
#include "../../helpers/Format.hpp"
#include "render/Texture.hpp"
#include <cstring>

CGLTexture::CGLTexture(bool opaque) {
    m_opaque = opaque;
}

CGLTexture::~CGLTexture() {
    if (!g_pCompositor || g_pCompositor->m_isShuttingDown || !g_pHyprRenderer)
        return;

    g_pHyprRenderer->makeEGLCurrent();
    if (m_texID) {
        GLCALL(glDeleteTextures(1, &m_texID));
        m_texID = 0;
    }

    if (m_eglImage)
        g_pHyprOpenGL->m_proc.eglDestroyImageKHR(g_pHyprOpenGL->m_eglDisplay, m_eglImage);
    m_eglImage = nullptr;
    m_cachedStates.fill(std::nullopt);
}

CGLTexture::CGLTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size_, bool keepDataCopy, bool opaque) :
    ITexture(drmFormat, pixels, stride, size_, keepDataCopy, opaque) {

    g_pHyprRenderer->makeEGLCurrent();

    const auto format = NFormatUtils::getPixelFormatFromDRM(drmFormat);
    ASSERT(format);

    m_type          = format->withAlpha ? TEXTURE_RGBA : TEXTURE_RGBX;
    m_size          = size_;
    m_isSynchronous = true;
    m_target        = GL_TEXTURE_2D;
    allocate();
    bind();
    setTexParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    setTexParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (format->swizzle.has_value())
        swizzle(format->swizzle.value());

    bool alignmentChanged = false;
    if (format->bytesPerBlock != 4) {
        const GLint alignment = (stride % 4 == 0) ? 4 : 1;
        GLCALL(glPixelStorei(GL_UNPACK_ALIGNMENT, alignment));
        alignmentChanged = true;
    }

    GLCALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, stride / format->bytesPerBlock));
    GLCALL(glTexImage2D(GL_TEXTURE_2D, 0, format->glInternalFormat ? format->glInternalFormat : format->glFormat, size_.x, size_.y, 0, format->glFormat, format->glType, pixels));
    GLCALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0));
    if (alignmentChanged)
        GLCALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 4));

    unbind();
}

CGLTexture::CGLTexture(const Aquamarine::SDMABUFAttrs& attrs, void* image, bool opaque) {
    m_opaque = opaque;
    if (!g_pHyprOpenGL->m_proc.glEGLImageTargetTexture2DOES) {
        Log::logger->log(Log::ERR, "Cannot create a dmabuf texture: no glEGLImageTargetTexture2DOES");
        return;
    }

    m_opaque = NFormatUtils::isFormatOpaque(attrs.format);

    // #TODO external only formats should be external aswell.
    // also needs a seperate color shader.
    /*if (NFormatUtils::isFormatYUV(attrs.format)) {
        m_target = GL_TEXTURE_EXTERNAL_OES;
        m_type   = TEXTURE_EXTERNAL;
    } else {*/
    m_target = GL_TEXTURE_2D;
    m_type   = NFormatUtils::isFormatOpaque(attrs.format) ? TEXTURE_RGBX : TEXTURE_RGBA;
    //}

    m_size = attrs.size;
    allocate();
    m_eglImage = image;

    bind();
    setTexParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    setTexParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLCALL(g_pHyprOpenGL->m_proc.glEGLImageTargetTexture2DOES(m_target, image));
    unbind();
}

void CGLTexture::update(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const CRegion& damage) {
    if (damage.empty())
        return;

    g_pHyprRenderer->makeEGLCurrent();

    const auto format = NFormatUtils::getPixelFormatFromDRM(drmFormat);
    ASSERT(format);

    bind();

    if (format->swizzle.has_value())
        swizzle(format->swizzle.value());

    bool alignmentChanged = false;
    if (format->bytesPerBlock != 4) {
        const GLint alignment = (stride % 4 == 0) ? 4 : 1;
        GLCALL(glPixelStorei(GL_UNPACK_ALIGNMENT, alignment));
        alignmentChanged = true;
    }

    GLCALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, stride / format->bytesPerBlock));

    damage.copy().intersect(CBox{{}, m_size}).forEachRect([&format, &pixels](const auto& rect) {
        GLCALL(glPixelStorei(GL_UNPACK_SKIP_PIXELS_EXT, rect.x1));
        GLCALL(glPixelStorei(GL_UNPACK_SKIP_ROWS_EXT, rect.y1));

        int width  = rect.x2 - rect.x1;
        int height = rect.y2 - rect.y1;
        GLCALL(glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x1, rect.y1, width, height, format->glFormat, format->glType, pixels));
    });

    if (alignmentChanged)
        GLCALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 4));

    GLCALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0));
    GLCALL(glPixelStorei(GL_UNPACK_SKIP_PIXELS_EXT, 0));
    GLCALL(glPixelStorei(GL_UNPACK_SKIP_ROWS_EXT, 0));

    unbind();

    if (m_keepDataCopy) {
        m_dataCopy.resize(stride * m_size.y);
        memcpy(m_dataCopy.data(), pixels, stride * m_size.y);
    }
}

void CGLTexture::allocate() {
    if (!m_texID)
        GLCALL(glGenTextures(1, &m_texID));
}

void CGLTexture::bind() {
    GLCALL(glBindTexture(m_target, m_texID));
}

void CGLTexture::unbind() {
    GLCALL(glBindTexture(m_target, 0));
}

bool CGLTexture::ok() {
    return m_texID > 0;
}

bool CGLTexture::isDMA() {
    return m_eglImage;
}

constexpr std::optional<size_t> CGLTexture::getCacheStateIndex(GLenum pname) {
    switch (pname) {
        case GL_TEXTURE_WRAP_S: return TEXTURE_PAR_WRAP_S;
        case GL_TEXTURE_WRAP_T: return TEXTURE_PAR_WRAP_T;
        case GL_TEXTURE_MAG_FILTER: return TEXTURE_PAR_MAG_FILTER;
        case GL_TEXTURE_MIN_FILTER: return TEXTURE_PAR_MIN_FILTER;
        case GL_TEXTURE_SWIZZLE_R: return TEXTURE_PAR_SWIZZLE_R;
        case GL_TEXTURE_SWIZZLE_B: return TEXTURE_PAR_SWIZZLE_B;
        default: return std::nullopt;
    }
}

void CGLTexture::setTexParameter(GLenum pname, GLint param) {
    const auto cacheIndex = getCacheStateIndex(pname);

    if (!cacheIndex) {
        GLCALL(glTexParameteri(m_target, pname, param));
        return;
    }

    const auto idx = cacheIndex.value();

    if (m_cachedStates[idx] == param)
        return;

    m_cachedStates[idx] = param;
    GLCALL(glTexParameteri(m_target, pname, param));
}

void CGLTexture::swizzle(const std::array<GLint, 4>& colors) {
    setTexParameter(GL_TEXTURE_SWIZZLE_R, colors.at(0));
    setTexParameter(GL_TEXTURE_SWIZZLE_G, colors.at(1));
    setTexParameter(GL_TEXTURE_SWIZZLE_B, colors.at(2));
    setTexParameter(GL_TEXTURE_SWIZZLE_A, colors.at(3));
}
