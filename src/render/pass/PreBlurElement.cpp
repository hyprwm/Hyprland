#include "PreBlurElement.hpp"

CPreBlurElement::CPreBlurElement() = default;

bool CPreBlurElement::needsLiveBlur(const Render::CRenderingContext&) {
    return false;
}

bool CPreBlurElement::needsPrecomputeBlur(const Render::CRenderingContext&) {
    return false;
}

bool CPreBlurElement::disableSimplification() {
    return true;
}

bool CPreBlurElement::undiscardable() {
    return true;
}
