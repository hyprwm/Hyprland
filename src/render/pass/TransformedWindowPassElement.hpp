#pragma once

#include "Pass.hpp"
#include "TexPassElement.hpp"

class CTransformedWindowPassElement : public IPassElement {
  public:
    struct SData {
        UP<Render::CRenderPass> pass;
        PHLWINDOWREF            window;
        CBox                    currentBox;
        CBox                    blurBox;
        bool                    blur              = false;
        bool                    blurUsesLive      = false;
        float                   blurA             = 1.F;
        int                     blurRound         = 0;
        float                   blurRoundingPower = 2.F;
        CBox                    transformedBox;
        SMotionBlurData         motionBlur;
        bool                    standalone        = false;
        bool                    renderingSnapshot = false;
    };

    CTransformedWindowPassElement(SData&& data);
    virtual ~CTransformedWindowPassElement() = default;

    virtual bool                needsLiveBlur(const Render::CRenderingContext&);
    virtual bool                needsPrecomputeBlur(const Render::CRenderingContext&);
    virtual std::optional<CBox> boundingBox(const Render::CRenderingContext& context);
    virtual CRegion             opaqueRegion(const Render::CRenderingContext& context);
    virtual bool                disableSimplification();

    virtual const char*         passName() {
        return "CTransformedWindowPassElement";
    }

    virtual ePassElementType type() {
        return EK_TRANSFORMED_WINDOW;
    };

    SData m_data;
};
