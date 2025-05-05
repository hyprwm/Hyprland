#include "RendererHintsPassElement.hpp"
#include "../OpenGL.hpp"

CRendererHintsPassElement::CRendererHintsPassElement(const CRendererHintsPassElement::SData& data_) : m_data(data_) {
    ;
}

void CRendererHintsPassElement::draw(const CRegion& damage) {
    if (m_data.renderModif.has_value())
        g_pHyprOpenGL->m_renderData.renderModif = *m_data.renderModif;
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
