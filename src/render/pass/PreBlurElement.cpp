#include "PreBlurElement.hpp"

CPreBlurElement::CPreBlurElement() = default;

bool CPreBlurElement::needsLiveBlur() {
    return false;
}

bool CPreBlurElement::needsPrecomputeBlur() {
    return false;
}

bool CPreBlurElement::disableSimplification() {
    return true;
}

bool CPreBlurElement::requiresFullDamage() {
    return true;
}

bool CPreBlurElement::undiscardable() {
    return true;
}
