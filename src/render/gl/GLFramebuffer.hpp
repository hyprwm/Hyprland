#pragma once

#include "../../defines.hpp"
#include "../Texture.hpp"
#include "../Framebuffer.hpp"
#include <drm_fourcc.h>

namespace Render::GL {
    class CGLFramebuffer : public IFramebuffer {
      public:
        CGLFramebuffer();
        CGLFramebuffer(const std::string& name);
        ~CGLFramebuffer();

        void   addStencil(SP<ITexture> tex) override;
        void   release() override;
        bool   readPixels(CHLBufferReference buffer, uint32_t offsetX = 0, uint32_t offsetY = 0, uint32_t width = 0, uint32_t height = 0) override;

        void   bind() override;
        void   unbind();
        GLuint getFBID();
        void   invalidate(const std::vector<GLenum>& attachments);

        // clear at most once per invalidate()
        void clearAfterInvalidation();

      protected:
        bool internalAlloc(int w, int h, DRMFormat format = DRM_FORMAT_ARGB8888) override;

      private:
        GLuint m_fb      = -1;
        bool   m_cleared = false;

        friend class CGLRenderbuffer;
    };
}
