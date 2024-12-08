#pragma once
#include "PassElement.hpp"

class CBorderPassElement : public IPassElement {
  public:
    struct SBorderData {
        CBox               box;
        CGradientValueData grad1, grad2;
        bool               hasGrad2 = false;
        float              lerp = 0.F, a = 1.F;
        int                round = 0, borderSize = 1, outerRound = -1;
    };

    CBorderPassElement(const SBorderData& data_);
    virtual ~CBorderPassElement() = default;

    virtual void        draw(const CRegion& damage);
    virtual bool        needsLiveBlur();
    virtual bool        needsPrecomputeBlur();

    virtual const char* passName() {
        return "CBorderPassElement";
    }

  private:
    SBorderData data;
};