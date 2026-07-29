#pragma once

#include "Kawase.hpp"

#include "../../../helpers/signal/Signal.hpp"
#include "../../../helpers/time/Time.hpp"

#include <vector>

namespace Render::GL {
    class CGLFramebuffer;

    class CWaterBlurProvider final : public CDualKawaseBlurProvider {
      public:
        explicit CWaterBlurProvider(CHyprOpenGLImpl& impl);

        eBlurType type() const noexcept override;
        bool      isAnimated() const noexcept override;

      protected:
        ePreparedFragmentShader finishFragment() const noexcept override;
        void                    updateProviderState(const SBlurContext& context, const CRegion& outputDamage) override;
        void                    setFinishUniforms(WP<CShader> shader, float strength, const SBlurContext& context) const override;
        float                   damageRadius() const override;

      private:
        static constexpr size_t MAX_IMPULSES = 16;

        struct SImpulse {
            Vector2D position  = {};
            float    radius    = 0.F;
            float    amplitude = 0.F;
        };

        struct SState {
            PHLWINDOWREF          window;
            PHLMONITORREF         monitor;
            SP<CGLFramebuffer>    buffers[2];
            Vector2D              simulationSize = {};
            std::vector<SImpulse> impulses;
            Time::steady_tp       lastUpdate    = {};
            Time::steady_tp       activeUntil   = {};
            uint64_t              lastFrame     = 0;
            uint8_t               currentBuffer = 0;
            bool                  reset         = true;
        };

        SState*                     stateForContext(const SBlurContext& context, bool create);
        const SState*               stateForContext(const SBlurContext& context) const;
        SState*                     windowState(PHLWINDOWREF window, bool create);
        SState*                     monitorState(PHLMONITORREF monitor, bool create);
        void                        addImpulse();
        void                        queueImpulse(SState& state, Vector2D position, float radius, float amplitude);
        void                        updateState(SState& state, const CBox& extent);
        void                        resetState(SState& state, const Vector2D& simulationSize);
        void                        drawStateStep(SState& state, float dt, const CBox& extent);
        CBox                        transformedPatternBox(const SBlurContext& context) const;
        bool                        stateIsActive(const SState& state, const Time::steady_tp& now) const;
        void                        pruneStates() const;

        CHyprOpenGLImpl&            m_impl;
        mutable std::vector<SState> m_windowStates;
        mutable std::vector<SState> m_monitorStates;
        uint64_t                    m_frame     = 0;
        bool                        m_mouseHeld = false;
        std::optional<Vector2D>     m_lastMouseHeldCoord;

        struct {
            CHyprSignalListener mouseButton;
            CHyprSignalListener mouseMotion;
            CHyprSignalListener renderPre;
            CHyprSignalListener windowDestroy;
            CHyprSignalListener config;
        } m_listeners;
    };

    float waterDamageRadius(int64_t size, int64_t passes, float displacement);
}
