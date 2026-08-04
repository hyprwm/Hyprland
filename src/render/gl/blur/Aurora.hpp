#pragma once

#include "Glass.hpp"

#include "../../../helpers/signal/Signal.hpp"
#include "../../../helpers/time/Time.hpp"

namespace Render::GL {
    class CAuroraBlurMaterial final : public CGlassBlurMaterial {
      public:
        CAuroraBlurMaterial();

        bool isAnimated() const noexcept override;
        void bindFinish(WP<CShader> shader, const SBlurMaterialContext& context) const override;

      private:
        void                    updateAnimation(float speed) const;
        float                   animationPhase() const;

        mutable Time::steady_tp m_lastAnimationUpdate;
        mutable double          m_animationTime = 0.0;
        mutable float           m_previousSpeed = 0.F;
        CHyprSignalListener     m_configListener;
    };

    class CAuroraBlurProvider final : public CGlassBlurProvider {
      public:
        explicit CAuroraBlurProvider(CHyprOpenGLImpl& impl);
    };
}
