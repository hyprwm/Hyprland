#pragma once
#include "PassElement.hpp"

class CPreBlurElement : public IPassElement {
  public:
    CPreBlurElement();
    virtual ~CPreBlurElement() = default;

    virtual bool        needsLiveBlur(Render::CRenderContext& ctx);
    virtual bool        needsPrecomputeBlur(Render::CRenderContext& ctx);
    virtual bool        disableSimplification(Render::CRenderContext& ctx);
    virtual bool        requiresFullDamage(Render::CRenderContext& ctx);
    virtual bool        undiscardable(Render::CRenderContext& ctx);

    virtual const char* passName() {
        return "CPreBlurElement";
    }

    virtual ePassElementType type() {
        return EK_PRE_BLUR;
    };
};
