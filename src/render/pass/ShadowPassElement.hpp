#pragma once
#include "PassElement.hpp"

class CHyprDropShadowDecoration;

class CShadowPassElement : public IPassElement {
  public:
    struct SShadowData {
        WP<CHyprDropShadowDecoration> deco;
        float                         a = 1.F;
    };

    CShadowPassElement(const SShadowData& data_);
    virtual ~CShadowPassElement() = default;

    virtual bool        needsLiveBlur(const Render::CRenderingContext&);
    virtual bool        needsPrecomputeBlur(const Render::CRenderingContext&);

    virtual const char* passName() {
        return "CShadowPassElement";
    }

    virtual ePassElementType type() {
        return EK_SHADOW;
    };

    SShadowData m_data;
};
