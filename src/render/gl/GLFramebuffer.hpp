#pragma once

#include "../../defines.hpp"
#include "../Texture.hpp"
#include "../Framebuffer.hpp"
#include <drm_fourcc.h>

class CGLFramebuffer : IFramebuffer {
  public:
    CGLFramebuffer();
    ~CGLFramebuffer();

    void   addStencil(SP<ITexture> tex) override;
    void   release() override;

    void   bind();
    void   unbind();
    GLuint getFBID();
    void   invalidate(const std::vector<GLenum>& attachments);

  private:
    bool   internalAlloc(int w, int h, uint32_t format = DRM_FORMAT_ARGB8888) override;

    GLuint m_fb = -1;

    friend class CRenderbuffer;
};
