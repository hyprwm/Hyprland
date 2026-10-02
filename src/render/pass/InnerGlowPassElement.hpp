#pragma once
#include "PassElement.hpp"

class CHyprInnerGlowDecoration;

namespace Workspace {
    class CWorkspacePresentable;
}

class CInnerGlowPassElement : public IPassElement {
  public:
    struct SInnerGlowData {
        WP<CHyprInnerGlowDecoration>         deco;
        float                                a = 1.F;
        SP<Workspace::CWorkspacePresentable> presentation;
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
