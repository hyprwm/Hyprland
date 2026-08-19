#pragma once

#include "../../blur/Provider.hpp"

namespace Render::GL {
    class CGLFramebuffer;

    class IGLBlurProvider : public Render::IBlurProvider {
      public:
        SP<IFramebuffer> blur(CRenderingContext& renderingContext, SP<IFramebuffer> source, float strength, const CRegion& originalDamage,
                              const SBlurContext& blurContext = {}) final;

      protected:
        IGLBlurProvider() = default;

        virtual SP<CGLFramebuffer> blurGL(CRenderingContext& renderingContext, SP<CGLFramebuffer> source, float strength, const CRegion& originalDamage,
                                          const SBlurContext& blurContext) = 0;
    };
}
