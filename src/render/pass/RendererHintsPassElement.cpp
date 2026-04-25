#include "RendererHintsPassElement.hpp"

CRendererHintsPassElement::CRendererHintsPassElement(const CRendererHintsPassElement::SData& data_) : m_data(data_) {
    ;
}

bool CRendererHintsPassElement::needsLiveBlur() {
    return false;
}

bool CRendererHintsPassElement::needsPrecomputeBlur() {
    return false;
}

bool CRendererHintsPassElement::undiscardable() {
    return true;
}
