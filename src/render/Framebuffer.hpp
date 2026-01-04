#pragma once

#include "../defines.hpp"
#include "../helpers/Format.hpp"
#include "Texture.hpp"

class CFramebuffer {
  public:
    CFramebuffer();
    ~CFramebuffer();

    bool         alloc(int w, int h, uint32_t format = GL_RGBA);
    void         bind();
    void         unbind();
    void         release();
    void         reset();
    bool         isAllocated();
    SP<CTexture> getTexture();
    GLuint       getFBID();

    Vector2D     m_size;
    DRMFormat    m_drmFormat = 0 /* DRM_FORMAT_INVALID */;

  private:
    SP<CTexture> m_tex;
    GLuint       m_fb          = -1;
    bool         m_fbAllocated = false;

    friend class CRenderbuffer;
};
