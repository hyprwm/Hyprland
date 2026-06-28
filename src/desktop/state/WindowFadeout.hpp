#pragma once

#include "Fadeout.hpp"

namespace Desktop {
    class CWindowFadeout final : public IFadeout {
      public:
        static SP<CWindowFadeout>     create(PHLWINDOW window, SP<Render::IFramebuffer> snapshot, float sourceAlpha);

        virtual PHLMONITORREF         monitor() const override;
        virtual eFadeoutPlane         plane() const override;
        virtual int                   zIndex() const override;
        virtual CBox                  renderBox() const override;
        virtual float                 alpha() const override;
        virtual bool                  done() const override;
        virtual SFadeoutRenderEffects effects() const override;

      private:
        CWindowFadeout() = default;

        PHLMONITORREF     m_monitor;
        eFadeoutPlane     m_plane  = FADEOUT_PLANE_WINDOW_TILED;
        int               m_zIndex = 0;
        Vector2D          m_sourcePos;
        Vector2D          m_sourceSize;
        PHLANIMVAR<float> m_alpha;

        bool              m_blur          = false;
        bool              m_blurXray      = false;
        int               m_rounding      = 0;
        float             m_roundingPower = 2.F;
    };
}
