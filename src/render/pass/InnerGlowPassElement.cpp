#include "InnerGlowPassElement.hpp"

CInnerGlowPassElement::CInnerGlowPassElement(const CInnerGlowPassElement::SInnerGlowData& data_) : m_data(data_) {
    ;
}

bool CInnerGlowPassElement::needsLiveBlur() {
    return false;
}

bool CInnerGlowPassElement::needsPrecomputeBlur() {
    return false;
}
