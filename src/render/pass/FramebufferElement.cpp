#include "FramebufferElement.hpp"

CFramebufferElement::CFramebufferElement(const CFramebufferElement::SFramebufferElementData& data_) : m_data(data_) {
    ;
}

bool CFramebufferElement::needsLiveBlur(const Render::CRenderingContext&) {
    return false;
}

bool CFramebufferElement::needsPrecomputeBlur(const Render::CRenderingContext&) {
    return false;
}

bool CFramebufferElement::undiscardable() {
    return true;
}
