#pragma once

#include "../defines.hpp"
#include <aquamarine/buffer/Buffer.hpp>
#include <hyprutils/math/Misc.hpp>

class IHLBuffer;
HYPRUTILS_FORWARD(Math, CRegion);
using namespace Hyprutils::Math;

enum TEXTURETYPE {
    TEXTURE_INVALID,  // Invalid
    TEXTURE_RGBA,     // 4 channels
    TEXTURE_RGBX,     // discard A
    TEXTURE_EXTERNAL, // EGLImage
};

class CTexture {
  public:
    CTexture();

    CTexture(CTexture&)        = delete;
    CTexture(CTexture&&)       = delete;
    CTexture(const CTexture&&) = delete;
    CTexture(const CTexture&)  = delete;

    CTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size);

    CTexture(const SP<Aquamarine::IBuffer> buffer);
    // this ctor takes ownership of the eglImage.
    CTexture(const Aquamarine::SDMABUFAttrs&, void* image);
    ~CTexture();

    void        destroyTexture();
    void        allocate();
    void        update(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const CRegion& damage);

    TEXTURETYPE m_iType      = TEXTURE_RGBA;
    GLenum      m_iTarget    = GL_TEXTURE_2D;
    GLuint      m_iTexID     = 0;
    Vector2D    m_vSize      = {};
    void*       m_pEglImage  = nullptr;
    eTransform  m_eTransform = HYPRUTILS_TRANSFORM_NORMAL;
    bool        m_bOpaque    = false;

  private:
    void createFromShm(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size);
    void createFromDma(const Aquamarine::SDMABUFAttrs&, void* image);
};