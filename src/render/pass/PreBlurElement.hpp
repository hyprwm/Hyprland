#pragma once
#include "PassElement.hpp"

class CPreBlurElement : public IPassElement {
  public:
    CPreBlurElement();
    virtual ~CPreBlurElement() = default;

    virtual bool        needsLiveBlur(const Render::CRenderingContext&);
    virtual bool        needsPrecomputeBlur(const Render::CRenderingContext&);
    virtual bool        disableSimplification();
    virtual bool        requiresFullDamage();
    virtual bool        undiscardable();

    virtual const char* passName() {
        return "CPreBlurElement";
    }

    virtual ePassElementType type() {
        return EK_PRE_BLUR;
    };
};
