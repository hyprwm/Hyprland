#include "RendererHintsPassElement.hpp"

CRendererHintsPassElement::CRendererHintsPassElement(const CRendererHintsPassElement::SData& data_) : m_data(data_) {
    ;
}

bool CRendererHintsPassElement::needsLiveBlur(const Render::CRenderingContext&) {
    return false;
}

bool CRendererHintsPassElement::needsPrecomputeBlur(const Render::CRenderingContext&) {
    return false;
}

bool CRendererHintsPassElement::undiscardable() {
    return true;
}
