#include "RectPassElement.hpp"
#include "../OpenGL.hpp"

CRectPassElement::CRectPassElement(const CRectPassElement::SRectData& data_) : m_data(data_) {
    ;
}

void CRectPassElement::draw(const CRegion& damage) {
    if (m_data.box.w <= 0 || m_data.box.h <= 0)
        return;

    if (!m_data.clipBox.empty())
        g_pHyprOpenGL->m_renderData.clipBox = m_data.clipBox;

    if (m_data.color.a == 1.F || !m_data.blur)
        g_pHyprOpenGL->renderRect(m_data.box, m_data.color, {.damage = &damage, .round = m_data.round, .roundingPower = m_data.roundingPower});
    else
        g_pHyprOpenGL->renderRect(m_data.box, m_data.color,
                                  {.round = m_data.round, .roundingPower = m_data.roundingPower, .blur = true, .blurA = m_data.blurA, .xray = m_data.xray});

    g_pHyprOpenGL->m_renderData.clipBox = {};
}

bool CRectPassElement::needsLiveBlur() {
    return m_data.color.a < 1.F && !m_data.xray && m_data.blur;
}

bool CRectPassElement::needsPrecomputeBlur() {
    return m_data.color.a < 1.F && m_data.xray && m_data.blur;
}

std::optional<CBox> CRectPassElement::boundingBox() {
    return m_data.box.copy().scale(1.F / g_pHyprOpenGL->m_renderData.pMonitor->m_scale).round();
}

CRegion CRectPassElement::opaqueRegion() {
    if (m_data.color.a < 1.F)
        return CRegion{};

    CRegion rg = boundingBox()->expand(-m_data.round);

    if (!m_data.clipBox.empty())
        rg.intersect(m_data.clipBox);

    return rg;
}
