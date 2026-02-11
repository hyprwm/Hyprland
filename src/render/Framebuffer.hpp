#pragma once

#include "../defines.hpp"
#include "../helpers/Format.hpp"
#include "Texture.hpp"
#include <drm_fourcc.h>

class CFramebuffer {
  public:
    CFramebuffer();
    ~CFramebuffer();

    bool         alloc(int w, int h, uint32_t format = DRM_FORMAT_ARGB8888);
    void         addStencil(SP<ITexture> tex);
    void         bind();
    void         unbind();
    void         release();
    void         reset();
    bool         isAllocated();
    SP<ITexture> getTexture();
    SP<ITexture> getStencilTex();
    GLuint       getFBID();
    void         invalidate(const std::vector<GLenum>& attachments);

    Vector2D     m_size;
    DRMFormat    m_drmFormat = 0 /* DRM_FORMAT_INVALID */;

  private:
    SP<ITexture> m_tex;
    GLuint       m_fb          = -1;
    bool         m_fbAllocated = false;

    SP<ITexture> m_stencilTex;

    friend class CRenderbuffer;
};
