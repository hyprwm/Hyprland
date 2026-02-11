#include "TexPassElement.hpp"
#include "../OpenGL.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::Utils;

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
    return m_data.box.copy().scale(1.F / g_pHyprOpenGL->m_renderData.pMonitor->m_scale).round();
}

CRegion CTexPassElement::opaqueRegion() {
    return {}; // TODO:
}

void CTexPassElement::discard() {
    ;
}
