#pragma once
#include "PassElement.hpp"
#include "../../config/ConfigDataValues.hpp"

class CGradientValueData;

class CBorderPassElement : public IPassElement {
  public:
    struct SBorderData {
        CBox               box;
        CGradientValueData grad1, grad2;
        bool               hasGrad2 = false;
        float              lerp = 0.F, a = 1.F;
        int                round = 0, borderSize = 1, outerRound = -1;
        float              roundingPower = 2.F;
        PHLWINDOWREF       window;
    };

    CBorderPassElement(const SBorderData& data_);
    virtual ~CBorderPassElement() = default;

    virtual bool        needsLiveBlur();
    virtual bool        needsPrecomputeBlur();

    virtual const char* passName() {
        return "CBorderPassElement";
    }

    virtual ePassElementKind kind() {
        return EK_BORDER;
    };

    SBorderData m_data;
};
