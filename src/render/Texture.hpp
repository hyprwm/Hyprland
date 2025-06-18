#pragma once

#include "../defines.hpp"
#include <aquamarine/buffer/Buffer.hpp>
#include <hyprutils/math/Misc.hpp>

class IHLBuffer;
HYPRUTILS_FORWARD(Math, CRegion);

enum eTextureType : int8_t {
    TEXTURE_INVALID = -1, // Invalid
    TEXTURE_RGBA    = 0,  // 4 channels
    TEXTURE_RGBX,         // discard A
    TEXTURE_EXTERNAL,     // EGLImage
};

enum eTextureParam : int {
    TEXTURE_WRAP_S     = GL_TEXTURE_WRAP_S,
    TEXTURE_WRAP_T     = GL_TEXTURE_WRAP_T,
    TEXTURE_MAG_FILTER = GL_TEXTURE_MAG_FILTER,
    TEXTURE_MIN_FILTER = GL_TEXTURE_MIN_FILTER,
    TEXTURE_SWIZZLE_R  = GL_TEXTURE_SWIZZLE_R,
    TEXTURE_SWIZZLE_B  = GL_TEXTURE_SWIZZLE_B,

    TEXTURE_LAST,
};

class CTexture {
  public:
    CTexture();

    CTexture(CTexture&)        = delete;
    CTexture(CTexture&&)       = delete;
    CTexture(const CTexture&&) = delete;
    CTexture(const CTexture&)  = delete;

    CTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy = false);

    CTexture(const SP<Aquamarine::IBuffer> buffer, bool keepDataCopy = false);
    // this ctor takes ownership of the eglImage.
    CTexture(const Aquamarine::SDMABUFAttrs&, void* image);
    ~CTexture();

    void                        destroyTexture();
    void                        allocate();
    void                        update(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const CRegion& damage);
    const std::vector<uint8_t>& dataCopy();
    void                        bind();
    void                        unbind();
    void                        setTexParameter(GLenum pname, GLint param);

    eTextureType                m_type          = TEXTURE_RGBA;
    GLenum                      m_target        = GL_TEXTURE_2D;
    GLuint                      m_texID         = 0;
    Vector2D                    m_size          = {};
    void*                       m_eglImage      = nullptr;
    eTransform                  m_transform     = HYPRUTILS_TRANSFORM_NORMAL;
    bool                        m_opaque        = false;
    uint32_t                    m_drmFormat     = 0; // for shm
    bool                        m_isSynchronous = false;

  private:
    void                            createFromShm(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size);
    void                            createFromDma(const Aquamarine::SDMABUFAttrs&, void* image);

    bool                            m_keepDataCopy = false;

    std::vector<uint8_t>            m_dataCopy;
    std::array<GLint, TEXTURE_LAST> textureState;
};
