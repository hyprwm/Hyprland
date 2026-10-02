#include "InnerGlowPassElement.hpp"

CInnerGlowPassElement::CInnerGlowPassElement(const CInnerGlowPassElement::SInnerGlowData& data_) : m_data(data_) {
    ;
}

bool CInnerGlowPassElement::needsLiveBlur(Render::CRenderContext& ctx) {
    return false;
}

bool CInnerGlowPassElement::needsPrecomputeBlur(Render::CRenderContext& ctx) {
    return false;
}
