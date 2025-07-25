#include "Texture.hpp"
#include "Renderer.hpp"
#include "../Compositor.hpp"
#include "../protocols/types/Buffer.hpp"
#include "../helpers/Format.hpp"
#include <cstring>

CTexture::CTexture() = default;

CTexture::~CTexture() {
    if (!g_pCompositor || g_pCompositor->m_isShuttingDown || !g_pHyprRenderer)
        return;

    g_pHyprRenderer->makeEGLCurrent();
    destroyTexture();
}

CTexture::CTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size_, bool keepDataCopy) : m_drmFormat(drmFormat), m_keepDataCopy(keepDataCopy) {
    createFromShm(drmFormat, pixels, stride, size_);
}

CTexture::CTexture(const Aquamarine::SDMABUFAttrs& attrs, void* image) {
    createFromDma(attrs, image);
}

CTexture::CTexture(const SP<Aquamarine::IBuffer> buffer, bool keepDataCopy) : m_keepDataCopy(keepDataCopy) {
    if (!buffer)
        return;

    m_opaque = buffer->opaque;

    auto attrs = buffer->dmabuf();

    if (!attrs.success) {
        // attempt shm
        auto shm = buffer->shm();

        if (!shm.success) {
            Debug::log(ERR, "Cannot create a texture: buffer has no dmabuf or shm");
            return;
        }

        auto [pixelData, fmt, bufLen] = buffer->beginDataPtr(0);

        m_drmFormat = fmt;

        createFromShm(fmt, pixelData, bufLen, shm.size);
        return;
    }

    auto image = g_pHyprOpenGL->createEGLImage(buffer->dmabuf());

    if (!image) {
        Debug::log(ERR, "Cannot create a texture: failed to create an EGLImage");
        return;
    }

    createFromDma(attrs, image);
}

void CTexture::createFromShm(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size_) {
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

    if (format->flipRB) {
        setTexParameter(GL_TEXTURE_SWIZZLE_R, GL_BLUE);
        setTexParameter(GL_TEXTURE_SWIZZLE_B, GL_RED);
    }

    GLCALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, stride / format->bytesPerBlock));
    GLCALL(glTexImage2D(GL_TEXTURE_2D, 0, format->glInternalFormat ? format->glInternalFormat : format->glFormat, size_.x, size_.y, 0, format->glFormat, format->glType, pixels));
    GLCALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0));
    unbind();

    if (m_keepDataCopy) {
        m_dataCopy.resize(stride * size_.y);
        memcpy(m_dataCopy.data(), pixels, stride * size_.y);
    }
}

void CTexture::createFromDma(const Aquamarine::SDMABUFAttrs& attrs, void* image) {
    if (!g_pHyprOpenGL->m_proc.glEGLImageTargetTexture2DOES) {
        Debug::log(ERR, "Cannot create a dmabuf texture: no glEGLImageTargetTexture2DOES");
        return;
    }

    m_opaque = NFormatUtils::isFormatOpaque(attrs.format);
    m_target = GL_TEXTURE_2D;
    m_type   = TEXTURE_RGBA;
    m_size   = attrs.size;
    m_type   = NFormatUtils::isFormatOpaque(attrs.format) ? TEXTURE_RGBX : TEXTURE_RGBA;
    allocate();
    m_eglImage = image;

    bind();
    setTexParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    setTexParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLCALL(g_pHyprOpenGL->m_proc.glEGLImageTargetTexture2DOES(m_target, image));
    unbind();
}

void CTexture::update(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const CRegion& damage) {
    g_pHyprRenderer->makeEGLCurrent();
    const auto format = NFormatUtils::getPixelFormatFromDRM(drmFormat);
    ASSERT(format);

    bind();

    if (format->flipRB) {
        setTexParameter(GL_TEXTURE_SWIZZLE_R, GL_BLUE);
        setTexParameter(GL_TEXTURE_SWIZZLE_B, GL_RED);
    }

    auto             clippedDamage = damage.copy().intersect(CBox{{}, m_size});
    const auto       rects         = clippedDamage.getRects();
    const auto       pixelSize     = format->bytesPerBlock;

    constexpr size_t MAX_RECTS_BEFORE_PACKING = 2;
    const auto       usePackedUpload          = rects.size() > MAX_RECTS_BEFORE_PACKING;

    if (usePackedUpload) {
        const auto extents = clippedDamage.getExtents();
        const auto xMin    = static_cast<int32_t>(extents.x);
        const auto yMin    = static_cast<int32_t>(extents.y);
        const auto width   = static_cast<int32_t>(extents.width);
        const auto height  = static_cast<int32_t>(extents.height);

        if (width <= 0 || height <= 0) {
            unbind();
            return;
        }

        const auto           rowPitch = static_cast<size_t>(width) * pixelSize;
        std::vector<uint8_t> packedBuffer(static_cast<size_t>(height) * rowPitch);

        for (int32_t row = 0; row < height; ++row) {
            const auto* src = pixels + (static_cast<size_t>(yMin + row) * stride) + (static_cast<size_t>(xMin) * pixelSize);
            auto*       dst = packedBuffer.data() + (static_cast<size_t>(row) * rowPitch);

            std::memcpy(dst, src, rowPitch);
        }

        GLCALL(glTexSubImage2D(GL_TEXTURE_2D, 0, xMin, yMin, width, height, format->glFormat, format->glType, packedBuffer.data()));
    } else {
        for (const auto& rect : rects) {
            const auto x      = rect.x1;
            const auto y      = rect.y1;
            const auto width  = rect.x2 - rect.x1;
            const auto height = rect.y2 - rect.y1;

            if (width <= 0 || height <= 0)
                continue;

            GLCALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, stride / pixelSize));
            GLCALL(glPixelStorei(GL_UNPACK_SKIP_PIXELS_EXT, x));
            GLCALL(glPixelStorei(GL_UNPACK_SKIP_ROWS_EXT, y));

            GLCALL(glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, format->glFormat, format->glType, pixels));
        }

        GLCALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0));
        GLCALL(glPixelStorei(GL_UNPACK_SKIP_PIXELS_EXT, 0));
        GLCALL(glPixelStorei(GL_UNPACK_SKIP_ROWS_EXT, 0));
    }

    unbind();

    if (m_keepDataCopy) {
        const size_t fullSize = static_cast<size_t>(stride) * m_size.y;
        m_dataCopy.resize(fullSize);
        std::memcpy(m_dataCopy.data(), pixels, fullSize);
    }
}

void CTexture::destroyTexture() {
    if (m_texID) {
        GLCALL(glDeleteTextures(1, &m_texID));
        m_texID = 0;
    }

    if (m_eglImage)
        g_pHyprOpenGL->m_proc.eglDestroyImageKHR(g_pHyprOpenGL->m_eglDisplay, m_eglImage);
    m_eglImage = nullptr;
    m_cachedStates.fill(std::nullopt);
}

void CTexture::allocate() {
    if (!m_texID)
        GLCALL(glGenTextures(1, &m_texID));
}

const std::vector<uint8_t>& CTexture::dataCopy() {
    return m_dataCopy;
}

void CTexture::bind() {
    GLCALL(glBindTexture(m_target, m_texID));
}

void CTexture::unbind() {
    GLCALL(glBindTexture(m_target, 0));
}

constexpr std::optional<size_t> CTexture::getCacheStateIndex(GLenum pname) {
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

void CTexture::setTexParameter(GLenum pname, GLint param) {
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
