#include "WLClasses.hpp"
#include "../config/ConfigManager.hpp"
#include "src/Compositor.hpp"

SLayerSurface::SLayerSurface() {
    alpha.create(AVARTYPE_FLOAT, g_pConfigManager->getAnimationPropertyConfig("fadeIn"), nullptr, AVARDAMAGE_ENTIRE);
    alpha.m_pLayer = this;
    alpha.registerVar();
}

void SLayerSurface::applyRules() {
    noAnimations = false;
    forceBlur    = false;
    ignoreZero   = false;
    ignoreFocus  = false;

    for (auto& rule : g_pConfigManager->getMatchingRules(this)) {
        if (rule.rule == "noanim")
            noAnimations = true;
        else if (rule.rule == "blur")
            forceBlur = true;
        else if (rule.rule == "ignorezero")
            ignoreZero = true;
        else if (rule.rule == "ignorefocus") {
            ignoreFocus = true;
            if(g_pCompositor->m_pLastFocus == layerSurface->surface) {
                unfocus();
            }
        }
    }
}

void SLayerSurface::unfocus() {
    const auto PMONITOR = g_pCompositor->getMonitorFromOutput(layerSurface->output);
    if (!PMONITOR)
        return;

    g_pInputManager->releaseAllMouseButtons();

    Vector2D       surfaceCoords;
    SLayerSurface* pFoundLayerSurface = nullptr;
    wlr_surface*   foundSurface       = nullptr;

    g_pCompositor->m_pLastFocus = nullptr;

    // find LS-es to focus
    foundSurface = g_pCompositor->vectorToLayerSurface(g_pInputManager->getMouseCoordsInternal(), &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY],
                                                       &surfaceCoords, &pFoundLayerSurface);

    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(g_pInputManager->getMouseCoordsInternal(), &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP],
                                                           &surfaceCoords, &pFoundLayerSurface);

    if (!foundSurface) {
        // if there isn't any, focus the last window
        const auto PLASTWINDOW = g_pCompositor->m_pLastWindow;
        g_pCompositor->focusWindow(nullptr);
        g_pCompositor->focusWindow(PLASTWINDOW);
    } else {
        // otherwise, full refocus
        g_pInputManager->refocus();
    }
}
