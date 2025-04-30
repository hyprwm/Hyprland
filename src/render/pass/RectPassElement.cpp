#include "RectPassElement.hpp"
#include "../OpenGL.hpp"

CRectPassElement::CRectPassElement(const CRectPassElement::SRectData& data_) : data(data_) {
    ;
}

void CRectPassElement::draw(const CRegion& damage) {
    if (data.box.w <= 0 || data.box.h <= 0)
        return;

    if (!data.clipBox.empty())
        g_pHyprOpenGL->m_RenderData.clipBox = data.clipBox;

    if (data.color.a == 1.F || !data.blur)
        g_pHyprOpenGL->renderRectWithDamage(data.box, data.color, damage, data.round, data.roundingPower);
    else
        g_pHyprOpenGL->renderRectWithBlur(data.box, data.color, data.round, data.roundingPower, data.blurA, data.xray);

    g_pHyprOpenGL->m_RenderData.clipBox = {};
}

bool CRectPassElement::needsLiveBlur() {
    return data.color.a < 1.F && !data.xray && data.blur;
}

bool CRectPassElement::needsPrecomputeBlur() {
    return data.color.a < 1.F && data.xray && data.blur;
}

std::optional<CBox> CRectPassElement::boundingBox() {
    return data.box.copy().scale(1.F / g_pHyprOpenGL->m_RenderData.pMonitor->m_scale).round();
}

CRegion CRectPassElement::opaqueRegion() {
    if (data.color.a < 1.F)
        return CRegion{};

    CRegion rg = boundingBox()->expand(-data.round);

    if (!data.clipBox.empty())
        rg.intersect(data.clipBox);

    return rg;
}
