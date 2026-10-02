#include "FramebufferElement.hpp"

CFramebufferElement::CFramebufferElement(const CFramebufferElement::SFramebufferElementData& data_) : m_data(data_) {
    ;
}

bool CFramebufferElement::needsLiveBlur(Render::CRenderContext& ctx) {
    return false;
}

bool CFramebufferElement::needsPrecomputeBlur(Render::CRenderContext& ctx) {
    return false;
}

bool CFramebufferElement::undiscardable(Render::CRenderContext& ctx) {
    return true;
}
