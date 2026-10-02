#pragma once
#include "PassElement.hpp"

class CHyprDropShadowDecoration;

namespace Workspace {
    class CWorkspacePresentable;
}

class CShadowPassElement : public IPassElement {
  public:
    struct SShadowData {
        WP<CHyprDropShadowDecoration>        deco;
        float                                a = 1.F;
        SP<Workspace::CWorkspacePresentable> presentation;
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
