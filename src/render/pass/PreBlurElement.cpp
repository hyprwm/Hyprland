#include "PreBlurElement.hpp"
#include "../OpenGL.hpp"

CPreBlurElement::CPreBlurElement() = default;

void CPreBlurElement::draw(const CRegion& damage) {
    g_pHyprOpenGL->preBlurForCurrentMonitor();
}

bool CPreBlurElement::needsLiveBlur() {
    return false;
}

bool CPreBlurElement::needsPrecomputeBlur() {
    return false;
}

bool CPreBlurElement::disableSimplification() {
    return true;
}
