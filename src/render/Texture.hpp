#pragma once

#include "../defines.hpp"

class IWLBuffer;
struct SDMABUFAttrs;
HYPRUTILS_FORWARD(Math, CRegion);

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
    CTexture(wlr_texture*);

    // this ctor takes ownership of the eglImage.
    CTexture(const SDMABUFAttrs&, void* image);
    ~CTexture();

    void        destroyTexture();
    void        allocate();
    void        update(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const CRegion& damage);

    TEXTURETYPE m_iType   = TEXTURE_RGBA;
    GLenum      m_iTarget = GL_TEXTURE_2D;
    GLuint      m_iTexID  = 0;
    Vector2D    m_vSize;
    void*       m_pEglImage  = nullptr;
    bool        m_bNonOwning = false; // wlr
};