#pragma once
#include "PassElement.hpp"

class CClearPassElement : public IPassElement {
  public:
    struct SClearData {
        CHyprColor color;
    };

    CClearPassElement(const SClearData& data);
    virtual ~CClearPassElement() = default;

    virtual bool                needsLiveBlur();
    virtual bool                needsPrecomputeBlur();
    virtual std::optional<CBox> boundingBox();
    virtual CRegion             opaqueRegion();

    virtual const char*         passName() {
        return "CClearPassElement";
    }

    virtual ePassElementKind kind() {
        return EK_CLEAR;
    };

    SClearData m_data;
};
