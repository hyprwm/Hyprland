#pragma once

#include "Kawase.hpp"

#include "../../../helpers/signal/Signal.hpp"
#include "../../../helpers/time/Time.hpp"

#include <array>
#include <limits>
#include <vector>

namespace Render::GL {
    class CGLFramebuffer;

    struct SFluidJarGeometryTransform {
        Vector2D positionScale  = {1, 1};
        Vector2D positionOffset = {};
        Vector2D velocityScale  = {1, 1};
    };

    class CFluidJarBlurProvider final : public CDualKawaseBlurProvider {
      public:
        explicit CFluidJarBlurProvider(CHyprOpenGLImpl& impl);

        eBlurType type() const noexcept override;
        bool      isAnimated() const noexcept override;
        bool      requiresLiveBlur() const noexcept override;

      protected:
        ePreparedFragmentShader finishFragment() const noexcept override;
        bool                    requiresPreparedInput() const noexcept override;
        void                    updateProviderState(const SBlurContext& context, const CRegion& outputDamage) override;
        void                    setFinishUniforms(WP<CShader> shader, float strength, const SBlurContext& context) const override;
        float                   damageRadius() const override;

      private:
        struct SState {
            PHLWINDOWREF                      window;
            std::array<SP<CGLFramebuffer>, 2> particles;
            std::array<SP<CGLFramebuffer>, 2> graph;
            std::array<SP<CGLFramebuffer>, 2> tracking;
            std::array<SP<CGLFramebuffer>, 2> visual;
            Vector2D                          simulationSize      = {};
            Vector2D                          particleTextureSize = {};
            Vector2D                          graphTextureSize    = {};
            Vector2D                          gridSize            = {};
            CBox                              extent              = {};
            Time::steady_tp                   lastUpdate          = {};
            std::array<float, 3>              wallVelocities      = {};
            double                            accumulator         = 0.0;
            double                            animationTime       = 0.0;
            float                             fillAmount          = 0.F;
            float                             precision           = 1.F;
            int                               particleCount       = 0;
            uint64_t                          simulationFrame     = 0;
            uint64_t                          lastFrame           = std::numeric_limits<uint64_t>::max();
            uint8_t                           currentParticles    = 0;
            uint8_t                           currentGraph        = 0;
            uint8_t                           currentTracking     = 0;
            uint8_t                           currentVisual       = 0;
            bool                              hasExtent           = false;
        };

        SState*       stateForContext(const SBlurContext& context, bool create);
        const SState* stateForContext(const SBlurContext& context) const;
        void          updateState(SState& state, const CBox& extent);
        void          initializeState(SState& state, const Vector2D& simulationSize, float fillAmount, float precision);
        void          transformState(SState& state, const Vector2D& simulationSize, const SFluidJarGeometryTransform& transform, const std::array<float, 3>& wallVelocities);
        void          allocateBuffers(std::array<SP<CGLFramebuffer>, 2>& buffers, const Vector2D& size, const std::string& name, DRMFormat format = DRM_FORMAT_ABGR16161616F) const;
        void          clearBuffers(const std::array<SP<CGLFramebuffer>, 2>& buffers, const std::array<float, 4>& color) const;
        void          clearIntegerBuffers(const std::array<SP<CGLFramebuffer>, 2>& buffers) const;
        void          drawInitialize(const SState& state, SP<CGLFramebuffer> target) const;
        void          drawResample(const SState& state, SP<CGLFramebuffer> source, SP<CGLFramebuffer> target, const Vector2D& oldGridSize, int oldParticleCount,
                                   const SFluidJarGeometryTransform& transform, const std::array<float, 3>& wallVelocities) const;
        void          drawHistoryResample(SP<CGLFramebuffer> source, SP<CGLFramebuffer> target, const Vector2D& oldSize, const SFluidJarGeometryTransform& transform,
                                          const std::array<float, 4>& fallback, bool linear) const;
        void          drawParticleStep(SState& state, float dt) const;
        void          drawGraphStep(SState& state) const;
        void          drawTrackingStep(SState& state) const;
        void          drawTrackingResample(SP<CGLFramebuffer> source, SP<CGLFramebuffer> target, const Vector2D& oldSize, const SFluidJarGeometryTransform& transform) const;
        void          drawVisualStep(SState& state, int steps = 1) const;
        void          preparePass(SP<CGLFramebuffer> target, const Vector2D& size, WP<CShader> shader) const;
        CBox          transformedPatternBox(const SBlurContext& context) const;
        void          scheduleNextFrame(const SState& state) const;
        void          pruneStates();

        CHyprOpenGLImpl&    m_impl;
        std::vector<SState> m_states;
        uint64_t            m_frame     = 0;
        bool                m_supported = false;

        struct {
            CHyprSignalListener renderPre;
            CHyprSignalListener windowDestroy;
        } m_listeners;
    };

    Vector2D                   fluidJarSimulationSize(const Vector2D& extent, float precision = 1.F);
    Vector2D                   fluidJarGridSize(const Vector2D& simulationSize);
    int                        fluidJarParticleCapacity(const Vector2D& simulationSize);
    int                        fluidJarInitialParticleCount(const Vector2D& simulationSize, float fillAmount);
    int                        fluidJarResizedParticleCount(int oldParticleCount, const Vector2D& simulationSize);
    float                      fluidJarDamageRadius(int64_t size, int64_t passes, float distortion = 1.F);
    SFluidJarGeometryTransform fluidJarGeometryTransform(const CBox& oldExtent, const CBox& newExtent, const Vector2D& oldSimulationSize, const Vector2D& newSimulationSize,
                                                         bool preserveWorldPosition = true);
    std::array<float, 3>       fluidJarWallVelocities(const CBox& oldExtent, const CBox& newExtent, const Vector2D& simulationSize, float elapsed, float speed = 1.F);
}
