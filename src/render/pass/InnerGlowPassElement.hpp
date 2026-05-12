#pragma once
#include "PassElement.hpp"

class CHyprInnerGlowDecoration;

class CInnerGlowPassElement : public IPassElement {
  public:
    struct SInnerGlowData {
        CHyprInnerGlowDecoration* deco = nullptr;
        float                     a    = 1.F;
    };

    CInnerGlowPassElement(const SInnerGlowData& data_);
    virtual ~CInnerGlowPassElement() = default;

    virtual bool        needsLiveBlur();
    virtual bool        needsPrecomputeBlur();

    virtual const char* passName() {
        return "CInnerGlowPassElement";
    }

    virtual ePassElementType type() {
        return EK_INNER_GLOW;
    };

    SInnerGlowData m_data;
};
