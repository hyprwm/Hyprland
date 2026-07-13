#pragma once

#include "../DesktopTypes.hpp"
#include "../view/types/GeometricAnimated.hpp"
#include "../../helpers/AnimatedVariable.hpp"
#include "../../helpers/time/Time.hpp"

#include <optional>

namespace Render {
    class IFramebuffer;
}

namespace Desktop {
    enum eFadeoutPlane : uint8_t {
        FADEOUT_PLANE_LAYER_BACKGROUND = 0,
        FADEOUT_PLANE_LAYER_BOTTOM,
        FADEOUT_PLANE_WINDOW_TILED,
        FADEOUT_PLANE_WINDOW_FLOATING,
        FADEOUT_PLANE_WINDOW_OVER_FULLSCREEN,
        FADEOUT_PLANE_LAYER_TOP,
        FADEOUT_PLANE_LAYER_OVERLAY,
        FADEOUT_PLANE_POPUP,
    };

    struct SFadeoutPreBlur {
        CBox  box;
        int   round         = 0;
        float roundingPower = 2.F;
        bool  xray          = false;
        float alpha         = 1.F;
    };

    struct SFadeoutTextureBlur {
        bool                 enabled    = false;
        float                alpha      = 1.F;
        bool                 forceBlend = false;
        std::optional<float> ignoreAlpha;
        std::optional<bool>  blockBlurOptimization;
    };

    struct SFadeoutRenderEffects {
        float                          dimAroundAlpha = 0.F;
        std::optional<SFadeoutPreBlur> preBlur;
        SFadeoutTextureBlur            textureBlur;
    };

    class IFadeout : public virtual View::CGeometricAnimated {
      public:
        virtual ~IFadeout() = default;

        virtual PHLMONITORREF         monitor() const = 0;
        PHLWORKSPACEREF               workspace() const;
        virtual eFadeoutPlane         plane() const  = 0;
        virtual int                   zIndex() const = 0;
        SP<Render::IFramebuffer>      framebuffer() const;
        virtual CBox                  renderBox() const = 0;
        virtual float                 alpha() const     = 0;
        virtual bool                  done() const      = 0;
        virtual SFadeoutRenderEffects effects() const;

      protected:
        IFadeout() = default;

        SP<Render::IFramebuffer> m_framebuffer;
        PHLWORKSPACEREF          m_workspace;
        SFadeoutRenderEffects    m_effects;
    };
}
