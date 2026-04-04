#include "RectPassElement.hpp"
#include "../Renderer.hpp"

CRectPassElement::CRectPassElement(const CRectPassElement::SRectData& data_) : m_data(data_) {
    ;
}

bool CRectPassElement::needsLiveBlur() {
    return m_data.color.a < 1.F && !m_data.xray && m_data.blur;
}

bool CRectPassElement::needsPrecomputeBlur() {
    return m_data.color.a < 1.F && m_data.xray && m_data.blur;
}

std::optional<CBox> CRectPassElement::boundingBox() {
    return m_data.box.copy().scale(1.F / g_pHyprRenderer->m_renderData.pMonitor->m_scale).round();
}

CRegion CRectPassElement::opaqueRegion() {
    if (m_data.color.a < 1.F)
        return CRegion{};

    CRegion rg = boundingBox()->expand(-m_data.round);

    if (!m_data.clipBox.empty())
        rg.intersect(m_data.clipBox);

    return rg;
}
