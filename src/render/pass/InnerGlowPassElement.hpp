#pragma once
#include "PassElement.hpp"
#include "../WindowRenderPresentation.hpp"

class CHyprInnerGlowDecoration;

class CInnerGlowPassElement : public IPassElement {
  public:
    struct SInnerGlowData {
        WP<CHyprInnerGlowDecoration>      deco;
        float                             a = 1.F;
        Render::SWindowRenderPresentation presentation;
    };

    CInnerGlowPassElement(const SInnerGlowData& data_);
    virtual ~CInnerGlowPassElement() = default;

    virtual bool        needsLiveBlur(Render::CRenderContext& ctx);
    virtual bool        needsPrecomputeBlur(Render::CRenderContext& ctx);

    virtual const char* passName() {
        return "CInnerGlowPassElement";
    }

    virtual ePassElementType type() {
        return EK_INNER_GLOW;
    };

    SInnerGlowData m_data;
};
