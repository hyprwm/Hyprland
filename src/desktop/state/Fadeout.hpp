#pragma once

#include "../DesktopTypes.hpp"
#include "../view/types/GeometricAnimated.hpp"
#include "../../helpers/AnimatedVariable.hpp"
#include "../../helpers/time/Time.hpp"

#include <optional>

namespace Render {
    class IFramebuffer;
    enum class eSceneMode : uint8_t;
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
        // Workspace scene placement only; snapshots retain their monitor plane.
        FADEOUT_PLANE_WINDOW_PINNED,
    };

    enum class eFadeoutSource : uint8_t {
        UNKNOWN,
        WINDOW,
        LAYER,
    };

    // Snapshot origin, independent of the legacy monitor workspace filter.
    struct SFadeoutSource {
        eFadeoutSource  type = eFadeoutSource::UNKNOWN;
        PHLWORKSPACEREF workspace;
        bool            pinned        = false; // Floating, output-global window (including its popups).
        bool            noScreenShare = false;
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
        SP<Render::IFramebuffer>      framebuffer(Render::eSceneMode mode) const;
        virtual CBox                  renderBox() const = 0;
        virtual float                 alpha() const     = 0;
        virtual bool                  done() const      = 0;
        virtual SFadeoutRenderEffects effects() const;
        virtual SFadeoutSource        source() const;

      protected:
        IFadeout() = default;

        SP<Render::IFramebuffer> m_framebuffer;
        // Neutral window pixels only; never a fallback for the monitor snapshot.
        SP<Render::IFramebuffer> m_workspaceFramebuffer;
        PHLWORKSPACEREF          m_workspace;
        SFadeoutRenderEffects    m_effects;
        SFadeoutSource           m_source;
    };
}
