#pragma once
#include "../../helpers/CornerRadii.hpp"
#include "PassElement.hpp"
#include "../../config/shared/complex/ComplexDataTypes.hpp"

class CBorderPassElement : public IPassElement {
  public:
    struct SBorderData {
        CBox                       box;
        Config::CGradientValueData grad1, grad2;
        bool                       hasGrad2 = false;
        float                      lerp = 0.F, a = 1.F;
        SCornerRadii               round = 0, outerRound = -1;
        int                        borderSize    = 1;
        float                      roundingPower = 2.F;
        PHLWINDOWREF               window;
    };

    CBorderPassElement(const SBorderData& data_);
    virtual ~CBorderPassElement() = default;

    virtual bool        needsLiveBlur(Render::CRenderContext& ctx);
    virtual bool        needsPrecomputeBlur(Render::CRenderContext& ctx);

    virtual const char* passName() {
        return "CBorderPassElement";
    }

    virtual ePassElementType type() {
        return EK_BORDER;
    };

    SBorderData m_data;
};
