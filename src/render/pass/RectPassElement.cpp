#include "RectPassElement.hpp"
#include "../OpenGL.hpp"

CRectPassElement::CRectPassElement(const CRectPassElement::SRectData& data_) : data(data_) {
    ;
}

void CRectPassElement::draw(const CRegion& damage) {
    if (data.box.w <= 0 || data.box.h <= 0)
        return;

    if (data.color.a == 1.F || !data.blur)
        g_pHyprOpenGL->renderRectWithDamage(&data.box, data.color, damage, data.round, data.roundingPower);
    else
        g_pHyprOpenGL->renderRectWithBlur(&data.box, data.color, data.round, data.roundingPower, data.blurA, data.xray);
}

bool CRectPassElement::needsLiveBlur() {
    return data.color.a < 1.F && !data.xray && data.blur;
}

bool CRectPassElement::needsPrecomputeBlur() {
    return data.color.a < 1.F && data.xray && data.blur;
}

std::optional<CBox> CRectPassElement::boundingBox() {
    return data.box.copy().expand(-data.round);
}

CRegion CRectPassElement::opaqueRegion() {
    return data.color.a >= 1.F ? *boundingBox() : CRegion{};
}
