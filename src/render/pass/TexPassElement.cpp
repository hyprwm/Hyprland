#include "TexPassElement.hpp"
#include "../Renderer.hpp"
#include "macros.hpp"

CTexPassElement::CTexPassElement(const SRenderData& data) : m_data(data) {
    RASSERT(m_data.tex && m_data.tex->ok(), "Trying to render invalid tex");
}

CTexPassElement::CTexPassElement(CTexPassElement::SRenderData&& data) : m_data(std::move(data)) {
    RASSERT(m_data.tex && m_data.tex->ok(), "Trying to render invalid tex");
}

bool CTexPassElement::needsLiveBlur() {
    return false; // TODO?
}

bool CTexPassElement::needsPrecomputeBlur() {
    return false; // TODO?
}

std::optional<CBox> CTexPassElement::boundingBox() {
    return m_data.box.copy().scale(1.F / g_pHyprRenderer->m_renderData.pMonitor->m_scale).round();
}

CRegion CTexPassElement::opaqueRegion() {
    return {}; // TODO:
}

void CTexPassElement::discard() {
    ;
}
