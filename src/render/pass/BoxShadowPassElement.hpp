#pragma once

#include "ShadowPassElement.hpp"

class CBoxShadowPassElement : public CShadowPassElement {
  public:
    struct SBoxShadowData {
        CBox       box;
        CBox       cutoutBox;
        CBox       clipBox;
        CHyprColor color;
        float      a             = 1.F;
        int        round         = 0;
        float      roundingPower = 2.F;
        int        range         = 0;
    };

    CBoxShadowPassElement(const SBoxShadowData& data);
    virtual ~CBoxShadowPassElement() = default;

    virtual std::optional<CBox> boundingBox(const Render::CRenderingContext& context);

    virtual const char*         passName() {
        return "CBoxShadowPassElement";
    }

    SBoxShadowData m_boxData;
};
