#pragma once

#include "../../blur/Provider.hpp"

namespace Render::GL {
    class CGLFramebuffer;

    class IGLBlurProvider : public Render::IBlurProvider {
      public:
        SP<IFramebuffer> blur(CRenderContext& ctx, SP<IFramebuffer> source, float strength, const CRegion& originalDamage, const SBlurContext& context = {}) final;

      protected:
        IGLBlurProvider() = default;

        virtual SP<CGLFramebuffer> blurGL(CRenderContext& ctx, SP<CGLFramebuffer> source, float strength, const CRegion& originalDamage, const SBlurContext& context) = 0;
    };
}
