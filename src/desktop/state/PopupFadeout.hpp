#pragma once

#include "Fadeout.hpp"

namespace Desktop {
    class CPopupFadeout final : public IFadeout {
      public:
        static SP<CPopupFadeout>      create(SP<View::CPopup> popup, SP<Render::IFramebuffer> snapshot, float sourceAlpha);

        virtual PHLMONITORREF         monitor() const override;
        virtual eFadeoutPlane         plane() const override;
        virtual int                   zIndex() const override;
        virtual CBox                  renderBox() const override;
        virtual float                 alpha() const override;
        virtual bool                  done() const override;
        virtual SFadeoutRenderEffects effects() const override;

      private:
        CPopupFadeout() = default;

        PHLMONITORREF     m_monitor;
        int               m_zIndex = 0;
        PHLANIMVAR<float> m_alpha;
    };
}
