#include "InnerGlowPassElement.hpp"

CInnerGlowPassElement::CInnerGlowPassElement(const CInnerGlowPassElement::SInnerGlowData& data_) : m_data(data_) {
    ;
}

bool CInnerGlowPassElement::needsLiveBlur(const Render::CRenderingContext&) {
    return false;
}

bool CInnerGlowPassElement::needsPrecomputeBlur(const Render::CRenderingContext&) {
    return false;
}
