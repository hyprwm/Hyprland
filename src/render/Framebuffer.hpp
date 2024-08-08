#pragma once

#include "../defines.hpp"
#include "Texture.hpp"

class CFramebuffer {
  public:
    CFramebuffer();
    ~CFramebuffer();

    bool         alloc(int w, int h, uint32_t format = GL_RGBA);
    void         addStencil();
    void         bind();
    void         release();
    void         reset();
    bool         isAllocated();

    Vector2D     m_vSize;

    SP<CTexture> m_cTex;
    GLuint       m_iFb;
    bool         m_iFbAllocated{false};

    SP<CTexture> m_pStencilTex;
};