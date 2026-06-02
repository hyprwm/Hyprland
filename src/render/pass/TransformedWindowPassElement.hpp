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
        float                   blurA             = 1.F;
        int                     blurRound         = 0;
        float                   blurRoundingPower = 2.F;
        SMotionBlurData         motionBlur;
    };

    CTransformedWindowPassElement(SData&& data);
    virtual ~CTransformedWindowPassElement() = default;

    virtual bool                needsLiveBlur();
    virtual bool                needsPrecomputeBlur();
    virtual std::optional<CBox> boundingBox();
    virtual CRegion             opaqueRegion();
    virtual bool                disableSimplification();

    virtual const char*         passName() {
        return "CTransformedWindowPassElement";
    }

    virtual ePassElementType type() {
        return EK_TRANSFORMED_WINDOW;
    };

    SData m_data;
};
