#pragma once
#include "PassElement.hpp"
#include <hyprutils/math/Region.hpp>
#include <optional>

namespace Workspace {
    class CWorkspacePresentable;
}

class CRectPassElement : public IPassElement {
  public:
    struct SRectData {
        CBox                                 box;
        CHyprColor                           color;
        int                                  round         = 0;
        float                                roundingPower = 2.0f;
        bool                                 blur = false, xray = false;
        float                                blurA = 1.F;
        std::optional<CBox>                  blurPatternBox;
        PHLWINDOWREF                         blurOwner;
        CBox                                 clipBox;
        SP<Workspace::CWorkspacePresentable> workspacePresentation;

        // internal
        CBox    modifiedBox;
        float   TOPLEFT[2];
        float   FULLSIZE[2];
        CRegion drawRegion;
    };

    CRectPassElement(const SRectData& data);
    virtual ~CRectPassElement() = default;

    virtual bool                needsLiveBlur(Render::CRenderContext& ctx);
    virtual bool                needsPrecomputeBlur(Render::CRenderContext& ctx);
    virtual std::optional<CBox> boundingBox(Render::CRenderContext& ctx);
    virtual CRegion             opaqueRegion(Render::CRenderContext& ctx);

    virtual const char*         passName() {
        return "CRectPassElement";
    }

    virtual ePassElementType type() {
        return EK_RECT;
    };

    SRectData m_data;
};
