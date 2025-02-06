#pragma once

#include "../defines.hpp"
#include "Texture.hpp"

class CFramebuffer {
  public:
    CFramebuffer();
    ~CFramebuffer();

    bool         alloc(int w, int h, uint32_t format = GL_RGBA);
    void         addStencil(SP<CTexture> tex);
    void         bind();
    void         unbind();
    void         release();
    void         reset();
    bool         isAllocated();
    SP<CTexture> getTexture();
    SP<CTexture> getStencilTex();
    GLuint       getFBID();

    Vector2D     m_vSize;

  private:
    SP<CTexture> m_cTex;
    GLuint       m_iFb          = -1;
    bool         m_iFbAllocated = false;

    SP<CTexture> m_pStencilTex;

    friend class CRenderbuffer;
};
