#pragma once

#include "../defines.hpp"
#include "Texture.hpp"

class CFramebuffer {
  public:
    ~CFramebuffer();

    bool      alloc(int w, int h);
    void      bind();
    void      release();
    void      reset();
    bool      isAllocated();

    Vector2D  m_vSize;

    CTexture  m_cTex;
    GLuint    m_iFb = -1;

    CTexture* m_pStencilTex = nullptr;
};