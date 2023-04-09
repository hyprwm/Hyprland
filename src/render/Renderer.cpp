#include "Renderer.hpp"
#include "../Compositor.hpp"
#include "linux-dmabuf-unstable-v1-protocol.h"

void renderSurface(struct wlr_surface* surface, int x, int y, void* data) {
    const auto TEXTURE = wlr_surface_get_texture(surface);
    const auto RDATA   = (SRenderData*)data;

    if (!TEXTURE)
        return;

    double outputX = 0, outputY = 0;
    wlr_output_layout_output_coords(g_pCompositor->m_sWLROutputLayout, RDATA->pMonitor->output, &outputX, &outputY);

    wlr_box windowBox;
    if (RDATA->surface && surface == RDATA->surface)
        windowBox = {(int)outputX + RDATA->x + x, (int)outputY + RDATA->y + y, RDATA->w, RDATA->h};
    else //  here we clamp to 2, these might be some tiny specks
        windowBox = {(int)outputX + RDATA->x + x, (int)outputY + RDATA->y + y, std::max(surface->current.width, 2), std::max(surface->current.height, 2)};

    if (RDATA->squishOversized) {
        if (x + windowBox.width > RDATA->w)
            windowBox.width = RDATA->w - x;
        if (y + windowBox.height > RDATA->h)
            windowBox.height = RDATA->h - y;
    }

    g_pHyprRenderer->calculateUVForSurface(RDATA->pWindow, surface, RDATA->squishOversized);

    scaleBox(&windowBox, RDATA->pMonitor->scale);

    static auto* const PROUNDING = &g_pConfigManager->getConfigValuePtr("decoration:rounding")->intValue;

    float              rounding = RDATA->dontRound ? 0 : RDATA->rounding == -1 ? *PROUNDING : RDATA->rounding;
    rounding *= RDATA->pMonitor->scale;

    rounding -= 1; // to fix a border issue

    if (RDATA->surface && surface == RDATA->surface) {
        if (wlr_xwayland_surface_try_from_wlr_surface(surface) && !wlr_xwayland_surface_try_from_wlr_surface(surface)->has_alpha && RDATA->fadeAlpha * RDATA->alpha == 1.f) {
            g_pHyprOpenGL->renderTexture(TEXTURE, &windowBox, RDATA->fadeAlpha * RDATA->alpha, rounding, true);
        } else {
            if (RDATA->blur)
                g_pHyprOpenGL->renderTextureWithBlur(TEXTURE, &windowBox, RDATA->fadeAlpha * RDATA->alpha, surface, rounding, RDATA->blockBlurOptimization);
            else
                g_pHyprOpenGL->renderTexture(TEXTURE, &windowBox, RDATA->fadeAlpha * RDATA->alpha, rounding, true);
        }
    } else {
        g_pHyprOpenGL->renderTexture(TEXTURE, &windowBox, RDATA->fadeAlpha * RDATA->alpha, rounding, true);
    }

    if (!g_pHyprRenderer->m_bBlockSurfaceFeedback) {
        wlr_surface_send_frame_done(surface, RDATA->when);
        wlr_presentation_surface_sampled_on_output(g_pCompositor->m_sWLRPresentation, surface, RDATA->pMonitor->output);
    }

    // reset the UV, we might've set it above
    g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
    g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
}

bool CHyprRenderer::shouldRenderWindow(CWindow* pWindow, CMonitor* pMonitor) {
    wlr_box geometry = pWindow->getFullWindowBoundingBox();

    if (!wlr_output_layout_intersects(g_pCompositor->m_sWLROutputLayout, pMonitor->output, &geometry))
        return false;

    if (pWindow->m_bPinned)
        return true;

    // now check if it has the same workspace
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    if (PWORKSPACE && PWORKSPACE->m_iMonitorID == pMonitor->ID) {
        if (PWORKSPACE->m_vRenderOffset.isBeingAnimated() || PWORKSPACE->m_fAlpha.isBeingAnimated() || PWORKSPACE->m_bForceRendering) {
            return true;
        } else {
            if (!(!PWORKSPACE->m_bHasFullscreenWindow || pWindow->m_bIsFullscreen || (pWindow->m_bIsFloating && pWindow->m_bCreatedOverFullscreen)))
                return false;
        }
    }

    if (pWindow->m_iWorkspaceID == pMonitor->activeWorkspace)
        return true;

    // if not, check if it maybe is active on a different monitor.
    if (g_pCompositor->isWorkspaceVisible(pWindow->m_iWorkspaceID) && pWindow->m_bIsFloating /* tiled windows can't be multi-ws */)
        return !pWindow->m_bIsFullscreen; // Do not draw fullscreen windows on other monitors

    if (pMonitor->specialWorkspaceID == pWindow->m_iWorkspaceID)
        return true;

    return false;
}

bool CHyprRenderer::shouldRenderWindow(CWindow* pWindow) {

    if (!g_pCompositor->windowValidMapped(pWindow))
        return false;

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    if (pWindow->m_bPinned || PWORKSPACE->m_bForceRendering)
        return true;

    if (g_pCompositor->isWorkspaceVisible(pWindow->m_iWorkspaceID))
        return true;

    for (auto& m : g_pCompositor->m_vMonitors) {
        if (PWORKSPACE && PWORKSPACE->m_iMonitorID == m->ID && (PWORKSPACE->m_vRenderOffset.isBeingAnimated() || PWORKSPACE->m_fAlpha.isBeingAnimated()))
            return true;

        if (m->specialWorkspaceID && g_pCompositor->isWorkspaceSpecial(pWindow->m_iWorkspaceID))
            return true;
    }

    return false;
}

void CHyprRenderer::renderWorkspaceWithFullscreenWindow(CMonitor* pMonitor, CWorkspace* pWorkspace, timespec* time) {
    CWindow* pWorkspaceWindow = nullptr;

    // loop over the tiled windows that are fading out
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_iWorkspaceID != pMonitor->activeWorkspace)
            continue;

        if (w->m_fAlpha.fl() == 0.f)
            continue;

        if (w->m_bIsFullscreen || w->m_bIsFloating)
            continue;

        renderWindow(w.get(), pMonitor, time, true, RENDER_PASS_ALL);
    }

    // and floating ones too
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_iWorkspaceID != pMonitor->activeWorkspace)
            continue;

        if (w->m_fAlpha.fl() == 0.f)
            continue;

        if (w->m_bIsFullscreen || !w->m_bIsFloating)
            continue;

        renderWindow(w.get(), pMonitor, time, true, RENDER_PASS_ALL);
    }

    for (auto& w : g_pCompositor->m_vWindows) {
        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID);

        if (w->m_iWorkspaceID != pWorkspace->m_iID || !w->m_bIsFullscreen) {
            if (!(PWORKSPACE && (PWORKSPACE->m_vRenderOffset.isBeingAnimated() || PWORKSPACE->m_fAlpha.isBeingAnimated() || PWORKSPACE->m_bForceRendering)))
                continue;

            if (w->m_iMonitorID != pMonitor->ID)
                continue;
        }

        if (w->m_iWorkspaceID == pMonitor->activeWorkspace && !w->m_bIsFullscreen)
            continue;

        renderWindow(w.get(), pMonitor, time, pWorkspace->m_efFullscreenMode != FULLSCREEN_FULL, RENDER_PASS_ALL);

        pWorkspaceWindow = w.get();
    }

    if (!pWorkspaceWindow) {
        // ?? happens sometimes...
        pWorkspace->m_bHasFullscreenWindow = false;
        return; // this will produce one blank frame. Oh well.
    }

    // then render windows over fullscreen.
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_iWorkspaceID != pWorkspaceWindow->m_iWorkspaceID || (!w->m_bCreatedOverFullscreen && !w->m_bPinned) || !w->m_bIsMapped)
            continue;

        renderWindow(w.get(), pMonitor, time, true, RENDER_PASS_ALL);
    }

    // and then special windows
    if (pMonitor->specialWorkspaceID)
        for (auto& w : g_pCompositor->m_vWindows) {
            if (!g_pCompositor->windowValidMapped(w.get()) && !w->m_bFadingOut)
                continue;

            if (w->m_iWorkspaceID != pMonitor->specialWorkspaceID)
                continue;

            if (!shouldRenderWindow(w.get(), pMonitor))
                continue;

            // render the bad boy
            renderWindow(w.get(), pMonitor, time, true, RENDER_PASS_ALL);
        }

    // and the overlay layers
    for (auto& ls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        if (ls->alpha.fl() != 0.f)
            renderLayer(ls.get(), pMonitor, time);
    }

    for (auto& ls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
        renderLayer(ls.get(), pMonitor, time);
    }

    renderDragIcon(pMonitor, time);

    // if correct monitor draw hyprerror
    if (pMonitor == g_pCompositor->m_vMonitors.front().get())
        g_pHyprError->draw();

    if (g_pSessionLockManager->isSessionLocked()) {
        const auto PSLS = g_pSessionLockManager->getSessionLockSurfaceForMonitor(pMonitor->ID);

        if (!PSLS) {
            // locked with no surface, fill with red
            wlr_box boxe = {0, 0, INT16_MAX, INT16_MAX};
            g_pHyprOpenGL->renderRect(&boxe, CColor(1.0, 0.2, 0.2, 1.0));
        } else {
            renderSessionLockSurface(PSLS, pMonitor, time);
        }
    }
}

void CHyprRenderer::renderWindow(CWindow* pWindow, CMonitor* pMonitor, timespec* time, bool decorate, eRenderPassMode mode, bool ignorePosition, bool ignoreAllGeometry) {
    if (pWindow->isHidden())
        return;

    if (pWindow->m_bFadingOut) {
        if (pMonitor->ID == pWindow->m_iMonitorID) // TODO: fix this
            g_pHyprOpenGL->renderSnapshot(&pWindow);
        return;
    }

    const auto         PWORKSPACE         = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);
    const auto         REALPOS            = pWindow->m_vRealPosition.vec() + (pWindow->m_bPinned ? Vector2D{} : PWORKSPACE->m_vRenderOffset.vec());
    static auto* const PNOFLOATINGBORDERS = &g_pConfigManager->getConfigValuePtr("general:no_border_on_floating")->intValue;
    static auto* const PDIMAROUND         = &g_pConfigManager->getConfigValuePtr("decoration:dim_around")->floatValue;

    SRenderData        renderdata = {pMonitor, time, REALPOS.x, REALPOS.y};
    if (ignorePosition) {
        renderdata.x = pMonitor->vecPosition.x;
        renderdata.y = pMonitor->vecPosition.y;
    }

    if (ignoreAllGeometry)
        decorate = false;

    renderdata.surface   = pWindow->m_pWLSurface.wlr();
    renderdata.w         = std::max(pWindow->m_vRealSize.vec().x, 5.0); // clamp the size to min 5,
    renderdata.h         = std::max(pWindow->m_vRealSize.vec().y, 5.0); // otherwise we'll have issues later with invalid boxes
    renderdata.dontRound = (pWindow->m_bIsFullscreen && PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL) || (!pWindow->m_sSpecialRenderData.rounding);
    renderdata.fadeAlpha = pWindow->m_fAlpha.fl() * (pWindow->m_bPinned ? 1.f : PWORKSPACE->m_fAlpha.fl());
    renderdata.alpha     = pWindow->m_fActiveInactiveAlpha.fl();
    renderdata.decorate  = decorate && !pWindow->m_bX11DoesntWantBorders && (pWindow->m_bIsFloating ? *PNOFLOATINGBORDERS == 0 : true) &&
        (!pWindow->m_bIsFullscreen || PWORKSPACE->m_efFullscreenMode != FULLSCREEN_FULL);
    renderdata.rounding = ignoreAllGeometry ? 0 : pWindow->m_sAdditionalConfigData.rounding.toUnderlying();
    renderdata.blur     = !ignoreAllGeometry; // if it shouldn't, it will be ignored later
    renderdata.pWindow  = pWindow;

    if (ignoreAllGeometry) {
        renderdata.alpha     = 1.f;
        renderdata.fadeAlpha = 1.f;
    }

    // apply opaque
    if (pWindow->m_sAdditionalConfigData.forceOpaque)
        renderdata.alpha = 1.f;

    g_pHyprOpenGL->m_pCurrentWindow = pWindow;

    if (*PDIMAROUND && pWindow->m_sAdditionalConfigData.dimAround && !m_bRenderingSnapshot && mode != RENDER_PASS_POPUP) {
        wlr_box monbox = {0, 0, g_pHyprOpenGL->m_RenderData.pMonitor->vecTransformedSize.x, g_pHyprOpenGL->m_RenderData.pMonitor->vecTransformedSize.y};
        g_pHyprOpenGL->renderRect(&monbox, CColor(0, 0, 0, *PDIMAROUND * renderdata.alpha * renderdata.fadeAlpha));
    }

    // clip box for animated offsets
    Vector2D offset;
    if (!ignorePosition && pWindow->m_bIsFloating && !pWindow->m_bPinned) {
        if (PWORKSPACE->m_vRenderOffset.vec().x != 0) {
            const auto PWSMON   = g_pCompositor->getMonitorFromID(PWORKSPACE->m_iMonitorID);
            const auto PROGRESS = PWORKSPACE->m_vRenderOffset.vec().x / PWSMON->vecSize.x;
            const auto WINBB    = pWindow->getFullWindowBoundingBox();

            if (WINBB.x < PWSMON->vecPosition.x) {
                offset.x = (PWSMON->vecPosition.x - WINBB.x) * PROGRESS;
            } else if (WINBB.x + WINBB.width > PWSMON->vecPosition.x + PWSMON->vecSize.x) {
                offset.x = (WINBB.x + WINBB.width - PWSMON->vecPosition.x - PWSMON->vecSize.x) * PROGRESS;
            }
        } else if (PWORKSPACE->m_vRenderOffset.vec().y) {
            const auto PWSMON   = g_pCompositor->getMonitorFromID(PWORKSPACE->m_iMonitorID);
            const auto PROGRESS = PWORKSPACE->m_vRenderOffset.vec().y / PWSMON->vecSize.y;
            const auto WINBB    = pWindow->getFullWindowBoundingBox();

            if (WINBB.y < PWSMON->vecPosition.y) {
                offset.y = (PWSMON->vecPosition.y - WINBB.y) * PROGRESS;
            } else if (WINBB.y + WINBB.height > PWSMON->vecPosition.y + PWSMON->vecSize.y) {
                offset.y = (WINBB.y + WINBB.width - PWSMON->vecPosition.y - PWSMON->vecSize.y) * PROGRESS;
            }
        }

        renderdata.x += offset.x;
        renderdata.y += offset.y;
    }

    // render window decorations first, if not fullscreen full
    if (mode == RENDER_PASS_ALL || mode == RENDER_PASS_MAIN) {
        if (!pWindow->m_bIsFullscreen || PWORKSPACE->m_efFullscreenMode != FULLSCREEN_FULL)
            for (auto& wd : pWindow->m_dWindowDecorations)
                wd->draw(pMonitor, renderdata.alpha * renderdata.fadeAlpha, offset);

        wlr_surface_for_each_surface(pWindow->m_pWLSurface.wlr(), renderSurface, &renderdata);

        if (renderdata.decorate && pWindow->m_sSpecialRenderData.border) {
            static auto* const PROUNDING = &g_pConfigManager->getConfigValuePtr("decoration:rounding")->intValue;

            float              rounding = renderdata.dontRound ? 0 : renderdata.rounding == -1 ? *PROUNDING : renderdata.rounding;
            rounding *= pMonitor->scale;

            auto       grad     = g_pHyprOpenGL->m_pCurrentWindow->m_cRealBorderColor;
            const bool ANIMATED = g_pHyprOpenGL->m_pCurrentWindow->m_fBorderFadeAnimationProgress.isBeingAnimated();
            float      a1       = renderdata.fadeAlpha * renderdata.alpha * (ANIMATED ? g_pHyprOpenGL->m_pCurrentWindow->m_fBorderFadeAnimationProgress.fl() : 1.f);

            if (g_pHyprOpenGL->m_pCurrentWindow->m_fBorderAngleAnimationProgress.getConfig()->pValues->internalEnabled) {
                grad.m_fAngle += g_pHyprOpenGL->m_pCurrentWindow->m_fBorderAngleAnimationProgress.fl() * M_PI * 2;
                grad.m_fAngle = normalizeAngleRad(grad.m_fAngle);
            }

            wlr_box windowBox = {renderdata.x - pMonitor->vecPosition.x, renderdata.y - pMonitor->vecPosition.y, renderdata.w, renderdata.h};

            scaleBox(&windowBox, pMonitor->scale);

            g_pHyprOpenGL->renderBorder(&windowBox, grad, rounding, a1);

            if (ANIMATED) {
                float a2 = renderdata.fadeAlpha * renderdata.alpha * (1.f - g_pHyprOpenGL->m_pCurrentWindow->m_fBorderFadeAnimationProgress.fl());
                g_pHyprOpenGL->renderBorder(&windowBox, g_pHyprOpenGL->m_pCurrentWindow->m_cRealBorderColorPrevious, rounding, a2);
            }
        }
    }

    if (mode == RENDER_PASS_ALL || mode == RENDER_PASS_POPUP) {
        if (!pWindow->m_bIsX11) {
            wlr_box geom;
            wlr_xdg_surface_get_geometry(pWindow->m_uSurface.xdg, &geom);

            renderdata.x -= geom.x;
            renderdata.y -= geom.y;

            renderdata.dontRound       = true; // don't round popups
            renderdata.pMonitor        = pMonitor;
            renderdata.squishOversized = false; // don't squish popups
            wlr_xdg_surface_for_each_popup_surface(pWindow->m_uSurface.xdg, renderSurface, &renderdata);
        }
    }

    g_pHyprOpenGL->m_pCurrentWindow     = nullptr;
    g_pHyprOpenGL->m_RenderData.clipBox = {0, 0, 0, 0};
}

void CHyprRenderer::renderLayer(SLayerSurface* pLayer, CMonitor* pMonitor, timespec* time) {
    if (pLayer->fadingOut) {
        g_pHyprOpenGL->renderSnapshot(&pLayer);
        return;
    }

    SRenderData renderdata           = {pMonitor, time, pLayer->geometry.x, pLayer->geometry.y};
    renderdata.fadeAlpha             = pLayer->alpha.fl();
    renderdata.blur                  = pLayer->forceBlur;
    renderdata.surface               = pLayer->layerSurface->surface;
    renderdata.decorate              = false;
    renderdata.w                     = pLayer->geometry.width;
    renderdata.h                     = pLayer->geometry.height;
    renderdata.blockBlurOptimization = pLayer->layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM || pLayer->layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;

    if (pLayer->ignoreZero)
        g_pHyprOpenGL->m_RenderData.discardMode |= DISCARD_ALPHAZERO;
    wlr_surface_for_each_surface(pLayer->layerSurface->surface, renderSurface, &renderdata);
    g_pHyprOpenGL->m_RenderData.discardMode &= ~DISCARD_ALPHAZERO;

    renderdata.squishOversized = false; // don't squish popups
    renderdata.dontRound       = true;
    wlr_layer_surface_v1_for_each_popup_surface(pLayer->layerSurface, renderSurface, &renderdata);
}

void CHyprRenderer::renderIMEPopup(SIMEPopup* pPopup, CMonitor* pMonitor, timespec* time) {
    SRenderData renderdata = {pMonitor, time, pPopup->realX, pPopup->realY};

    renderdata.blur     = false;
    renderdata.surface  = pPopup->pSurface->surface;
    renderdata.decorate = false;
    renderdata.w        = pPopup->pSurface->surface->current.width;
    renderdata.h        = pPopup->pSurface->surface->current.height;

    wlr_surface_for_each_surface(pPopup->pSurface->surface, renderSurface, &renderdata);
}

void CHyprRenderer::renderSessionLockSurface(SSessionLockSurface* pSurface, CMonitor* pMonitor, timespec* time) {
    SRenderData renderdata = {pMonitor, time, pMonitor->vecPosition.x, pMonitor->vecPosition.y};

    renderdata.blur     = false;
    renderdata.surface  = pSurface->pWlrLockSurface->surface;
    renderdata.decorate = false;
    renderdata.w        = pMonitor->vecSize.x;
    renderdata.h        = pMonitor->vecSize.y;

    wlr_surface_for_each_surface(pSurface->pWlrLockSurface->surface, renderSurface, &renderdata);
}

void CHyprRenderer::renderAllClientsForMonitor(const int& ID, timespec* time) {
    const auto         PMONITOR    = g_pCompositor->getMonitorFromID(ID);
    static auto* const PDIMSPECIAL = &g_pConfigManager->getConfigValuePtr("decoration:dim_special")->floatValue;

    if (!PMONITOR)
        return;

    if (!g_pCompositor->m_sSeat.exclusiveClient && g_pSessionLockManager->isSessionLocked()) {
        // locked with no exclusive, draw only red
        wlr_box boxe = {0, 0, INT16_MAX, INT16_MAX};
        g_pHyprOpenGL->renderRect(&boxe, CColor(1.0, 0.2, 0.2, 1.0));
        return;
    }

    // Render layer surfaces below windows for monitor
    for (auto& ls : PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) {
        renderLayer(ls.get(), PMONITOR, time);
    }
    for (auto& ls : PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) {
        renderLayer(ls.get(), PMONITOR, time);
    }

    // pre window pass
    g_pHyprOpenGL->preWindowPass();

    // if there is a fullscreen window, render it and then do not render anymore.
    // fullscreen window will hide other windows and top layers
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);

    if (PWORKSPACE->m_bHasFullscreenWindow) {
        renderWorkspaceWithFullscreenWindow(PMONITOR, PWORKSPACE, time);
        return;
    }

    CWindow* lastWindow = nullptr;

    // Non-floating main
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->isHidden() && !w->m_bIsMapped && !w->m_bFadingOut)
            continue;

        if (w->m_bIsFloating)
            continue; // floating are in the second pass

        if (g_pCompositor->isWorkspaceSpecial(w->m_iWorkspaceID))
            continue; // special are in the third pass

        if (!shouldRenderWindow(w.get(), PMONITOR))
            continue;

        // render active window after all others of this pass
        if (w.get() == g_pCompositor->m_pLastWindow) {
            lastWindow = w.get();
            continue;
        }

        // render the bad boy
        renderWindow(w.get(), PMONITOR, time, true, RENDER_PASS_MAIN);
    }

    if (lastWindow)
        renderWindow(lastWindow, PMONITOR, time, true, RENDER_PASS_MAIN);

    // Non-floating popup
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->isHidden() && !w->m_bIsMapped && !w->m_bFadingOut)
            continue;

        if (w->m_bIsFloating)
            continue; // floating are in the second pass

        if (g_pCompositor->isWorkspaceSpecial(w->m_iWorkspaceID))
            continue; // special are in the third pass

        if (!shouldRenderWindow(w.get(), PMONITOR))
            continue;

        // render the bad boy
        renderWindow(w.get(), PMONITOR, time, true, RENDER_PASS_POPUP);
    }

    // floating on top
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->isHidden() && !w->m_bIsMapped && !w->m_bFadingOut)
            continue;

        if (!w->m_bIsFloating || w->m_bPinned)
            continue;

        if (g_pCompositor->isWorkspaceSpecial(w->m_iWorkspaceID))
            continue;

        if (!shouldRenderWindow(w.get(), PMONITOR))
            continue;

        // render the bad boy
        renderWindow(w.get(), PMONITOR, time, true, RENDER_PASS_ALL);
    }

    // pinned always above
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->isHidden() && !w->m_bIsMapped && !w->m_bFadingOut)
            continue;

        if (!w->m_bPinned || !w->m_bIsFloating)
            continue;

        if (g_pCompositor->isWorkspaceSpecial(w->m_iWorkspaceID))
            continue;

        if (!shouldRenderWindow(w.get(), PMONITOR))
            continue;

        // render the bad boy
        renderWindow(w.get(), PMONITOR, time, true, RENDER_PASS_ALL);
    }

    // and then special
    bool renderedSpecialBG = false;
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->isHidden() && !w->m_bIsMapped && !w->m_bFadingOut)
            continue;

        if (!g_pCompositor->isWorkspaceSpecial(w->m_iWorkspaceID))
            continue;

        if (!shouldRenderWindow(w.get(), PMONITOR))
            continue;

        if (!renderedSpecialBG) {
            if (*PDIMSPECIAL != 0.f) {
                const auto PSPECIALWORKSPACE = g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID);

                const auto SPECIALANIMPROGRS =
                    PSPECIALWORKSPACE->m_vRenderOffset.isBeingAnimated() ? PSPECIALWORKSPACE->m_vRenderOffset.getCurveValue() : PSPECIALWORKSPACE->m_fAlpha.getCurveValue();

                const bool ANIMOUT = !PMONITOR->specialWorkspaceID;

                wlr_box    monbox = {0, 0, PMONITOR->vecTransformedSize.x, PMONITOR->vecTransformedSize.y};
                g_pHyprOpenGL->renderRect(&monbox, CColor(0, 0, 0, *PDIMSPECIAL * (ANIMOUT ? (1.0 - SPECIALANIMPROGRS) : SPECIALANIMPROGRS)));
            }

            renderedSpecialBG = true;
        }

        // render the bad boy
        renderWindow(w.get(), PMONITOR, time, true, RENDER_PASS_ALL);
    }

    // Render surfaces above windows for monitor
    for (auto& ls : PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        renderLayer(ls.get(), PMONITOR, time);
    }

    // Render IME popups
    for (auto& imep : g_pInputManager->m_sIMERelay.m_lIMEPopups) {
        renderIMEPopup(&imep, PMONITOR, time);
    }

    for (auto& ls : PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
        renderLayer(ls.get(), PMONITOR, time);
    }

    renderDragIcon(PMONITOR, time);

    if (g_pSessionLockManager->isSessionLocked()) {
        const auto PSLS = g_pSessionLockManager->getSessionLockSurfaceForMonitor(PMONITOR->ID);

        if (!PSLS) {
            // locked with no surface, fill with red
            wlr_box boxe = {0, 0, INT16_MAX, INT16_MAX};
            g_pHyprOpenGL->renderRect(&boxe, CColor(1.0, 0.2, 0.2, 1.0));
        } else {
            renderSessionLockSurface(PSLS, PMONITOR, time);
        }
    }
}

void CHyprRenderer::calculateUVForSurface(CWindow* pWindow, wlr_surface* pSurface, bool main) {
    if (!pWindow || !pWindow->m_bIsX11) {
        Vector2D uvTL;
        Vector2D uvBR = Vector2D(1, 1);

        if (pSurface->current.viewport.has_src) {
            // we stretch it to dest. if no dest, to 1,1
            wlr_fbox bufferSource;
            wlr_surface_get_buffer_source_box(pSurface, &bufferSource);

            Vector2D bufferSize = Vector2D(pSurface->buffer->texture->width, pSurface->buffer->texture->height);

            // calculate UV for the basic src_box. Assume dest == size. Scale to dest later
            uvTL = Vector2D(bufferSource.x / bufferSize.x, bufferSource.y / bufferSize.y);
            uvBR = Vector2D((bufferSource.x + bufferSource.width) / bufferSize.x, (bufferSource.y + bufferSource.height) / bufferSize.y);

            if (uvBR.x < 0.01f || uvBR.y < 0.01f) {
                uvTL = Vector2D();
                uvBR = Vector2D(1, 1);
            }
        }

        g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft     = uvTL;
        g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = uvBR;

        if (g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft == Vector2D() && g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight == Vector2D(1, 1)) {
            // No special UV mods needed
            g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
            g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
        }

        if (!main || !pWindow)
            return;

        wlr_box geom;
        wlr_xdg_surface_get_geometry(pWindow->m_uSurface.xdg, &geom);

        // ignore X and Y, adjust uv
        if (geom.x != 0 || geom.y != 0 || geom.width > pWindow->m_vRealSize.goalv().x || geom.height > pWindow->m_vRealSize.goalv().y) {
            const auto XPERC = (double)geom.x / (double)pSurface->current.width;
            const auto YPERC = (double)geom.y / (double)pSurface->current.height;
            const auto WPERC = (double)(geom.x + geom.width) / (double)pSurface->current.width;
            const auto HPERC = (double)(geom.y + geom.height) / (double)pSurface->current.height;

            const auto TOADDTL = Vector2D(XPERC * (uvBR.x - uvTL.x), YPERC * (uvBR.y - uvTL.y));
            uvBR               = uvBR - Vector2D(1.0 - WPERC * (uvBR.x - uvTL.x), 1.0 - HPERC * (uvBR.y - uvTL.y));
            uvTL               = uvTL + TOADDTL;

            if (geom.width > pWindow->m_vRealSize.goalv().x || geom.height > pWindow->m_vRealSize.goalv().y) {
                uvBR.x = uvBR.x * (pWindow->m_vRealSize.goalv().x / geom.width);
                uvBR.y = uvBR.y * (pWindow->m_vRealSize.goalv().y / geom.height);
            }
        }

        g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft     = uvTL;
        g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = uvBR;

        if (g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft == Vector2D() && g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight == Vector2D(1, 1)) {
            // No special UV mods needed
            g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
            g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
        }
    } else {
        g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
        g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
    }
}

void countSubsurfacesIter(wlr_surface* pSurface, int x, int y, void* data) {
    *(int*)data += 1;
}

bool CHyprRenderer::attemptDirectScanout(CMonitor* pMonitor) {
    if (!pMonitor->mirrors.empty() || pMonitor->isMirror() || m_bDirectScanoutBlocked)
        return false; // do not DS if this monitor is being mirrored. Will break the functionality.

    if (!wlr_output_is_direct_scanout_allowed(pMonitor->output))
        return false;

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pMonitor->activeWorkspace);

    if (!PWORKSPACE || !PWORKSPACE->m_bHasFullscreenWindow || g_pInputManager->m_sDrag.drag || g_pCompositor->m_sSeat.exclusiveClient || pMonitor->specialWorkspaceID)
        return false;

    const auto PCANDIDATE = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);

    if (!PCANDIDATE)
        return false; // ????

    if (PCANDIDATE->m_fAlpha.fl() != 1.f || PCANDIDATE->m_fActiveInactiveAlpha.fl() != 1.f || PWORKSPACE->m_fAlpha.fl() != 1.f)
        return false;

    if (PCANDIDATE->m_vRealSize.vec() != pMonitor->vecSize || PCANDIDATE->m_vRealPosition.vec() != pMonitor->vecPosition || PCANDIDATE->m_vRealPosition.isBeingAnimated() ||
        PCANDIDATE->m_vRealSize.isBeingAnimated())
        return false;

    if (!pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY].empty())
        return false;

    for (auto& topls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        if (topls->alpha.fl() != 0.f)
            return false;
    }

    // check if it did not open any subsurfaces or shit
    int surfaceCount = 0;
    if (PCANDIDATE->m_bIsX11) {
        surfaceCount = 1;

        // check opaque
        if (PCANDIDATE->m_uSurface.xwayland->has_alpha)
            return false;
    } else {
        wlr_xdg_surface_for_each_surface(PCANDIDATE->m_uSurface.xdg, countSubsurfacesIter, &surfaceCount);
        wlr_xdg_surface_for_each_popup_surface(PCANDIDATE->m_uSurface.xdg, countSubsurfacesIter, &surfaceCount);

        if (!PCANDIDATE->m_uSurface.xdg->surface->opaque)
            return false;
    }

    if (surfaceCount != 1)
        return false;

    const auto PSURFACE = PCANDIDATE->m_pWLSurface.wlr();

    if (!PSURFACE || PSURFACE->current.scale != pMonitor->output->scale || PSURFACE->current.transform != pMonitor->output->transform)
        return false;

    // finally, we should be GTG.
    wlr_output_attach_buffer(pMonitor->output, &PSURFACE->buffer->base);

    if (!wlr_output_test(pMonitor->output)) {
        return false;
    }

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_surface_send_frame_done(PSURFACE, &now);
    wlr_presentation_surface_sampled_on_output(g_pCompositor->m_sWLRPresentation, PSURFACE, pMonitor->output);

    if (wlr_output_commit(pMonitor->output)) {
        if (!m_pLastScanout) {
            m_pLastScanout = PCANDIDATE;
            Debug::log(LOG, "Entered a direct scanout to %x: \"%s\"", PCANDIDATE, PCANDIDATE->m_szTitle.c_str());
        }
    } else {
        m_pLastScanout = nullptr;
        return false;
    }

    return true;
}

void CHyprRenderer::renderMonitor(CMonitor* pMonitor) {
    static std::chrono::high_resolution_clock::time_point startRender        = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point startRenderOverlay = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point endRenderOverlay   = std::chrono::high_resolution_clock::now();

    static auto* const                                    PDEBUGOVERLAY       = &g_pConfigManager->getConfigValuePtr("debug:overlay")->intValue;
    static auto* const                                    PDAMAGETRACKINGMODE = &g_pConfigManager->getConfigValuePtr("debug:damage_tracking")->intValue;
    static auto* const                                    PDAMAGEBLINK        = &g_pConfigManager->getConfigValuePtr("debug:damage_blink")->intValue;
    static auto* const                                    PNODIRECTSCANOUT    = &g_pConfigManager->getConfigValuePtr("misc:no_direct_scanout")->intValue;
    static auto* const                                    PVFR                = &g_pConfigManager->getConfigValuePtr("misc:vfr")->intValue;

    static int                                            damageBlinkCleanup = 0; // because double-buffered

    if (!*PDAMAGEBLINK)
        damageBlinkCleanup = 0;

    startRender = std::chrono::high_resolution_clock::now();

    if (*PDEBUGOVERLAY == 1) {
        g_pDebugOverlay->frameData(pMonitor);
    }

    if (pMonitor->framesToSkip > 0) {
        pMonitor->framesToSkip -= 1;

        if (!pMonitor->noFrameSchedule)
            g_pCompositor->scheduleFrameForMonitor(pMonitor);
        else {
            Debug::log(LOG, "NoFrameSchedule hit for %s.", pMonitor->szName.c_str());
        }
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(pMonitor->ID);

        if (pMonitor->framesToSkip > 10)
            pMonitor->framesToSkip = 0;
        return;
    }

    // checks //
    if (pMonitor->ID == m_pMostHzMonitor->ID ||
        *PVFR == 1) { // unfortunately with VFR we don't have the guarantee mostHz is going to be updated all the time, so we have to ignore that
        g_pCompositor->sanityCheckWorkspaces();

        g_pConfigManager->dispatchExecOnce(); // We exec-once when at least one monitor starts refreshing, meaning stuff has init'd

        if (g_pConfigManager->m_bWantsMonitorReload)
            g_pConfigManager->performMonitorReload();

        ensureCursorRenderingMode(); // so that the cursor gets hidden/shown if the user requested timeouts
    }
    //       //

    if (pMonitor->scheduledRecalc) {
        pMonitor->scheduledRecalc = false;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(pMonitor->ID);
    }

    // Direct scanout first
    if (!*PNODIRECTSCANOUT) {
        if (attemptDirectScanout(pMonitor)) {
            return;
        } else if (m_pLastScanout) {
            Debug::log(LOG, "Left a direct scanout.");
            m_pLastScanout = nullptr;
        }
    }

    EMIT_HOOK_EVENT("preRender", pMonitor);

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // check the damage
    pixman_region32_t damage;
    bool              hasChanged = pMonitor->output->needs_frame || pixman_region32_not_empty(&pMonitor->damage.current);
    int               bufferAge;

    if (!hasChanged && *PDAMAGETRACKINGMODE != DAMAGE_TRACKING_NONE && pMonitor->forceFullFrames == 0 && damageBlinkCleanup == 0)
        return;

    if (*PDAMAGETRACKINGMODE == -1) {
        Debug::log(CRIT, "Damage tracking mode -1 ????");
        return;
    }

    const bool UNLOCK_SC = g_pHyprRenderer->m_bSoftwareCursorsLocked;
    if (UNLOCK_SC)
        wlr_output_lock_software_cursors(pMonitor->output, true);

    if (!wlr_output_attach_render(pMonitor->output, &bufferAge)) {
        Debug::log(ERR, "Couldn't attach render to display %s ???", pMonitor->szName.c_str());

        if (UNLOCK_SC)
            wlr_output_lock_software_cursors(pMonitor->output, false);

        return;
    }

    pixman_region32_init(&damage);
    wlr_damage_ring_get_buffer_damage(&pMonitor->damage, bufferAge, &damage);

    pMonitor->renderingActive = true;

    // we need to cleanup fading out when rendering the appropriate context
    g_pCompositor->cleanupFadingOut(pMonitor->ID);

    // if we have no tracking or full tracking, invalidate the entire monitor
    if (*PDAMAGETRACKINGMODE == DAMAGE_TRACKING_NONE || *PDAMAGETRACKINGMODE == DAMAGE_TRACKING_MONITOR || pMonitor->forceFullFrames > 0 || damageBlinkCleanup > 0 ||
        pMonitor->isMirror() /* why??? */) {
        pixman_region32_union_rect(&damage, &damage, 0, 0, (int)pMonitor->vecTransformedSize.x * 10, (int)pMonitor->vecTransformedSize.y * 10); // wot?

        pixman_region32_copy(&g_pHyprOpenGL->m_rOriginalDamageRegion, &damage);
    } else {
        static auto* const PBLURENABLED = &g_pConfigManager->getConfigValuePtr("decoration:blur")->intValue;

        // if we use blur we need to expand the damage for proper blurring
        if (*PBLURENABLED == 1) {
            // TODO: can this be optimized?
            static auto* const PBLURSIZE   = &g_pConfigManager->getConfigValuePtr("decoration:blur_size")->intValue;
            static auto* const PBLURPASSES = &g_pConfigManager->getConfigValuePtr("decoration:blur_passes")->intValue;
            const auto         BLURRADIUS =
                *PBLURPASSES > 10 ? pow(2, 15) : std::clamp(*PBLURSIZE, (int64_t)1, (int64_t)40) * pow(2, *PBLURPASSES); // is this 2^pass? I don't know but it works... I think.

            // now, prep the damage, get the extended damage region
            wlr_region_expand(&damage, &damage, BLURRADIUS); // expand for proper blurring

            pixman_region32_copy(&g_pHyprOpenGL->m_rOriginalDamageRegion, &damage);

            wlr_region_expand(&damage, &damage, BLURRADIUS); // expand for proper blurring 2
        } else {
            pixman_region32_copy(&g_pHyprOpenGL->m_rOriginalDamageRegion, &damage);
        }
    }

    if (pMonitor->forceFullFrames > 0) {
        pMonitor->forceFullFrames -= 1;
        if (pMonitor->forceFullFrames > 10)
            pMonitor->forceFullFrames = 0;
    }

    // TODO: this is getting called with extents being 0,0,0,0 should it be?
    // potentially can save on resources.

    g_pHyprOpenGL->begin(pMonitor, &damage);

    if (pMonitor->isMirror()) {
        g_pHyprOpenGL->renderMirrored();
    } else {
        g_pHyprOpenGL->clear(CColor(17.0 / 255.0, 17.0 / 255.0, 17.0 / 255.0, 1.0));
        g_pHyprOpenGL->clearWithTex(); // will apply the hypr "wallpaper"

        renderAllClientsForMonitor(pMonitor->ID, &now);

        if (pMonitor == g_pCompositor->m_pLastMonitor) {
            g_pHyprNotificationOverlay->draw(pMonitor);
            g_pHyprError->draw();
        }

        // for drawing the debug overlay
        if (pMonitor == g_pCompositor->m_vMonitors.front().get() && *PDEBUGOVERLAY == 1) {
            startRenderOverlay = std::chrono::high_resolution_clock::now();
            g_pDebugOverlay->draw();
            endRenderOverlay = std::chrono::high_resolution_clock::now();
        }

        if (*PDAMAGEBLINK && damageBlinkCleanup == 0) {
            wlr_box monrect = {0, 0, pMonitor->vecTransformedSize.x, pMonitor->vecTransformedSize.y};
            g_pHyprOpenGL->renderRect(&monrect, CColor(1.0, 0.0, 1.0, 100.0 / 255.0), 0);
            damageBlinkCleanup = 1;
        } else if (*PDAMAGEBLINK) {
            damageBlinkCleanup++;
            if (damageBlinkCleanup > 3)
                damageBlinkCleanup = 0;
        }

        if (wlr_renderer_begin(g_pCompositor->m_sWLRRenderer, pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y)) {
            wlr_output_render_software_cursors(pMonitor->output, NULL);
            wlr_renderer_end(g_pCompositor->m_sWLRRenderer);
        }
    }

    g_pHyprOpenGL->end();

    // calc frame damage
    pixman_region32_t frameDamage;
    pixman_region32_init(&frameDamage);

    const auto TRANSFORM = wlr_output_transform_invert(pMonitor->output->transform);
    wlr_region_transform(&frameDamage, &g_pHyprOpenGL->m_rOriginalDamageRegion, TRANSFORM, (int)pMonitor->vecTransformedSize.x, (int)pMonitor->vecTransformedSize.y);

    if (*PDAMAGETRACKINGMODE == DAMAGE_TRACKING_NONE || *PDAMAGETRACKINGMODE == DAMAGE_TRACKING_MONITOR)
        pixman_region32_union_rect(&frameDamage, &frameDamage, 0, 0, (int)pMonitor->vecTransformedSize.x, (int)pMonitor->vecTransformedSize.y);

    if (*PDAMAGEBLINK)
        pixman_region32_union(&frameDamage, &frameDamage, &damage);

    wlr_output_set_damage(pMonitor->output, &frameDamage);

    if (!pMonitor->mirrors.empty())
        g_pHyprRenderer->damageMirrorsWith(pMonitor, &frameDamage);

    pixman_region32_fini(&frameDamage);

    pMonitor->renderingActive = false;

    wlr_damage_ring_rotate(&pMonitor->damage);

    if (!wlr_output_commit(pMonitor->output)) {
        pixman_region32_fini(&damage);

        if (UNLOCK_SC)
            wlr_output_lock_software_cursors(pMonitor->output, false);

        return;
    }

    g_pProtocolManager->m_pScreencopyProtocolManager->onRenderEnd(pMonitor);
    pixman_region32_fini(&damage);

    if (UNLOCK_SC)
        wlr_output_lock_software_cursors(pMonitor->output, false);

    if (*PDAMAGEBLINK || *PVFR == 0 || pMonitor->pendingFrame)
        g_pCompositor->scheduleFrameForMonitor(pMonitor);

    pMonitor->pendingFrame = false;

    const float µs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - startRender).count() / 1000.f;
    g_pDebugOverlay->renderData(pMonitor, µs);

    if (*PDEBUGOVERLAY == 1) {
        if (pMonitor == g_pCompositor->m_vMonitors.front().get()) {
            const float µsNoOverlay = µs - std::chrono::duration_cast<std::chrono::nanoseconds>(endRenderOverlay - startRenderOverlay).count() / 1000.f;
            g_pDebugOverlay->renderDataNoOverlay(pMonitor, µsNoOverlay);
        } else {
            g_pDebugOverlay->renderDataNoOverlay(pMonitor, µs);
        }
    }
}

void CHyprRenderer::setWindowScanoutMode(CWindow* pWindow) {
    if (!g_pCompositor->m_sWLRLinuxDMABuf || g_pSessionLockManager->isSessionLocked())
        return;

    if (!pWindow->m_bIsFullscreen) {
        wlr_linux_dmabuf_v1_set_surface_feedback(g_pCompositor->m_sWLRLinuxDMABuf, pWindow->m_pWLSurface.wlr(), nullptr);
        Debug::log(LOG, "Scanout mode OFF set for %x", pWindow);
        return;
    }

    const auto                                      PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    const wlr_linux_dmabuf_feedback_v1_init_options INIT_OPTIONS = {
        .main_renderer          = g_pCompositor->m_sWLRRenderer,
        .scanout_primary_output = PMONITOR->output,
    };

    wlr_linux_dmabuf_feedback_v1 feedback = {0};

    if (!wlr_linux_dmabuf_feedback_v1_init_with_options(&feedback, &INIT_OPTIONS))
        return;

    wlr_linux_dmabuf_v1_set_surface_feedback(g_pCompositor->m_sWLRLinuxDMABuf, pWindow->m_pWLSurface.wlr(), &feedback);
    wlr_linux_dmabuf_feedback_v1_finish(&feedback);

    Debug::log(LOG, "Scanout mode ON set for %x", pWindow);
}

void CHyprRenderer::outputMgrApplyTest(wlr_output_configuration_v1* config, bool test) {
    wlr_output_configuration_head_v1* head;
    bool                              noError = true;

    wl_list_for_each(head, &config->heads, link) {

        std::string commandForCfg = "";
        const auto  OUTPUT        = head->state.output;

        commandForCfg += std::string(OUTPUT->name) + ",";

        if (!head->state.enabled) {
            commandForCfg += "disabled";
            if (!test)
                g_pConfigManager->parseKeyword("monitor", commandForCfg, true);
            continue;
        }

        wlr_output_enable(OUTPUT, head->state.enabled);

        if (head->state.mode)
            commandForCfg +=
                std::to_string(head->state.mode->width) + "x" + std::to_string(head->state.mode->height) + "@" + std::to_string(head->state.mode->refresh / 1000.f) + ",";
        else
            commandForCfg += std::to_string(head->state.custom_mode.width) + "x" + std::to_string(head->state.custom_mode.height) + "@" +
                std::to_string(head->state.custom_mode.refresh / 1000.f) + ",";

        commandForCfg += std::to_string(head->state.x) + "x" + std::to_string(head->state.y) + "," + std::to_string(head->state.scale);

        if (!test) {
            g_pConfigManager->parseKeyword("monitor", commandForCfg, true);

            std::string transformStr = std::string(OUTPUT->name) + ",transform," + std::to_string((int)OUTPUT->transform);

            const auto  PMONITOR = g_pCompositor->getMonitorFromName(OUTPUT->name);

            if (!PMONITOR || OUTPUT->transform != PMONITOR->transform)
                g_pConfigManager->parseKeyword("monitor", transformStr);
        }

        noError = wlr_output_test(OUTPUT);

        if (!noError)
            break;
    }

    if (!test)
        g_pConfigManager->m_bWantsMonitorReload = true; // for monitor keywords

    if (noError)
        wlr_output_configuration_v1_send_succeeded(config);
    else
        wlr_output_configuration_v1_send_failed(config);
    wlr_output_configuration_v1_destroy(config);

    Debug::log(LOG, "OutputMgr Applied/Tested.");
}

// taken from Sway.
// this is just too much of a spaghetti for me to understand
void apply_exclusive(struct wlr_box* usable_area, uint32_t anchor, int32_t exclusive, int32_t margin_top, int32_t margin_right, int32_t margin_bottom, int32_t margin_left) {
    if (exclusive <= 0) {
        return;
    }
    struct {
        uint32_t singular_anchor;
        uint32_t anchor_triplet;
        int*     positive_axis;
        int*     negative_axis;
        int      margin;
    } edges[] = {
        // Top
        {
            .singular_anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP,
            .anchor_triplet  = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP,
            .positive_axis   = &usable_area->y,
            .negative_axis   = &usable_area->height,
            .margin          = margin_top,
        },
        // Bottom
        {
            .singular_anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
            .anchor_triplet  = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
            .positive_axis   = NULL,
            .negative_axis   = &usable_area->height,
            .margin          = margin_bottom,
        },
        // Left
        {
            .singular_anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT,
            .anchor_triplet  = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
            .positive_axis   = &usable_area->x,
            .negative_axis   = &usable_area->width,
            .margin          = margin_left,
        },
        // Right
        {
            .singular_anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
            .anchor_triplet  = ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
            .positive_axis   = NULL,
            .negative_axis   = &usable_area->width,
            .margin          = margin_right,
        },
    };
    for (size_t i = 0; i < sizeof(edges) / sizeof(edges[0]); ++i) {
        if ((anchor == edges[i].singular_anchor || anchor == edges[i].anchor_triplet) && exclusive + edges[i].margin > 0) {
            if (edges[i].positive_axis) {
                *edges[i].positive_axis += exclusive + edges[i].margin;
            }
            if (edges[i].negative_axis) {
                *edges[i].negative_axis -= exclusive + edges[i].margin;
            }
            break;
        }
    }
}

void CHyprRenderer::arrangeLayerArray(CMonitor* pMonitor, const std::vector<std::unique_ptr<SLayerSurface>>& layerSurfaces, bool exclusiveZone, wlr_box* usableArea) {
    wlr_box full_area = {pMonitor->vecPosition.x, pMonitor->vecPosition.y, pMonitor->vecSize.x, pMonitor->vecSize.y};

    for (auto& ls : layerSurfaces) {
        if (ls->fadingOut || ls->readyToDelete || !ls->layerSurface || ls->noProcess)
            continue;

        const auto PLAYER = ls->layerSurface;
        const auto PSTATE = &PLAYER->current;
        if (exclusiveZone != (PSTATE->exclusive_zone > 0)) {
            continue;
        }

        wlr_box bounds;
        if (PSTATE->exclusive_zone == -1) {
            bounds = full_area;
        } else {
            bounds = *usableArea;
        }

        wlr_box box = {.width = PSTATE->desired_width, .height = PSTATE->desired_height};
        // Horizontal axis
        const uint32_t both_horiz = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
        if (box.width == 0) {
            box.x = bounds.x;
        } else if ((PSTATE->anchor & both_horiz) == both_horiz) {
            box.x = bounds.x + ((bounds.width / 2) - (box.width / 2));
        } else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT)) {
            box.x = bounds.x;
        } else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
            box.x = bounds.x + (bounds.width - box.width);
        } else {
            box.x = bounds.x + ((bounds.width / 2) - (box.width / 2));
        }
        // Vertical axis
        const uint32_t both_vert = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
        if (box.height == 0) {
            box.y = bounds.y;
        } else if ((PSTATE->anchor & both_vert) == both_vert) {
            box.y = bounds.y + ((bounds.height / 2) - (box.height / 2));
        } else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP)) {
            box.y = bounds.y;
        } else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)) {
            box.y = bounds.y + (bounds.height - box.height);
        } else {
            box.y = bounds.y + ((bounds.height / 2) - (box.height / 2));
        }
        // Margin
        if (box.width == 0) {
            box.x += PSTATE->margin.left;
            box.width = bounds.width - (PSTATE->margin.left + PSTATE->margin.right);
        } else if ((PSTATE->anchor & both_horiz) == both_horiz) {
            // don't apply margins
        } else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT)) {
            box.x += PSTATE->margin.left;
        } else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
            box.x -= PSTATE->margin.right;
        }
        if (box.height == 0) {
            box.y += PSTATE->margin.top;
            box.height = bounds.height - (PSTATE->margin.top + PSTATE->margin.bottom);
        } else if ((PSTATE->anchor & both_vert) == both_vert) {
            // don't apply margins
        } else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP)) {
            box.y += PSTATE->margin.top;
        } else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)) {
            box.y -= PSTATE->margin.bottom;
        }
        if (box.width <= 0 || box.height <= 0) {
            Debug::log(ERR, "LayerSurface %x has a negative/zero w/h???", ls.get());
            continue;
        }
        // Apply
        ls->geometry = box;

        apply_exclusive(usableArea, PSTATE->anchor, PSTATE->exclusive_zone, PSTATE->margin.top, PSTATE->margin.right, PSTATE->margin.bottom, PSTATE->margin.left);

        wlr_layer_surface_v1_configure(ls->layerSurface, box.width, box.height);

        Debug::log(LOG, "LayerSurface %x arranged: x: %i y: %i w: %i h: %i with margins: t: %i l: %i r: %i b: %i", &ls, box.x, box.y, box.width, box.height, PSTATE->margin.top,
                   PSTATE->margin.left, PSTATE->margin.right, PSTATE->margin.bottom);
    }
}

void CHyprRenderer::arrangeLayersForMonitor(const int& monitor) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(monitor);

    if (!PMONITOR)
        return;

    // Reset the reserved
    PMONITOR->vecReservedBottomRight = Vector2D();
    PMONITOR->vecReservedTopLeft     = Vector2D();

    wlr_box usableArea = {PMONITOR->vecPosition.x, PMONITOR->vecPosition.y, PMONITOR->vecSize.x, PMONITOR->vecSize.y};

    for (auto& la : PMONITOR->m_aLayerSurfaceLayers)
        arrangeLayerArray(PMONITOR, la, true, &usableArea);

    for (auto& la : PMONITOR->m_aLayerSurfaceLayers)
        arrangeLayerArray(PMONITOR, la, false, &usableArea);

    PMONITOR->vecReservedTopLeft     = Vector2D(usableArea.x, usableArea.y) - PMONITOR->vecPosition;
    PMONITOR->vecReservedBottomRight = PMONITOR->vecSize - Vector2D(usableArea.width, usableArea.height) - PMONITOR->vecReservedTopLeft;

    auto ADDITIONALRESERVED = g_pConfigManager->m_mAdditionalReservedAreas.find(PMONITOR->szName);
    if (ADDITIONALRESERVED == g_pConfigManager->m_mAdditionalReservedAreas.end()) {
        ADDITIONALRESERVED = g_pConfigManager->m_mAdditionalReservedAreas.find(""); // glob wildcard
    }

    if (ADDITIONALRESERVED != g_pConfigManager->m_mAdditionalReservedAreas.end()) {
        PMONITOR->vecReservedTopLeft     = PMONITOR->vecReservedTopLeft + Vector2D(ADDITIONALRESERVED->second.left, ADDITIONALRESERVED->second.top);
        PMONITOR->vecReservedBottomRight = PMONITOR->vecReservedBottomRight + Vector2D(ADDITIONALRESERVED->second.right, ADDITIONALRESERVED->second.bottom);
    }

    // damage the monitor if can
    damageMonitor(PMONITOR);

    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(monitor);

    Debug::log(LOG, "Monitor %s layers arranged: reserved: %f %f %f %f", PMONITOR->szName.c_str(), PMONITOR->vecReservedTopLeft.x, PMONITOR->vecReservedTopLeft.y,
               PMONITOR->vecReservedBottomRight.x, PMONITOR->vecReservedBottomRight.y);
}

void CHyprRenderer::damageSurface(wlr_surface* pSurface, double x, double y) {
    if (!pSurface)
        return; // wut?

    if (g_pCompositor->m_bUnsafeState)
        return;

    pixman_region32_t damageBox;
    pixman_region32_init(&damageBox);
    wlr_surface_get_effective_damage(pSurface, &damageBox);

    // schedule frame events
    if (!wl_list_empty(&pSurface->current.frame_callback_list)) {
        g_pCompositor->scheduleFrameForMonitor(g_pCompositor->getMonitorFromVector(Vector2D(x, y)));
    }

    if (!pixman_region32_not_empty(&damageBox)) {
        pixman_region32_fini(&damageBox);
        return;
    }

    pixman_region32_t damageBoxForEach;
    pixman_region32_init(&damageBoxForEach);

    for (auto& m : g_pCompositor->m_vMonitors) {
        if (!m->output)
            continue;

        double lx = 0, ly = 0;
        wlr_output_layout_output_coords(g_pCompositor->m_sWLROutputLayout, m->output, &lx, &ly);

        pixman_region32_copy(&damageBoxForEach, &damageBox);
        pixman_region32_translate(&damageBoxForEach, x - m->vecPosition.x, y - m->vecPosition.y);
        wlr_region_scale(&damageBoxForEach, &damageBoxForEach, m->scale);
        pixman_region32_translate(&damageBoxForEach, lx + m->vecPosition.x, ly + m->vecPosition.y);

        m->addDamage(&damageBoxForEach);
    }

    pixman_region32_fini(&damageBoxForEach);

    static auto* const PLOGDAMAGE = &g_pConfigManager->getConfigValuePtr("debug:log_damage")->intValue;

    if (*PLOGDAMAGE)
        Debug::log(LOG, "Damage: Surface (extents): xy: %d, %d wh: %d, %d", damageBox.extents.x1, damageBox.extents.y1, damageBox.extents.x2 - damageBox.extents.x1,
                   damageBox.extents.y2 - damageBox.extents.y1);

    pixman_region32_fini(&damageBox);
}

void CHyprRenderer::damageWindow(CWindow* pWindow) {
    if (g_pCompositor->m_bUnsafeState)
        return;

    wlr_box damageBox = pWindow->getFullWindowBoundingBox();
    for (auto& m : g_pCompositor->m_vMonitors) {
        wlr_box fixedDamageBox = {damageBox.x - m->vecPosition.x, damageBox.y - m->vecPosition.y, damageBox.width, damageBox.height};
        scaleBox(&fixedDamageBox, m->scale);
        m->addDamage(&fixedDamageBox);
    }

    static auto* const PLOGDAMAGE = &g_pConfigManager->getConfigValuePtr("debug:log_damage")->intValue;

    if (*PLOGDAMAGE)
        Debug::log(LOG, "Damage: Window (%s): xy: %d, %d wh: %d, %d", pWindow->m_szTitle.c_str(), damageBox.x, damageBox.y, damageBox.width, damageBox.height);
}

void CHyprRenderer::damageMonitor(CMonitor* pMonitor) {
    if (g_pCompositor->m_bUnsafeState || pMonitor->isMirror())
        return;

    wlr_box damageBox = {0, 0, INT16_MAX, INT16_MAX};
    pMonitor->addDamage(&damageBox);

    static auto* const PLOGDAMAGE = &g_pConfigManager->getConfigValuePtr("debug:log_damage")->intValue;

    if (*PLOGDAMAGE)
        Debug::log(LOG, "Damage: Monitor %s", pMonitor->szName.c_str());
}

void CHyprRenderer::damageBox(wlr_box* pBox) {
    if (g_pCompositor->m_bUnsafeState)
        return;

    for (auto& m : g_pCompositor->m_vMonitors) {
        if (m->isMirror())
            continue; // don't damage mirrors traditionally

        wlr_box damageBox = {pBox->x - m->vecPosition.x, pBox->y - m->vecPosition.y, pBox->width, pBox->height};
        scaleBox(&damageBox, m->scale);
        m->addDamage(&damageBox);
    }

    static auto* const PLOGDAMAGE = &g_pConfigManager->getConfigValuePtr("debug:log_damage")->intValue;

    if (*PLOGDAMAGE)
        Debug::log(LOG, "Damage: Box: xy: %d, %d wh: %d, %d", pBox->x, pBox->y, pBox->width, pBox->height);
}

void CHyprRenderer::damageBox(const int& x, const int& y, const int& w, const int& h) {
    wlr_box box = {x, y, w, h};
    damageBox(&box);
}

void CHyprRenderer::damageRegion(pixman_region32_t* rg) {
    PIXMAN_DAMAGE_FOREACH(rg) {
        const auto RECT = RECTSARR[i];
        damageBox(RECT.x1, RECT.y1, RECT.x2 - RECT.x1, RECT.y2 - RECT.y1);
    }
}

void CHyprRenderer::damageMirrorsWith(CMonitor* pMonitor, pixman_region32_t* pRegion) {
    for (auto& mirror : pMonitor->mirrors) {
        Vector2D          scale = {mirror->vecSize.x / pMonitor->vecSize.x, mirror->vecSize.y / pMonitor->vecSize.y};

        pixman_region32_t rg;
        pixman_region32_init(&rg);
        pixman_region32_copy(&rg, pRegion);
        wlr_region_scale_xy(&rg, &rg, scale.x, scale.y);
        pMonitor->addDamage(&rg);
        pixman_region32_fini(&rg);

        g_pCompositor->scheduleFrameForMonitor(mirror);
    }
}

void CHyprRenderer::renderDragIcon(CMonitor* pMonitor, timespec* time) {
    if (!(g_pInputManager->m_sDrag.dragIcon && g_pInputManager->m_sDrag.iconMapped && g_pInputManager->m_sDrag.dragIcon->surface))
        return;

    SRenderData renderdata = {pMonitor, time, g_pInputManager->m_sDrag.pos.x, g_pInputManager->m_sDrag.pos.y};
    renderdata.surface     = g_pInputManager->m_sDrag.dragIcon->surface;
    renderdata.w           = g_pInputManager->m_sDrag.dragIcon->surface->current.width;
    renderdata.h           = g_pInputManager->m_sDrag.dragIcon->surface->current.height;

    wlr_surface_for_each_surface(g_pInputManager->m_sDrag.dragIcon->surface, renderSurface, &renderdata);

    wlr_box box = {g_pInputManager->m_sDrag.pos.x - 2, g_pInputManager->m_sDrag.pos.y - 2, g_pInputManager->m_sDrag.dragIcon->surface->current.width + 4,
                   g_pInputManager->m_sDrag.dragIcon->surface->current.height + 4};
    g_pHyprRenderer->damageBox(&box);
}

DAMAGETRACKINGMODES CHyprRenderer::damageTrackingModeFromStr(const std::string& mode) {
    if (mode == "full")
        return DAMAGE_TRACKING_FULL;
    if (mode == "monitor")
        return DAMAGE_TRACKING_MONITOR;
    if (mode == "none")
        return DAMAGE_TRACKING_NONE;

    return DAMAGE_TRACKING_INVALID;
}

bool CHyprRenderer::applyMonitorRule(CMonitor* pMonitor, SMonitorRule* pMonitorRule, bool force) {

    Debug::log(LOG, "Applying monitor rule for %s", pMonitor->szName.c_str());

    // if it's disabled, disable and ignore
    if (pMonitorRule->disabled) {

        if (pMonitor->m_bEnabled)
            pMonitor->onDisconnect();

        return true;
    }

    // don't touch VR headsets
    if (pMonitor->output->non_desktop)
        return true;

    if (!pMonitor->m_bEnabled) {
        pMonitor->onConnect(true); // enable it.
        force = true;
    }

    // Check if the rule isn't already applied
    if (!force && DELTALESSTHAN(pMonitor->vecPixelSize.x, pMonitorRule->resolution.x, 1) && DELTALESSTHAN(pMonitor->vecPixelSize.y, pMonitorRule->resolution.y, 1) &&
        DELTALESSTHAN(pMonitor->refreshRate, pMonitorRule->refreshRate, 1) && pMonitor->scale == pMonitorRule->scale &&
        ((DELTALESSTHAN(pMonitor->vecPosition.x, pMonitorRule->offset.x, 1) && DELTALESSTHAN(pMonitor->vecPosition.y, pMonitorRule->offset.y, 1)) ||
         pMonitorRule->offset == Vector2D(-1, -1)) &&
        pMonitor->transform == pMonitorRule->transform && pMonitorRule->enable10bit == pMonitor->enabled10bit) {

        Debug::log(LOG, "Not applying a new rule to %s because it's already applied!", pMonitor->szName.c_str());
        return true;
    }

    if (pMonitorRule->scale > 0.1) {
        wlr_output_set_scale(pMonitor->output, pMonitorRule->scale);
        pMonitor->scale = pMonitorRule->scale;
    } else {
        const auto DEFAULTSCALE = pMonitor->getDefaultScale();
        wlr_output_set_scale(pMonitor->output, DEFAULTSCALE);
        pMonitor->scale = DEFAULTSCALE;
    }

    wlr_output_set_transform(pMonitor->output, pMonitorRule->transform);
    pMonitor->transform = pMonitorRule->transform;

    // loop over modes and choose an appropriate one.
    if (pMonitorRule->resolution != Vector2D() && pMonitorRule->resolution != Vector2D(-1, -1) && pMonitorRule->resolution != Vector2D(-1, -2)) {
        if (!wl_list_empty(&pMonitor->output->modes)) {
            wlr_output_mode* mode;
            bool             found = false;

            wl_list_for_each(mode, &pMonitor->output->modes, link) {
                // if delta of refresh rate, w and h chosen and mode is < 1 we accept it
                if (DELTALESSTHAN(mode->width, pMonitorRule->resolution.x, 1) && DELTALESSTHAN(mode->height, pMonitorRule->resolution.y, 1) &&
                    DELTALESSTHAN(mode->refresh / 1000.f, pMonitorRule->refreshRate, 1)) {
                    wlr_output_set_mode(pMonitor->output, mode);

                    if (!wlr_output_test(pMonitor->output)) {
                        Debug::log(LOG, "Monitor %s: REJECTED available mode: %ix%i@%2f!", pMonitor->output->name, (int)pMonitorRule->resolution.x, (int)pMonitorRule->resolution.y,
                                   (float)pMonitorRule->refreshRate, mode->width, mode->height, mode->refresh / 1000.f);
                        continue;
                    }

                    Debug::log(LOG, "Monitor %s: requested %ix%i@%2f, found available mode: %ix%i@%imHz, applying.", pMonitor->output->name, (int)pMonitorRule->resolution.x,
                               (int)pMonitorRule->resolution.y, (float)pMonitorRule->refreshRate, mode->width, mode->height, mode->refresh);

                    found = true;

                    pMonitor->refreshRate = mode->refresh / 1000.f;
                    pMonitor->vecSize     = Vector2D(mode->width, mode->height);

                    break;
                }
            }

            if (!found) {
                wlr_output_set_custom_mode(pMonitor->output, (int)pMonitorRule->resolution.x, (int)pMonitorRule->resolution.y, (int)pMonitorRule->refreshRate * 1000);
                pMonitor->vecSize     = pMonitorRule->resolution;
                pMonitor->refreshRate = pMonitorRule->refreshRate;

                if (!wlr_output_test(pMonitor->output)) {
                    Debug::log(ERR, "Custom resolution FAILED, falling back to preferred");

                    const auto PREFERREDMODE = wlr_output_preferred_mode(pMonitor->output);

                    if (!PREFERREDMODE) {
                        Debug::log(ERR, "Monitor %s has NO PREFERRED MODE, and an INVALID one was requested: %ix%i@%2f", (int)pMonitorRule->resolution.x,
                                   (int)pMonitorRule->resolution.y, (float)pMonitorRule->refreshRate);
                        return true;
                    }

                    // Preferred is valid
                    wlr_output_set_mode(pMonitor->output, PREFERREDMODE);

                    Debug::log(ERR, "Monitor %s got an invalid requested mode: %ix%i@%2f, using the preferred one instead: %ix%i@%2f", pMonitor->output->name,
                               (int)pMonitorRule->resolution.x, (int)pMonitorRule->resolution.y, (float)pMonitorRule->refreshRate, PREFERREDMODE->width, PREFERREDMODE->height,
                               PREFERREDMODE->refresh / 1000.f);

                    pMonitor->refreshRate = PREFERREDMODE->refresh / 1000.f;
                    pMonitor->vecSize     = Vector2D(PREFERREDMODE->width, PREFERREDMODE->height);
                } else {
                    Debug::log(LOG, "Set a custom mode %ix%i@%2f (mode not found in monitor modes)", (int)pMonitorRule->resolution.x, (int)pMonitorRule->resolution.y,
                               (float)pMonitorRule->refreshRate);
                }
            }
        } else {
            wlr_output_set_custom_mode(pMonitor->output, (int)pMonitorRule->resolution.x, (int)pMonitorRule->resolution.y, (int)pMonitorRule->refreshRate * 1000);
            pMonitor->vecSize     = pMonitorRule->resolution;
            pMonitor->refreshRate = pMonitorRule->refreshRate;

            if (!wlr_output_test(pMonitor->output)) {
                Debug::log(ERR, "Custom resolution FAILED, falling back to preferred");

                const auto PREFERREDMODE = wlr_output_preferred_mode(pMonitor->output);

                if (!PREFERREDMODE) {
                    Debug::log(ERR, "Monitor %s has NO PREFERRED MODE, and an INVALID one was requested: %ix%i@%2f", (int)pMonitorRule->resolution.x,
                               (int)pMonitorRule->resolution.y, (float)pMonitorRule->refreshRate);
                    return true;
                }

                // Preferred is valid
                wlr_output_set_mode(pMonitor->output, PREFERREDMODE);

                Debug::log(ERR, "Monitor %s got an invalid requested mode: %ix%i@%2f, using the preferred one instead: %ix%i@%2f", pMonitor->output->name,
                           (int)pMonitorRule->resolution.x, (int)pMonitorRule->resolution.y, (float)pMonitorRule->refreshRate, PREFERREDMODE->width, PREFERREDMODE->height,
                           PREFERREDMODE->refresh / 1000.f);

                pMonitor->refreshRate = PREFERREDMODE->refresh / 1000.f;
                pMonitor->vecSize     = Vector2D(PREFERREDMODE->width, PREFERREDMODE->height);
            } else {
                Debug::log(LOG, "Set a custom mode %ix%i@%2f (mode not found in monitor modes)", (int)pMonitorRule->resolution.x, (int)pMonitorRule->resolution.y,
                           (float)pMonitorRule->refreshRate);
            }
        }
    } else if (pMonitorRule->resolution != Vector2D()) {
        if (!wl_list_empty(&pMonitor->output->modes)) {
            wlr_output_mode* mode;
            float            currentWidth   = 0;
            float            currentHeight  = 0;
            float            currentRefresh = 0;
            bool             success        = false;

            //(-1,-1) indicates a preference to refreshrate over resolution, (-1,-2) preference to resolution
            if (pMonitorRule->resolution == Vector2D(-1, -1)) {
                wl_list_for_each(mode, &pMonitor->output->modes, link) {
                    if ((mode->width >= currentWidth && mode->height >= currentHeight && mode->refresh >= (currentRefresh - 1000.f)) || mode->refresh > (currentRefresh + 3000.f)) {
                        wlr_output_set_mode(pMonitor->output, mode);
                        if (wlr_output_test(pMonitor->output)) {
                            currentWidth   = mode->width;
                            currentHeight  = mode->height;
                            currentRefresh = mode->refresh;
                            success        = true;
                        }
                    }
                }
            } else {
                wl_list_for_each(mode, &pMonitor->output->modes, link) {
                    if ((mode->width >= currentWidth && mode->height >= currentHeight && mode->refresh >= (currentRefresh - 1000.f)) ||
                        (mode->width > currentWidth && mode->height > currentHeight)) {
                        wlr_output_set_mode(pMonitor->output, mode);
                        if (wlr_output_test(pMonitor->output)) {
                            currentWidth   = mode->width;
                            currentHeight  = mode->height;
                            currentRefresh = mode->refresh;
                            success        = true;
                        }
                    }
                }
            }

            if (!success) {
                Debug::log(LOG, "Monitor %s: REJECTED mode: %ix%i@%2f! Falling back to preferred.", pMonitor->output->name, (int)pMonitorRule->resolution.x,
                           (int)pMonitorRule->resolution.y, (float)pMonitorRule->refreshRate, mode->width, mode->height, mode->refresh / 1000.f);

                const auto PREFERREDMODE = wlr_output_preferred_mode(pMonitor->output);

                if (!PREFERREDMODE) {
                    Debug::log(ERR, "Monitor %s has NO PREFERRED MODE, and an INVALID one was requested: %ix%i@%2f", (int)pMonitorRule->resolution.x,
                               (int)pMonitorRule->resolution.y, (float)pMonitorRule->refreshRate);
                    return true;
                }

                // Preferred is valid
                wlr_output_set_mode(pMonitor->output, PREFERREDMODE);

                Debug::log(ERR, "Monitor %s got an invalid requested mode: %ix%i@%2f, using the preferred one instead: %ix%i@%2f", pMonitor->output->name,
                           (int)pMonitorRule->resolution.x, (int)pMonitorRule->resolution.y, (float)pMonitorRule->refreshRate, PREFERREDMODE->width, PREFERREDMODE->height,
                           PREFERREDMODE->refresh / 1000.f);

                pMonitor->refreshRate = PREFERREDMODE->refresh / 1000.f;
                pMonitor->vecSize     = Vector2D(PREFERREDMODE->width, PREFERREDMODE->height);
            } else {

                Debug::log(LOG, "Monitor %s: Applying highest mode %ix%i@%2f.", pMonitor->output->name, (int)currentWidth, (int)currentHeight, (int)currentRefresh / 1000.f,
                           mode->width, mode->height, mode->refresh / 1000.f);

                pMonitor->refreshRate = currentRefresh / 1000.f;
                pMonitor->vecSize     = Vector2D(currentWidth, currentHeight);
            }
        }
    } else {
        const auto PREFERREDMODE = wlr_output_preferred_mode(pMonitor->output);

        if (!PREFERREDMODE) {
            Debug::log(ERR, "Monitor %s has NO PREFERRED MODE", (int)pMonitorRule->resolution.x, (int)pMonitorRule->resolution.y, (float)pMonitorRule->refreshRate);

            if (!wl_list_empty(&pMonitor->output->modes)) {
                wlr_output_mode* mode;

                wl_list_for_each(mode, &pMonitor->output->modes, link) {
                    wlr_output_set_mode(pMonitor->output, mode);

                    if (!wlr_output_test(pMonitor->output)) {
                        Debug::log(LOG, "Monitor %s: REJECTED available mode: %ix%i@%2f!", pMonitor->output->name, (int)pMonitorRule->resolution.x, (int)pMonitorRule->resolution.y,
                                   (float)pMonitorRule->refreshRate, mode->width, mode->height, mode->refresh / 1000.f);
                        continue;
                    }

                    Debug::log(LOG, "Monitor %s: requested %ix%i@%2f, found available mode: %ix%i@%imHz, applying.", pMonitor->output->name, (int)pMonitorRule->resolution.x,
                               (int)pMonitorRule->resolution.y, (float)pMonitorRule->refreshRate, mode->width, mode->height, mode->refresh);

                    pMonitor->refreshRate = mode->refresh / 1000.f;
                    pMonitor->vecSize     = Vector2D(mode->width, mode->height);

                    break;
                }
            }
        } else {
            // Preferred is valid
            wlr_output_set_mode(pMonitor->output, PREFERREDMODE);

            pMonitor->vecSize     = Vector2D(PREFERREDMODE->width, PREFERREDMODE->height);
            pMonitor->refreshRate = PREFERREDMODE->refresh / 1000.f;

            Debug::log(LOG, "Setting preferred mode for %s", pMonitor->output->name);
        }
    }

    pMonitor->vrrActive = pMonitor->output->pending.adaptive_sync_enabled; // disabled here, will be tested in CConfigManager::ensureVRR()

    pMonitor->vecPixelSize = pMonitor->vecSize;

    if (pMonitorRule->enable10bit) {
        // try 10b RGB
        wlr_output_set_render_format(pMonitor->output, DRM_FORMAT_XRGB2101010);
        pMonitor->enabled10bit = true;

        if (!wlr_output_test(pMonitor->output)) {
            Debug::log(ERR, "Output %s -> 10 bit enabled, but failed format DRM_FORMAT_XRGB2101010. Trying BGR.", pMonitor->output->name);

            wlr_output_set_render_format(pMonitor->output, DRM_FORMAT_XBGR2101010);

            if (!wlr_output_test(pMonitor->output)) {
                Debug::log(ERR, "Output %s -> 10 bit enabled, but failed format DRM_FORMAT_XBGR2101010. Falling back to 8 bit.", pMonitor->output->name);

                wlr_output_set_render_format(pMonitor->output, DRM_FORMAT_XRGB8888);
            } else {
                Debug::log(LOG, "10bit format DRM_FORMAT_XBGR2101010 succeeded for output %s", pMonitor->output->name);
            }
        } else {
            Debug::log(LOG, "10bit format DRM_FORMAT_XRGB2101010 succeeded for output %s", pMonitor->output->name);
        }
    } else {
        wlr_output_set_render_format(pMonitor->output, DRM_FORMAT_XRGB8888);
        pMonitor->enabled10bit = false;
    }

    if (!wlr_output_commit(pMonitor->output)) {
        Debug::log(ERR, "Couldn't commit output named %s", pMonitor->output->name);
    }

    int x, y;
    wlr_output_transformed_resolution(pMonitor->output, &x, &y);
    pMonitor->vecSize            = (Vector2D(x, y) / pMonitor->scale).floor();
    pMonitor->vecTransformedSize = Vector2D(x, y);

    if (pMonitor->createdByUser) {
        wlr_box transformedBox = {0, 0, (int)pMonitor->vecTransformedSize.x, (int)pMonitor->vecTransformedSize.y};
        wlr_box_transform(&transformedBox, &transformedBox, wlr_output_transform_invert(pMonitor->output->transform), (int)pMonitor->vecTransformedSize.x,
                          (int)pMonitor->vecTransformedSize.y);

        pMonitor->vecPixelSize = Vector2D(transformedBox.width, transformedBox.height);
    }

    if (pMonitorRule->offset == Vector2D(-1, -1) && pMonitor->vecPosition == Vector2D(-1, -1)) {
        // let's find manually a sensible position for it, to the right.
        Vector2D finalPos;

        for (auto& m : g_pCompositor->m_vMonitors) {
            if (m->ID == pMonitor->ID)
                continue;

            if (m->vecPosition.x + std::ceil(m->vecSize.x) > finalPos.x) {
                finalPos.x = m->vecPosition.x + std::ceil(m->vecSize.x);
            }
        }

        pMonitor->vecPosition = finalPos;
    } else if (pMonitorRule->offset != Vector2D(-1, -1)) {
        pMonitor->vecPosition = pMonitorRule->offset;
    }

    wlr_output_enable(pMonitor->output, 1);

    // update renderer (here because it will call rollback, so we cannot do this before committing)
    g_pHyprOpenGL->destroyMonitorResources(pMonitor);

    // updato wlroots
    if (!pMonitor->isMirror())
        wlr_output_layout_add(g_pCompositor->m_sWLROutputLayout, pMonitor->output, (int)pMonitor->vecPosition.x, (int)pMonitor->vecPosition.y);

    wlr_damage_ring_set_bounds(&pMonitor->damage, pMonitor->vecTransformedSize.x, pMonitor->vecTransformedSize.y);

    // updato us
    arrangeLayersForMonitor(pMonitor->ID);

    // frame skip
    pMonitor->framesToSkip = 1;

    // reload to fix mirrors
    g_pConfigManager->m_bWantsMonitorReload = true;

    Debug::log(LOG, "Monitor %s data dump: res %ix%i@%.2fHz, scale %.2f, transform %i, pos %ix%i, 10b %i", pMonitor->szName.c_str(), (int)pMonitor->vecPixelSize.x,
               (int)pMonitor->vecPixelSize.y, pMonitor->refreshRate, pMonitor->scale, (int)pMonitor->transform, (int)pMonitor->vecPosition.x, (int)pMonitor->vecPosition.y,
               (int)pMonitor->enabled10bit);

    return true;
}

void CHyprRenderer::ensureCursorRenderingMode() {
    static auto* const PCURSORTIMEOUT = &g_pConfigManager->getConfigValuePtr("general:cursor_inactive_timeout")->intValue;
    static auto* const PHIDEONTOUCH   = &g_pConfigManager->getConfigValuePtr("misc:hide_cursor_on_touch")->intValue;

    const auto         PASSEDCURSORSECONDS = g_pInputManager->m_tmrLastCursorMovement.getSeconds();

    if (*PCURSORTIMEOUT > 0 || *PHIDEONTOUCH) {
        const bool HIDE = (*PCURSORTIMEOUT > 0 && *PCURSORTIMEOUT < PASSEDCURSORSECONDS) || (g_pInputManager->m_bLastInputTouch && *PHIDEONTOUCH);

        if (HIDE && m_bHasARenderedCursor) {
            m_bHasARenderedCursor = false;

            wlr_cursor_set_surface(g_pCompositor->m_sWLRCursor, nullptr, 0, 0); // hide

            Debug::log(LOG, "Hiding the cursor (timeout)");

            for (auto& m : g_pCompositor->m_vMonitors)
                g_pHyprRenderer->damageMonitor(m.get()); // TODO: maybe just damage the cursor area?
        } else if (!HIDE && !m_bHasARenderedCursor) {
            m_bHasARenderedCursor = true;

            if (!m_bWindowRequestedCursorHide)
                wlr_xcursor_manager_set_cursor_image(g_pCompositor->m_sWLRXCursorMgr, "left_ptr", g_pCompositor->m_sWLRCursor);

            Debug::log(LOG, "Showing the cursor (timeout)");

            for (auto& m : g_pCompositor->m_vMonitors)
                g_pHyprRenderer->damageMonitor(m.get()); // TODO: maybe just damage the cursor area?
        }
    } else {
        m_bHasARenderedCursor = true;
    }
}

bool CHyprRenderer::shouldRenderCursor() {
    return m_bHasARenderedCursor;
}

std::tuple<float, float, float> CHyprRenderer::getRenderTimes(CMonitor* pMonitor) {
    const auto POVERLAY = &g_pDebugOverlay->m_mMonitorOverlays[pMonitor];

    float      avgRenderTime = 0;
    float      maxRenderTime = 0;
    float      minRenderTime = 9999;
    for (auto& rt : POVERLAY->m_dLastRenderTimes) {
        if (rt > maxRenderTime)
            maxRenderTime = rt;
        if (rt < minRenderTime)
            minRenderTime = rt;
        avgRenderTime += rt;
    }
    avgRenderTime /= POVERLAY->m_dLastRenderTimes.size() == 0 ? 1 : POVERLAY->m_dLastRenderTimes.size();

    return std::make_tuple<>(avgRenderTime, maxRenderTime, minRenderTime);
}

static int handleCrashLoop(void* data) {

    g_pHyprNotificationOverlay->addNotification("Hyprland will crash in " + std::to_string(10 - (int)(g_pHyprRenderer->m_fCrashingDistort * 2.f)) + "s.", CColor(0), 5000,
                                                ICON_INFO);

    g_pHyprRenderer->m_fCrashingDistort += 0.5f;

    if (g_pHyprRenderer->m_fCrashingDistort >= 5.5f)
        *((int*)nullptr) = 1337;

    wl_event_source_timer_update(g_pHyprRenderer->m_pCrashingLoop, 1000);

    return 1;
}

void CHyprRenderer::initiateManualCrash() {
    g_pHyprNotificationOverlay->addNotification("Manual crash initiated. Farewell...", CColor(0), 5000, ICON_INFO);

    m_pCrashingLoop = wl_event_loop_add_timer(g_pCompositor->m_sWLEventLoop, handleCrashLoop, nullptr);
    wl_event_source_timer_update(m_pCrashingLoop, 1000);

    m_bCrashingInProgress = true;
    m_fCrashingDistort    = 0.5;

    g_pHyprOpenGL->m_tGlobalTimer.reset();

    g_pConfigManager->setInt("debug:damage_tracking", 0);
}