#pragma once

#include "Pass.hpp"
#include "TexPassElement.hpp"

namespace Workspace {
    class CWorkspacePresentable;
}

class CTransformedWindowPassElement : public IPassElement {
  public:
    struct SData {
        UP<Render::CRenderPass>              pass;
        PHLWINDOWREF                         window;
        CBox                                 currentBox;
        CBox                                 blurBox;
        bool                                 blur              = false;
        bool                                 blurUsesLive      = false;
        float                                blurA             = 1.F;
        int                                  blurRound         = 0;
        float                                blurRoundingPower = 2.F;
        CBox                                 transformedBox;
        SMotionBlurData                      motionBlur;
        bool                                 standalone        = false;
        bool                                 renderingSnapshot = false;
        SP<Workspace::CWorkspacePresentable> workspacePresentation;
    };

    CTransformedWindowPassElement(SData&& data);
    virtual ~CTransformedWindowPassElement() = default;

    virtual bool                needsLiveBlur(Render::CRenderContext& ctx);
    virtual bool                needsPrecomputeBlur(Render::CRenderContext& ctx);
    virtual std::optional<CBox> boundingBox(Render::CRenderContext& ctx);
    virtual CRegion             opaqueRegion(Render::CRenderContext& ctx);
    virtual bool                disableSimplification(Render::CRenderContext& ctx);

    virtual const char*         passName() {
        return "CTransformedWindowPassElement";
    }

    virtual ePassElementType type() {
        return EK_TRANSFORMED_WINDOW;
    };

    SData m_data;
};
