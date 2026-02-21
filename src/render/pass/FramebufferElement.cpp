#include "FramebufferElement.hpp"

CFramebufferElement::CFramebufferElement(const CFramebufferElement::SFramebufferElementData& data_) : m_data(data_) {
    ;
}

bool CFramebufferElement::needsLiveBlur() {
    return false;
}

bool CFramebufferElement::needsPrecomputeBlur() {
    return false;
}

bool CFramebufferElement::undiscardable() {
    return true;
}
