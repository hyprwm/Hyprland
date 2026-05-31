#pragma once

#include "../../desktop/DesktopTypes.hpp"
#include "../../helpers/MotionBlur.hpp"
#include "../Framebuffer.hpp"
#include "../pass/SurfacePassElement.hpp"

class CEventLoopTimer;

namespace Render {
    struct SWindowTransformContext {
        CBox          currentBox;
        CBox          sourceBox;
        PHLMONITORREF monitor;
        bool          standalone        = false;
        bool          renderingSnapshot = false;
    };

    // A window transformer can be attached to a window.
    // If any is attached, Hyprland will render the window to a separate fb, then call the transform() func with it,
    // and finally render it back to the main fb after all transformers pass.
    //
    // Worth noting transformers for now only affect the main pass (not popups)
    class IWindowTransformer {
      public:
        virtual ~IWindowTransformer() = default;

        // called by Hyprland. For more data about what is being rendered, inspect render data.
        // returns the out fb.
        virtual SP<Render::IFramebuffer> transform(SP<Render::IFramebuffer> in, const SWindowTransformContext& context) = 0;

        virtual int                      priority() const;
        virtual bool                     active() const;
        virtual bool                     blocksDirectScanout() const;
        virtual CBox                     transformedExtents(const CBox& currentBox) const;
        virtual CBox                     sourceBoxForRender(const CBox& currentBox, const CBox& monitorBox) const;
        virtual CBox                     transformBoxForDamage(const CBox& currentBox) const;

        // called by Hyprland before a window main pass is started.
        virtual void preWindowRender(CSurfacePassElement::SRenderData* pRenderData);

        // called by Hyprland before the transformed window fb is rendered back to the main fb.
        virtual void amendTransformedRenderData(const CBox& currentBox, SMotionBlurData* pMotionBlurData);
    };
}
