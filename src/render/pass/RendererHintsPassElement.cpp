#include "RendererHintsPassElement.hpp"
#include "../OpenGL.hpp"

CRendererHintsPassElement::CRendererHintsPassElement(const CRendererHintsPassElement::SData& data_) : data(data_) {
    ;
}

void CRendererHintsPassElement::draw(const CRegion& damage) {
    if (data.renderModif.has_value())
        g_pHyprOpenGL->m_RenderData.renderModif = *data.renderModif;
}

bool CRendererHintsPassElement::needsLiveBlur() {
    return false;
}

bool CRendererHintsPassElement::needsPrecomputeBlur() {
    return false;
}
