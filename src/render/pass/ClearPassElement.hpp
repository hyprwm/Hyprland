#pragma once
#include "PassElement.hpp"

class CClearPassElement : public IPassElement {
  public:
    struct SClearData {
        CHyprColor color;
    };

    CClearPassElement(const SClearData& data);
    virtual ~CClearPassElement() = default;

    virtual bool                needsLiveBlur(const Render::CRenderingContext&);
    virtual bool                needsPrecomputeBlur(const Render::CRenderingContext&);
    virtual std::optional<CBox> boundingBox(const Render::CRenderingContext& context);
    virtual CRegion             opaqueRegion(const Render::CRenderingContext& context);

    virtual const char*         passName() {
        return "CClearPassElement";
    }

    virtual ePassElementType type() {
        return EK_CLEAR;
    };

    SClearData m_data;
};
