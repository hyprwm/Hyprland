#include "ShadowPassElement.hpp"
#include "../OpenGL.hpp"
#include "../decorations/CHyprDropShadowDecoration.hpp"

CShadowPassElement::CShadowPassElement(const CShadowPassElement::SShadowData& data_) : m_data(data_) {
    ;
}

void CShadowPassElement::draw(const CRegion& damage) {
    m_data.deco->render(g_pHyprOpenGL->m_renderData.pMonitor.lock(), m_data.a);
}

bool CShadowPassElement::needsLiveBlur() {
    return false;
}

bool CShadowPassElement::needsPrecomputeBlur() {
    return false;
}
