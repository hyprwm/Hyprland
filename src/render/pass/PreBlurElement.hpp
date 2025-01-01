#pragma once
#include "PassElement.hpp"

class CPreBlurElement : public IPassElement {
  public:
    CPreBlurElement();
    virtual ~CPreBlurElement() = default;

    virtual void        draw(const CRegion& damage);
    virtual bool        needsLiveBlur();
    virtual bool        needsPrecomputeBlur();
    virtual bool        disableSimplification();
    virtual bool        undiscardable();

    virtual const char* passName() {
        return "CPreBlurElement";
    }
};