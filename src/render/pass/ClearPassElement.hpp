#pragma once
#include "PassElement.hpp"

class CClearPassElement : public IPassElement {
  public:
    struct SClearData {
        CHyprColor color;
    };

    CClearPassElement(const SClearData& data);
    virtual ~CClearPassElement() = default;

    virtual bool                needsLiveBlur(Render::CRenderContext& ctx);
    virtual bool                needsPrecomputeBlur(Render::CRenderContext& ctx);
    virtual std::optional<CBox> boundingBox(Render::CRenderContext& ctx);
    virtual CRegion             opaqueRegion(Render::CRenderContext& ctx);

    virtual const char*         passName() {
        return "CClearPassElement";
    }

    virtual ePassElementType type() {
        return EK_CLEAR;
    };

    SClearData m_data;
};
