#include "PreBlurElement.hpp"

CPreBlurElement::CPreBlurElement() = default;

bool CPreBlurElement::needsLiveBlur(Render::CRenderContext& ctx) {
    return false;
}

bool CPreBlurElement::needsPrecomputeBlur(Render::CRenderContext& ctx) {
    return false;
}

bool CPreBlurElement::disableSimplification(Render::CRenderContext& ctx) {
    return true;
}

bool CPreBlurElement::requiresFullDamage(Render::CRenderContext& ctx) {
    return true;
}

bool CPreBlurElement::undiscardable(Render::CRenderContext& ctx) {
    return true;
}
