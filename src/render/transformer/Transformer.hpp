#pragma once

#include "../../desktop/DesktopTypes.hpp"
#include "../../helpers/MotionBlur.hpp"
#include "../Framebuffer.hpp"
#include "../pass/SurfacePassElement.hpp"

#include <vector>

class CEventLoopTimer;

namespace Render {
    class CRenderingContext;

    CBox pixelBoxForLogical(const CBox& box, double scale);

    struct SWindowTransformBuffer {
        SP<Render::IFramebuffer> framebuffer;
        CBox                     box;
        bool                     success = true;
    };

    struct SWindowTransformStage {
        CBox fullInputBox;
        CBox fullOutputBox;
        CBox inputBox;
        CBox outputBox;
        bool allocatesOutputBuffer = true;
    };

    struct SWindowTransformPlan {
        CBox                               sourceBox;
        CBox                               outputBox;
        std::vector<SWindowTransformStage> stages;
    };

    struct SWindowTransformContext {
        CBox          currentBox;
        CBox          inputBox;
        CBox          outputBox;
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
        // returns the output framebuffer and the monitor-local pixel box represented by it.
        virtual SWindowTransformBuffer transform(CRenderingContext&, const SWindowTransformBuffer& in, const SWindowTransformContext& context) = 0;

        virtual int                    priority() const;
        virtual bool                   active() const;
        virtual bool                   blocksDirectScanout() const;
        virtual bool                   allocatesOutputBuffer() const;
        virtual CBox                   transformedExtents(const CBox& currentBox) const;
        virtual CBox                   sourceBoxForOutput(const CBox& outputBox, const CBox& inputBox) const;
        virtual CBox                   transformBoxForDamage(const CBox& currentBox) const;

        // called by Hyprland before a window main pass is started.
        virtual void preWindowRender(CRenderingContext&, CSurfacePassElement::SRenderData* pRenderData);

        // called by Hyprland before the transformed window fb is rendered back to the main fb.
        virtual void amendTransformedRenderData(CRenderingContext&, const CBox& currentBox, SMotionBlurData* pMotionBlurData);
    };
}
