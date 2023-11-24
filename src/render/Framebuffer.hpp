#pragma once

#include "../defines.hpp"
#include "Texture.hpp"

class CFramebuffer {
  public:
    ~CFramebuffer();

    bool      alloc(int w, int h, uint32_t format = GL_RGBA);
    void      addStencil();
    void      bind();
    void      release();
    void      reset();
    bool      isAllocated();

    Vector2D  m_vSize;

    CTexture  m_cTex;
    GLuint    m_iFb = -1;

    CTexture* m_pStencilTex = nullptr;
};