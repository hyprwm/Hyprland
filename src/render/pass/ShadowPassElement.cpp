#include "ShadowPassElement.hpp"
#include "../OpenGL.hpp"
#include "../decorations/CHyprDropShadowDecoration.hpp"

CShadowPassElement::CShadowPassElement(const CShadowPassElement::SShadowData& data_) : data(data_) {
    ;
}

void CShadowPassElement::draw(const CRegion& damage) {
    data.deco->render(g_pHyprOpenGL->m_RenderData.pMonitor.lock(), data.a);
}

bool CShadowPassElement::needsLiveBlur() {
    return false;
}

bool CShadowPassElement::needsPrecomputeBlur() {
    return false;
}