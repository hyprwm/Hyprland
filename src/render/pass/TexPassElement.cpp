#include "TexPassElement.hpp"
#include "../OpenGL.hpp"
#include "../Renderer.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::Utils;

CTexPassElement::CTexPassElement(const SRenderData& data) : m_data(data) {
    ;
}

CTexPassElement::CTexPassElement(CTexPassElement::SRenderData&& data) : m_data(std::move(data)) {
    ;
}

void CTexPassElement::draw(const CRegion& damage) {
    g_pHyprOpenGL->pushMonitorTransformEnabled(m_data.flipEndFrame);

    const bool  blackoutRequested = m_data.captureBlackout && CHyprRenderer::shouldBlackoutNoScreenShare();
    const bool  maskApplied       = blackoutRequested && g_pHyprOpenGL->captureMRTActiveForCurrentMonitor();
    const bool  allowCapture      = m_data.captureWrites || maskApplied;

    auto        captureState = g_pHyprOpenGL->captureStateGuard(allowCapture, maskApplied);

    CScopeGuard x = {[this]() {
        g_pHyprOpenGL->popMonitorTransformEnabled();
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

    if (m_data.blur) {
        g_pHyprOpenGL->renderTexture(m_data.tex, m_data.box,
                                     {
                                         .a                     = m_data.a,
                                         .blur                  = true,
                                         .blurA                 = m_data.blurA,
                                         .overallA              = 1.F,
                                         .round                 = m_data.round,
                                         .roundingPower         = m_data.roundingPower,
                                         .blockBlurOptimization = m_data.blockBlurOptimization.value_or(false),
                                     });
    } else {
        g_pHyprOpenGL->renderTexture(m_data.tex, m_data.box,
                                     {.damage = m_data.damage.empty() ? &damage : &m_data.damage, .a = m_data.a, .round = m_data.round, .roundingPower = m_data.roundingPower});
    }
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
