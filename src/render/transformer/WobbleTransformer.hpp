#pragma once

#include "Transformer.hpp"
#include "../../helpers/DeformableMesh.hpp"

#include <chrono>
#include <optional>
#include <hyprutils/animation/AnimationManager.hpp>

namespace Render {
    class CWobbleTransformer : public IWindowTransformer {
      public:
        CWobbleTransformer(PHLWINDOWREF window);
        virtual ~CWobbleTransformer() = default;

        static bool                      shouldEnable(PHLWINDOW window);
        static void                      ensureTickListener();

        virtual SP<Render::IFramebuffer> transform(SP<Render::IFramebuffer> in, const SWindowTransformContext& context);
        virtual int                      priority() const;
        virtual bool                     active() const;
        virtual bool                     blocksDirectScanout() const;
        virtual CBox                     transformedExtents(const CBox& currentBox) const;
        virtual CBox                     sourceBoxForRender(const CBox& currentBox, const CBox& monitorBox) const;

        void                             record(const CBox& previous, const CBox& current, std::optional<Vector2D> grabPoint = std::nullopt);
        void                             reset();
        void                             resetWithDamage();
        bool                             tick();

      private:
        Hyprutils::Animation::SSpringCurve    spring() const;
        void                                  damageCurrent() const;
        void                                  scheduleFrame() const;

        PHLWINDOWREF                          m_window;
        CDeformableMesh                       m_mesh;
        std::chrono::steady_clock::time_point m_lastTick;
        bool                                  m_active = false;
    };
}
