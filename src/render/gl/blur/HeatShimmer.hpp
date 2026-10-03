#pragma once

#include "Glass.hpp"
#include "BlurAnimationClock.hpp"

#include "../../../helpers/signal/Signal.hpp"

namespace Render::GL {
    class CHeatShimmerBlurMaterial final : public CGlassBlurMaterial {
      public:
        CHeatShimmerBlurMaterial();

        bool isAnimated(CRenderContext& ctx) const noexcept override;
        void bindFinish(CRenderContext& ctx, WP<CShader> shader, const SBlurMaterialContext& context) const override;

      private:
        float                       animationPhase(CRenderContext& ctx) const;

        mutable CBlurAnimationClock m_animationClock;
        CHyprSignalListener         m_configListener;

        friend class CBlurAnimationClockTestAccessor;
    };

    class CHeatShimmerBlurProvider final : public CGlassBlurProvider {
      public:
        explicit CHeatShimmerBlurProvider(CHyprOpenGLImpl& impl);
    };
}
