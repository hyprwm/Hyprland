#pragma once
#include "PassElement.hpp"
#include "../WindowRenderPresentation.hpp"

class CHyprDropShadowDecoration;

class CShadowPassElement : public IPassElement {
  public:
    struct SShadowData {
        WP<CHyprDropShadowDecoration>     deco;
        float                             a = 1.F;
        Render::SWindowRenderPresentation presentation;
    };

    CShadowPassElement(const SShadowData& data_);
    virtual ~CShadowPassElement() = default;

    virtual bool        needsLiveBlur(Render::CRenderContext& ctx);
    virtual bool        needsPrecomputeBlur(Render::CRenderContext& ctx);

    virtual const char* passName() {
        return "CShadowPassElement";
    }

    virtual ePassElementType type() {
        return EK_SHADOW;
    };

    SShadowData m_data;
};
