#pragma once

#include "../../defines.hpp"
#include "../Texture.hpp"
#include "../Framebuffer.hpp"
#include <drm_fourcc.h>

class CGLFramebuffer : public IFramebuffer {
  public:
    CGLFramebuffer();
    ~CGLFramebuffer();

    void   addStencil(SP<ITexture> tex) override;
    void   release() override;
    bool   readPixels(CHLBufferReference buffer, uint32_t offsetX = 0, uint32_t offsetY = 0) override;

    void   bind();
    void   unbind();
    GLuint getFBID();
    void   invalidate(const std::vector<GLenum>& attachments);

  protected:
    bool internalAlloc(int w, int h, uint32_t format = DRM_FORMAT_ARGB8888) override;

  private:
    GLuint m_fb = -1;

    friend class CGLRenderbuffer;
};
