#include "RectPassElement.hpp"
#include "../OpenGL.hpp"

CRectPassElement::CRectPassElement(const CRectPassElement::SRectData& data_) : data(data_) {
    ;
}

void CRectPassElement::draw(const CRegion& damage) {
    if (data.color.a == 1.F)
        g_pHyprOpenGL->renderRectWithDamage(&data.box, data.color, damage, data.round);
    else
        g_pHyprOpenGL->renderRectWithBlur(&data.box, data.color, data.round, data.blurA, data.xray);
}

bool CRectPassElement::needsLiveBlur() {
    return data.color.a < 1.F && !data.xray;
}

bool CRectPassElement::needsPrecomputeBlur() {
    return data.color.a < 1.F && data.xray;
}