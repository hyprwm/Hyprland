#include "BorderPassElement.hpp"
#include "../OpenGL.hpp"
#include "../../desktop/Window.hpp"
#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::Utils;

CBorderPassElement::CBorderPassElement(const CBorderPassElement::SBorderData& data_) : m_data(data_) {
    ;
}

void CBorderPassElement::draw(const CRegion& damage) {
    const auto prevWindow   = g_pHyprOpenGL->m_renderData.currentWindow;
    const bool prevCaptures = g_pHyprOpenGL->captureWritesEnabled();

    g_pHyprOpenGL->m_renderData.currentWindow = m_data.pWindow;
    const auto window                         = m_data.pWindow.lock();

    const bool allowCapture = !window || !window->m_windowData.noScreenShare.valueOrDefault() || window->m_windowData.noScreenShareMask.valueOrDefault().decorationsEnabled();

    g_pHyprOpenGL->setCaptureWritesEnabled(allowCapture);

    CScopeGuard guard{[prevWindow, prevCaptures]() {
        g_pHyprOpenGL->m_renderData.currentWindow = prevWindow;
        g_pHyprOpenGL->setCaptureWritesEnabled(prevCaptures);
    }};

    if (m_data.hasGrad2)
        g_pHyprOpenGL->renderBorder(
            m_data.box, m_data.grad1, m_data.grad2, m_data.lerp,
            {.round = m_data.round, .roundingPower = m_data.roundingPower, .borderSize = m_data.borderSize, .a = m_data.a, .outerRound = m_data.outerRound});
    else
        g_pHyprOpenGL->renderBorder(
            m_data.box, m_data.grad1,
            {.round = m_data.round, .roundingPower = m_data.roundingPower, .borderSize = m_data.borderSize, .a = m_data.a, .outerRound = m_data.outerRound});
}

bool CBorderPassElement::needsLiveBlur() {
    return false;
}

bool CBorderPassElement::needsPrecomputeBlur() {
    return false;
}
