#include "TexPassElement.hpp"
#include "../OpenGL.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::Utils;

CTexPassElement::CTexPassElement(const CTexPassElement::SRenderData& data_) : m_data(data_) {
    ;
}

void CTexPassElement::draw(const CRegion& damage) {
    g_pHyprOpenGL->m_endFrame = m_data.flipEndFrame;

    CScopeGuard x = {[this]() {
        //
        g_pHyprOpenGL->m_endFrame           = false;
        g_pHyprOpenGL->m_renderData.clipBox = {};
        if (m_data.replaceProjection)
            g_pHyprOpenGL->m_renderData.monitorProjection = g_pHyprOpenGL->m_renderData.pMonitor->m_projMatrix;
        if (m_data.ignoreAlpha.has_value())
            g_pHyprOpenGL->m_renderData.discardMode = 0;
    }};

    if (!m_data.clipBox.empty())
        g_pHyprOpenGL->m_renderData.clipBox = m_data.clipBox;

    if (m_data.replaceProjection)
        g_pHyprOpenGL->m_renderData.monitorProjection = *m_data.replaceProjection;

    if (m_data.ignoreAlpha.has_value()) {
        g_pHyprOpenGL->m_renderData.discardMode    = DISCARD_ALPHA;
        g_pHyprOpenGL->m_renderData.discardOpacity = *m_data.ignoreAlpha;
    }

    if (m_data.blur)
        g_pHyprOpenGL->renderTextureWithBlur(m_data.tex, m_data.box, m_data.a, nullptr, m_data.round, m_data.roundingPower, false, m_data.blurA, 1.F);
    else
        g_pHyprOpenGL->renderTextureInternalWithDamage(m_data.tex, m_data.box, m_data.a, m_data.damage.empty() ? damage : m_data.damage, m_data.round, m_data.roundingPower);
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
