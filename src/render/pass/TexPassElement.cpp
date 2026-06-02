#include "TexPassElement.hpp"
#include "../../helpers/MotionBlur.hpp"
#include "../Renderer.hpp"

CBox SMotionBlurData::extents() const {
    if (!enabled)
        return current;

    return MotionBlur::extents(previous, current);
}

CTexPassElement::CTexPassElement(const SRenderData& data) : m_data(data) {
    ;
}

CTexPassElement::CTexPassElement(CTexPassElement::SRenderData&& data) : m_data(std::move(data)) {
    ;
}

bool CTexPassElement::needsLiveBlur() {
    return false; // TODO?
}

bool CTexPassElement::needsPrecomputeBlur() {
    return false; // TODO?
}

std::optional<CBox> CTexPassElement::boundingBox() {
    if (m_data.motionBlur.enabled)
        return m_data.motionBlur.extents().copy().scale(1.F / g_pHyprRenderer->m_renderData.pMonitor->m_scale).round();

    return m_data.box.copy().scale(1.F / g_pHyprRenderer->m_renderData.pMonitor->m_scale).round();
}

CRegion CTexPassElement::opaqueRegion() {
    return {}; // TODO:
}

void CTexPassElement::discard() {
    ;
}
