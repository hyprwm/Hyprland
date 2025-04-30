#include "TexPassElement.hpp"
#include "../OpenGL.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::Utils;

CTexPassElement::CTexPassElement(const CTexPassElement::SRenderData& data_) : data(data_) {
    ;
}

void CTexPassElement::draw(const CRegion& damage) {
    g_pHyprOpenGL->m_bEndFrame = data.flipEndFrame;

    CScopeGuard x = {[]() {
        //
        g_pHyprOpenGL->m_bEndFrame          = false;
        g_pHyprOpenGL->m_RenderData.clipBox = {};
    }};

    if (!data.clipBox.empty())
        g_pHyprOpenGL->m_RenderData.clipBox = data.clipBox;

    if (data.replaceProjection)
        g_pHyprOpenGL->m_RenderData.monitorProjection = *data.replaceProjection;
    g_pHyprOpenGL->renderTextureInternalWithDamage(data.tex, data.box, data.a, data.damage.empty() ? damage : data.damage, data.round, data.roundingPower);
    if (data.replaceProjection)
        g_pHyprOpenGL->m_RenderData.monitorProjection = g_pHyprOpenGL->m_RenderData.pMonitor->m_projMatrix;
}

bool CTexPassElement::needsLiveBlur() {
    return false; // TODO?
}

bool CTexPassElement::needsPrecomputeBlur() {
    return false; // TODO?
}

std::optional<CBox> CTexPassElement::boundingBox() {
    return data.box.copy().scale(1.F / g_pHyprOpenGL->m_RenderData.pMonitor->m_scale).round();
}

CRegion CTexPassElement::opaqueRegion() {
    return {}; // TODO:
}

void CTexPassElement::discard() {
    ;
}
