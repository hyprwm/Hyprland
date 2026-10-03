#pragma once

#include "Glass.hpp"
#include "BlurAnimationClock.hpp"

#include "../../../helpers/signal/Signal.hpp"

namespace Render::GL {
    class CAuroraBlurMaterial final : public CGlassBlurMaterial {
      public:
        CAuroraBlurMaterial();

        bool isAnimated(CRenderContext& ctx) const noexcept override;
        void bindFinish(CRenderContext& ctx, WP<CShader> shader, const SBlurMaterialContext& context) const override;

      private:
        float                       animationPhase(CRenderContext& ctx) const;

        mutable CBlurAnimationClock m_animationClock;
        CHyprSignalListener         m_configListener;

        friend class CBlurAnimationClockTestAccessor;
    };

    class CAuroraBlurProvider final : public CGlassBlurProvider {
      public:
        explicit CAuroraBlurProvider(CHyprOpenGLImpl& impl);
    };
}
