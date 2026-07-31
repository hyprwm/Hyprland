#pragma once

#include "Kawase.hpp"

#include "../../../helpers/signal/Signal.hpp"
#include "../../../helpers/time/Time.hpp"

#include <array>
#include <optional>

namespace Render::GL {
    class CRippleBlurMaterial final : public IGLBlurMaterial {
      public:
        CRippleBlurMaterial();

        eBlurType                 type() const noexcept override;
        SBlurMaterialRequirements requirements() const noexcept override;
        bool                      isAnimated() const noexcept override;
        float                     sampleRadius() const override;
        void                      bindFinish(WP<CShader> shader, const SBlurMaterialContext& context) const override;

      private:
        static constexpr size_t MAX_IMPULSES = 256;

        struct SImpulse {
            Vector2D        globalPosition = {};
            Time::steady_tp started        = {};
            PHLMONITORREF   monitor;
            float           damageReach = 0.F;
            bool            occupied    = false;
        };

        void                               damageImpulse(const SImpulse& impulse) const;
        bool                               impulseIsActive(const SImpulse& impulse, PHLMONITORREF monitor, const Time::steady_tp& now, float duration) const;
        void                               addImpulse();

        std::array<SImpulse, MAX_IMPULSES> m_impulses;
        size_t                             m_nextImpulse = 0;
        bool                               m_mouseIsHeld = false;
        std::optional<Vector2D>            m_lastMouseHeldCoord;

        struct {
            CHyprSignalListener mouseButton;
            CHyprSignalListener mouseMotion;
        } m_listeners;
    };

    class CRippleBlurProvider final : public CDualKawaseBlurProvider {
      public:
        explicit CRippleBlurProvider(CHyprOpenGLImpl& impl);
    };

    float rippleDamageRadius(int64_t size, int64_t passes, float displacement);
    float rippleOutputReach(float radius, float width);
}
