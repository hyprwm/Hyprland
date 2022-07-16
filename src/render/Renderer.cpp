#include "Renderer.hpp"
#include "../Compositor.hpp"

void renderSurface(struct wlr_surface* surface, int x, int y, void* data) {
    const auto TEXTURE = wlr_surface_get_texture(surface);
    const auto RDATA = (SRenderData*)data;

    if (!TEXTURE)
        return;

    double outputX = 0, outputY = 0;
    wlr_output_layout_output_coords(g_pCompositor->m_sWLROutputLayout, RDATA->output, &outputX, &outputY);

    wlr_box windowBox;
    if (RDATA->surface && surface == RDATA->surface)
        windowBox = {(int)outputX + RDATA->x + x, (int)outputY + RDATA->y + y, RDATA->w, RDATA->h};
    else                                                                                              //  here we clamp to 2, these might be some tiny specks
        windowBox = {(int)outputX + RDATA->x + x, (int)outputY + RDATA->y + y, std::clamp(surface->current.width, 2, 1337420), std::clamp(surface->current.height, 2, 1337420)};
    
    // squish all oversized but dont in some cases, jesus christ this is a mess
    // TODO: this shouldn't be done this way. Custom UV here as well.
    // this is fucking horrible
    // Issue: will cause oversized apps with reserved area to overflow from the window box. (see chromium on ozone wayland)
    const auto PRESQUISHSIZE = Vector2D(windowBox.width, windowBox.height);
    if (RDATA->squishOversized) {
        if (x + windowBox.width > RDATA->w)
            windowBox.width = RDATA->w - x;
        if (y + windowBox.height > RDATA->h)
            windowBox.height = RDATA->h - y;
    }
    
    scaleBox(&windowBox, RDATA->output->scale);

    static auto *const PROUNDING = &g_pConfigManager->getConfigValuePtr("decoration:rounding")->intValue;

    float rounding = RDATA->dontRound ? 0 : RDATA->rounding == -1 ? *PROUNDING : RDATA->rounding;

    if (RDATA->surface && surface == RDATA->surface) {
        if (RDATA->blur)
            g_pHyprOpenGL->renderTextureWithBlur(TEXTURE, &windowBox, RDATA->fadeAlpha * RDATA->alpha, surface, rounding);
        else
            g_pHyprOpenGL->renderTexture(TEXTURE, &windowBox, RDATA->fadeAlpha * RDATA->alpha, rounding, true);

        if (RDATA->decorate) {
            auto col = g_pHyprOpenGL->m_pCurrentWindow->m_cRealBorderColor.col();
            col.a *= RDATA->fadeAlpha * RDATA->alpha / 255.f;
            g_pHyprOpenGL->renderBorder(&windowBox, col, rounding);
        }
    }
    else {
        if (RDATA->surface && wlr_surface_is_xdg_surface(RDATA->surface)) {
            wlr_box geo;
            wlr_xdg_surface_get_geometry(wlr_xdg_surface_from_wlr_surface(RDATA->surface), &geo);

            // TODO: continuation of the above madness.
            if (geo.x != 0 || geo.y != 0) {
                windowBox.width = PRESQUISHSIZE.x;
                windowBox.height = PRESQUISHSIZE.y;
            }

            windowBox.x -= geo.x;
            windowBox.y -= geo.y;
        }

        g_pHyprOpenGL->renderTexture(TEXTURE, &windowBox, RDATA->fadeAlpha * RDATA->alpha, rounding, false);
    }
        

    wlr_surface_send_frame_done(surface, RDATA->when);

    wlr_presentation_surface_sampled_on_output(g_pCompositor->m_sWLRPresentation, surface, RDATA->output);
}

bool CHyprRenderer::shouldRenderWindow(CWindow* pWindow, SMonitor* pMonitor) {
    wlr_box geometry = pWindow->getFullWindowBoundingBox();

    if (!wlr_output_layout_intersects(g_pCompositor->m_sWLROutputLayout, pMonitor->output, &geometry))
        return false;

    // now check if it has the same workspace
    if (pWindow->m_iWorkspaceID == pMonitor->activeWorkspace)
        return true;

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);
    // if not, check if it maybe is active on a different monitor.                                                                                             vvv might be animation in progress
    if (g_pCompositor->isWorkspaceVisible(pWindow->m_iWorkspaceID) || (PWORKSPACE && PWORKSPACE->m_iMonitorID == pMonitor->ID && PWORKSPACE->m_bForceRendering) || (PWORKSPACE && PWORKSPACE->m_iMonitorID == pMonitor->ID && (PWORKSPACE->m_vRenderOffset.isBeingAnimated() || PWORKSPACE->m_fAlpha.isBeingAnimated())))
        return true;

    if (pMonitor->specialWorkspaceOpen && pWindow->m_iWorkspaceID == SPECIAL_WORKSPACE_ID)
        return true;

    return false;
}

bool CHyprRenderer::shouldRenderWindow(CWindow* pWindow) {

    if (!g_pCompositor->windowValidMapped(pWindow))
        return false;

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    if (g_pCompositor->isWorkspaceVisible(pWindow->m_iWorkspaceID))
        return true;

    for (auto& m : g_pCompositor->m_vMonitors) {
        if (PWORKSPACE && PWORKSPACE->m_iMonitorID == m->ID && (PWORKSPACE->m_vRenderOffset.isBeingAnimated() || PWORKSPACE->m_fAlpha.isBeingAnimated()))
            return true;

        if (m->specialWorkspaceOpen && pWindow->m_iWorkspaceID == SPECIAL_WORKSPACE_ID)
            return true;
    }

    return false;
}

void CHyprRenderer::renderWorkspaceWithFullscreenWindow(SMonitor* pMonitor, CWorkspace* pWorkspace, timespec* time) {
    CWindow* pWorkspaceWindow = nullptr;

    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_iWorkspaceID != pWorkspace->m_iID || !w->m_bIsFullscreen)
            continue;

        // found it!
        renderWindow(w.get(), pMonitor, time, pWorkspace->m_efFullscreenMode != FULLSCREEN_FULL, RENDER_PASS_ALL);

        pWorkspaceWindow = w.get();
    }

    // then render windows over fullscreen
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_iWorkspaceID != pWorkspaceWindow->m_iWorkspaceID || !w->m_bCreatedOverFullscreen || !w->m_bIsMapped)
            continue;

        renderWindow(w.get(), pMonitor, time, true, RENDER_PASS_ALL);
    }

    // and then special windows
    for (auto& w : g_pCompositor->m_vWindows) {
        if (!g_pCompositor->windowValidMapped(w.get()) && !w->m_bFadingOut)
            continue;

        if (w->m_iWorkspaceID != SPECIAL_WORKSPACE_ID)
            continue;

        if (!shouldRenderWindow(w.get(), pMonitor))
            continue;

        // render the bad boy
        renderWindow(w.get(), pMonitor, time, true, RENDER_PASS_ALL);
    }

    // and the overlay layers
    if (pWorkspace->m_efFullscreenMode != FULLSCREEN_FULL) {
        // on non-full we draw the bar and shit
        for (auto& ls : pMonitor->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
            renderLayer(ls, pMonitor, time);
        }
    }

    for (auto& ls : pMonitor->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
        renderLayer(ls, pMonitor, time);
    }

    renderDragIcon(pMonitor, time);

    // if correct monitor draw hyprerror
    if (pMonitor == g_pCompositor->m_vMonitors.front().get())
        g_pHyprError->draw();
}

void CHyprRenderer::renderWindow(CWindow* pWindow, SMonitor* pMonitor, timespec* time, bool decorate, eRenderPassMode mode) {
    if (pWindow->m_bHidden)
        return;

    if (pWindow->m_bFadingOut) {
        if (pMonitor->ID == pWindow->m_iMonitorID) // TODO: fix this
            g_pHyprOpenGL->renderSnapshot(&pWindow);
        return;
    }
    
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);
    const auto REALPOS = pWindow->m_vRealPosition.vec() + PWORKSPACE->m_vRenderOffset.vec();
    static const auto PNOFLOATINGBORDERS = &g_pConfigManager->getConfigValuePtr("general:no_border_on_floating")->intValue;

    SRenderData renderdata = {pMonitor->output, time, REALPOS.x, REALPOS.y};
    renderdata.surface = g_pXWaylandManager->getWindowSurface(pWindow);
    renderdata.w = std::clamp(pWindow->m_vRealSize.vec().x, (double)5, (double)1337420); // clamp the size to min 5,
    renderdata.h = std::clamp(pWindow->m_vRealSize.vec().y, (double)5, (double)1337420); // otherwise we'll have issues later with invalid boxes
    renderdata.dontRound = pWindow->m_bIsFullscreen && PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL;
    renderdata.fadeAlpha = pWindow->m_fAlpha.fl() * (PWORKSPACE->m_fAlpha.fl() / 255.f);
    renderdata.alpha = pWindow->m_fActiveInactiveAlpha.fl();
    renderdata.decorate = decorate && !pWindow->m_bX11DoesntWantBorders && (pWindow->m_bIsFloating ? *PNOFLOATINGBORDERS == 0 : true) && (!pWindow->m_bIsFullscreen || PWORKSPACE->m_efFullscreenMode != FULLSCREEN_FULL);
    renderdata.rounding = pWindow->m_sAdditionalConfigData.rounding;
    renderdata.blur = true; // if it shouldn't, it will be ignored later

    // apply window special data
    if (pWindow->m_sSpecialRenderData.alphaInactive == -1)
        renderdata.alpha *= pWindow->m_sSpecialRenderData.alpha;
    else
        renderdata.alpha *= pWindow == g_pCompositor->m_pLastWindow ? pWindow->m_sSpecialRenderData.alpha : pWindow->m_sSpecialRenderData.alphaInactive;

    g_pHyprOpenGL->m_pCurrentWindow = pWindow;

    // render window decorations first, if not fullscreen full

    if (mode == RENDER_PASS_ALL || mode == RENDER_PASS_MAIN) {
        if (!pWindow->m_bIsFullscreen || PWORKSPACE->m_efFullscreenMode != FULLSCREEN_FULL) for (auto& wd : pWindow->m_dWindowDecorations)
                wd->draw(pMonitor, renderdata.alpha * renderdata.fadeAlpha / 255.f);

        if (!pWindow->m_bIsX11) {
            wlr_box geom;
            wlr_xdg_surface_get_geometry(pWindow->m_uSurface.xdg, &geom);

            // first, check for poorly sized windows.

            g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft = Vector2D((double)geom.x / (double)pWindow->m_uSurface.xdg->surface->current.width, (double)geom.y / (double)pWindow->m_uSurface.xdg->surface->current.height);
            g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = Vector2D((double)(geom.width + geom.x) / (double)pWindow->m_uSurface.xdg->surface->current.width, (double)(geom.y + geom.height) / (double)pWindow->m_uSurface.xdg->surface->current.height);

            if (g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft == Vector2D() && g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight == Vector2D(1, 1)) {
                // No special UV mods needed
                g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft = Vector2D(-1, -1);
                g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
            }

            // then, if the surface is too big, modify the pos UV
            if (geom.width > renderdata.w + 1 || geom.height > renderdata.h + 1) {
                const auto OFF = Vector2D(renderdata.w / (double)geom.width, renderdata.h / (double)geom.height);

                if (g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft == Vector2D(-1, -1))
                    g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft = Vector2D(0, 0);

                g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = Vector2D(
                    g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight.x * ((double)renderdata.w / ((double)geom.width / g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight.x)),
                    g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight.y * ((double)renderdata.h / ((double)geom.height / g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight.y))
                );
            }
        } else {
            g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft = Vector2D(-1, -1);
            g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
        }

        wlr_surface_for_each_surface(g_pXWaylandManager->getWindowSurface(pWindow), renderSurface, &renderdata);

        g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft = Vector2D(-1, -1);
        g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
    }

    if (mode == RENDER_PASS_ALL || mode == RENDER_PASS_POPUP) {
        if (!pWindow->m_bIsX11) {
            renderdata.dontRound = false;  // restore dontround
            renderdata.pMonitor = pMonitor;
            renderdata.squishOversized = false; // don't squish popups
            wlr_xdg_surface_for_each_popup_surface(pWindow->m_uSurface.xdg, renderSurface, &renderdata);
        }
    }

    g_pHyprOpenGL->m_pCurrentWindow = nullptr;
}

void CHyprRenderer::renderLayer(SLayerSurface* pLayer, SMonitor* pMonitor, timespec* time) {
    if (pLayer->fadingOut) {
        g_pHyprOpenGL->renderSnapshot(&pLayer);
        return;
    }

    SRenderData renderdata = {pMonitor->output, time, pLayer->geometry.x, pLayer->geometry.y};
    renderdata.fadeAlpha = pLayer->alpha.fl();
    renderdata.blur = pLayer->forceBlur;
    renderdata.surface = pLayer->layerSurface->surface;
    renderdata.decorate = false;
    renderdata.w = pLayer->layerSurface->surface->current.width;
    renderdata.h = pLayer->layerSurface->surface->current.height;
    wlr_surface_for_each_surface(pLayer->layerSurface->surface, renderSurface, &renderdata);
}

void CHyprRenderer::renderAllClientsForMonitor(const int& ID, timespec* time) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(ID);

    if (!PMONITOR)
        return;

    // Render layer surfaces below windows for monitor
    for (auto& ls : PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) {
        renderLayer(ls, PMONITOR, time);
    }
    for (auto& ls : PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) {
        renderLayer(ls, PMONITOR, time);
    }

    // if there is a fullscreen window, render it and then do not render anymore.
    // fullscreen window will hide other windows and top layers
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);

    if (PWORKSPACE->m_bHasFullscreenWindow) {
        renderWorkspaceWithFullscreenWindow(PMONITOR, PWORKSPACE, time);
        return;
    }

    // Non-floating main
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_bHidden && !w->m_bIsMapped && !w->m_bFadingOut)
            continue;

        if (w->m_bIsFloating)
            continue;  // floating are in the second pass

        if (w->m_iWorkspaceID == SPECIAL_WORKSPACE_ID)
            continue;  // special are in the third pass

        if (!shouldRenderWindow(w.get(), PMONITOR))
            continue;

        // render the bad boy
        renderWindow(w.get(), PMONITOR, time, true, RENDER_PASS_MAIN);
    }

    // Non-floating popup
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_bHidden && !w->m_bIsMapped && !w->m_bFadingOut)
            continue;

        if (w->m_bIsFloating)
            continue;  // floating are in the second pass

        if (w->m_iWorkspaceID == SPECIAL_WORKSPACE_ID)
            continue; // special are in the third pass

        if (!shouldRenderWindow(w.get(), PMONITOR))
            continue;

        // render the bad boy
        renderWindow(w.get(), PMONITOR, time, true, RENDER_PASS_POPUP);
    }

    // floating on top
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_bHidden && !w->m_bIsMapped && !w->m_bFadingOut)
            continue;

        if (!w->m_bIsFloating)
            continue;

        if (w->m_iWorkspaceID == SPECIAL_WORKSPACE_ID)
            continue;

        if (!shouldRenderWindow(w.get(), PMONITOR))
            continue;

        // render the bad boy
        renderWindow(w.get(), PMONITOR, time, true, RENDER_PASS_ALL);
    }

    // and then special
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_bHidden && !w->m_bIsMapped && !w->m_bFadingOut)
            continue;
            
        if (w->m_iWorkspaceID != SPECIAL_WORKSPACE_ID)
            continue;

        if (!shouldRenderWindow(w.get(), PMONITOR))
            continue;

        // render the bad boy
        renderWindow(w.get(), PMONITOR, time, true, RENDER_PASS_ALL);
    }

    // Render surfaces above windows for monitor
    for (auto& ls : PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        renderLayer(ls, PMONITOR, time);
    }
    for (auto& ls : PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
        renderLayer(ls, PMONITOR, time);
    }

    renderDragIcon(PMONITOR, time);
}

void CHyprRenderer::outputMgrApplyTest(wlr_output_configuration_v1* config, bool test) {
    wlr_output_configuration_head_v1* head;
    bool noError = true;

    wl_list_for_each(head, &config->heads, link) {

        std::string commandForCfg = "";
        const auto OUTPUT = head->state.output;

        commandForCfg += std::string(OUTPUT->name) + ",";

        if (!head->state.enabled) {
            commandForCfg += "disabled";
            if (!test)
                g_pConfigManager->parseKeyword("monitor", commandForCfg, true);
            continue;
        }

        wlr_output_enable(OUTPUT, head->state.enabled);

        if (head->state.mode)
            commandForCfg += std::to_string(head->state.mode->width) + "x" + std::to_string(head->state.mode->height) + "@" + std::to_string(head->state.mode->refresh / 1000.f) + ",";
        else
            commandForCfg += std::to_string(head->state.custom_mode.width) + "x" + std::to_string(head->state.custom_mode.height) + "@" + std::to_string(head->state.custom_mode.refresh / 1000.f) + ",";

        commandForCfg += std::to_string(head->state.x) + "x" + std::to_string(head->state.y) + "," + std::to_string(head->state.scale);

        if (!test)
            g_pConfigManager->parseKeyword("monitor", commandForCfg, true);

        noError = wlr_output_test(OUTPUT);

        if (!noError)
            break;
    }

    if (!test)
        g_pConfigManager->m_bWantsMonitorReload = true;  // for monitor keywords

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
        int* positive_axis;
        int* negative_axis;
        int margin;
    } edges[] = {
        // Top
        {
            .singular_anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP,
            .anchor_triplet =
                ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
                ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP,
            .positive_axis = &usable_area->y,
            .negative_axis = &usable_area->height,
            .margin = margin_top,
        },
        // Bottom
        {
            .singular_anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
            .anchor_triplet =
                ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
                ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
            .positive_axis = NULL,
            .negative_axis = &usable_area->height,
            .margin = margin_bottom,
        },
        // Left
        {
            .singular_anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT,
            .anchor_triplet =
                ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
            .positive_axis = &usable_area->x,
            .negative_axis = &usable_area->width,
            .margin = margin_left,
        },
        // Right
        {
            .singular_anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
            .anchor_triplet =
                ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
                ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
            .positive_axis = NULL,
            .negative_axis = &usable_area->width,
            .margin = margin_right,
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

void CHyprRenderer::arrangeLayerArray(SMonitor* pMonitor, const std::list<SLayerSurface*>& layerSurfaces, bool exclusiveZone, wlr_box* usableArea) {
    wlr_box full_area = {pMonitor->vecPosition.x, pMonitor->vecPosition.y, pMonitor->vecSize.x, pMonitor->vecSize.y};

    for (auto& ls : layerSurfaces) {
        if (ls->fadingOut || ls->readyToDelete || !ls->layerSurface)
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

        wlr_box box = {
            .width = PSTATE->desired_width,
            .height = PSTATE->desired_height};
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
            box.width = bounds.width -
                        (PSTATE->margin.left + PSTATE->margin.right);
        } else if ((PSTATE->anchor & both_horiz) == both_horiz) {
            // don't apply margins
        } else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT)) {
            box.x += PSTATE->margin.left;
        } else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
            box.x -= PSTATE->margin.right;
        }
        if (box.height == 0) {
            box.y += PSTATE->margin.top;
            box.height = bounds.height -
                         (PSTATE->margin.top + PSTATE->margin.bottom);
        } else if ((PSTATE->anchor & both_vert) == both_vert) {
            // don't apply margins
        } else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP)) {
            box.y += PSTATE->margin.top;
        } else if ((PSTATE->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)) {
            box.y -= PSTATE->margin.bottom;
        }
        if (box.width <= 0 || box.height <= 0) {
            Debug::log(ERR, "LayerSurface %x has a negative/zero w/h???", ls);
            continue;
        }
        // Apply
        ls->geometry = box;

        apply_exclusive(usableArea, PSTATE->anchor, PSTATE->exclusive_zone, PSTATE->margin.top, PSTATE->margin.right, PSTATE->margin.bottom, PSTATE->margin.left);

        wlr_layer_surface_v1_configure(ls->layerSurface, box.width, box.height);

        Debug::log(LOG, "LayerSurface %x arranged: x: %i y: %i w: %i h: %i with margins: t: %i l: %i r: %i b: %i", &ls, box.x, box.y, box.width, box.height, PSTATE->margin.top, PSTATE->margin.left, PSTATE->margin.right, PSTATE->margin.bottom);
    }
}

void CHyprRenderer::arrangeLayersForMonitor(const int& monitor) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(monitor);

    if (!PMONITOR)
        return;

    // Reset the reserved
    PMONITOR->vecReservedBottomRight    = Vector2D();
    PMONITOR->vecReservedTopLeft        = Vector2D();

    wlr_box usableArea = {PMONITOR->vecPosition.x, PMONITOR->vecPosition.y, PMONITOR->vecSize.x, PMONITOR->vecSize.y};

    for (auto& la : PMONITOR->m_aLayerSurfaceLists)
        arrangeLayerArray(PMONITOR, la, true, &usableArea);

    for (auto& la : PMONITOR->m_aLayerSurfaceLists)
        arrangeLayerArray(PMONITOR, la, false, &usableArea);

    PMONITOR->vecReservedTopLeft = Vector2D(usableArea.x, usableArea.y) - PMONITOR->vecPosition;
    PMONITOR->vecReservedBottomRight = PMONITOR->vecSize - Vector2D(usableArea.width, usableArea.height) - PMONITOR->vecReservedTopLeft;

    const auto ENTRY = g_pConfigManager->m_mAdditionalReservedAreas[PMONITOR->szName];
    PMONITOR->vecReservedTopLeft = PMONITOR->vecReservedTopLeft + Vector2D(ENTRY.left, ENTRY.top);
    PMONITOR->vecReservedBottomRight = PMONITOR->vecReservedBottomRight + Vector2D(ENTRY.right, ENTRY.bottom);

    // damage the monitor if can
    if (PMONITOR->damage)
        damageMonitor(PMONITOR);

    Debug::log(LOG, "Monitor %s layers arranged: reserved: %f %f %f %f", PMONITOR->szName.c_str(), PMONITOR->vecReservedTopLeft.x, PMONITOR->vecReservedTopLeft.y, PMONITOR->vecReservedBottomRight.x, PMONITOR->vecReservedBottomRight.y);
}

void CHyprRenderer::damageSurface(wlr_surface* pSurface, double x, double y) {
    if (!pSurface)
        return; // wut?

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

    pixman_region32_translate(&damageBox, x, y);

    for (auto& m : g_pCompositor->m_vMonitors) {
        double lx = 0, ly = 0;
        wlr_output_layout_output_coords(g_pCompositor->m_sWLROutputLayout, m->output, &lx, &ly);
        pixman_region32_translate(&damageBox, lx, ly);
        wlr_region_scale(&damageBox, &damageBox, m->scale);
        wlr_output_damage_add(m->damage, &damageBox);
        pixman_region32_translate(&damageBox, -lx, -ly);
    }

    pixman_region32_fini(&damageBox);

    static auto *const PLOGDAMAGE = &g_pConfigManager->getConfigValuePtr("debug:log_damage")->intValue;

    if (*PLOGDAMAGE)
        Debug::log(LOG, "Damage: Surface (extents): xy: %d, %d wh: %d, %d", damageBox.extents.x1, damageBox.extents.y1, damageBox.extents.x2 - damageBox.extents.x1, damageBox.extents.y2 - damageBox.extents.y1);
}

void CHyprRenderer::damageWindow(CWindow* pWindow) {
    wlr_box damageBox = pWindow->getFullWindowBoundingBox();
    for (auto& m : g_pCompositor->m_vMonitors) {
        wlr_box fixedDamageBox = {damageBox.x - m->vecPosition.x, damageBox.y - m->vecPosition.y, damageBox.width, damageBox.height};
        scaleBox(&fixedDamageBox, m->scale);
        wlr_output_damage_add_box(m->damage, &fixedDamageBox);
    }

    static auto* const PLOGDAMAGE = &g_pConfigManager->getConfigValuePtr("debug:log_damage")->intValue;

    if (*PLOGDAMAGE)
        Debug::log(LOG, "Damage: Window (%s): xy: %d, %d wh: %d, %d", pWindow->m_szTitle.c_str(), damageBox.x, damageBox.y, damageBox.width, damageBox.height);
}

void CHyprRenderer::damageMonitor(SMonitor* pMonitor) {
    wlr_box damageBox = {0, 0, pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y};
    wlr_output_damage_add_box(pMonitor->damage, &damageBox);

    static auto *const PLOGDAMAGE = &g_pConfigManager->getConfigValuePtr("debug:log_damage")->intValue;

    if (*PLOGDAMAGE)
        Debug::log(LOG, "Damage: Monitor %s", pMonitor->szName.c_str());
}

void CHyprRenderer::damageBox(wlr_box* pBox) {
    for (auto& m : g_pCompositor->m_vMonitors) {
        wlr_box damageBox = {pBox->x - m->vecPosition.x, pBox->y - m->vecPosition.y, pBox->width, pBox->height};
        scaleBox(&damageBox, m->scale);
        wlr_output_damage_add_box(m->damage, &damageBox);
    }

    static auto *const PLOGDAMAGE = &g_pConfigManager->getConfigValuePtr("debug:log_damage")->intValue;

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

void CHyprRenderer::renderDragIcon(SMonitor* pMonitor, timespec* time) {
    if (!(g_pInputManager->m_sDrag.dragIcon && g_pInputManager->m_sDrag.iconMapped && g_pInputManager->m_sDrag.dragIcon->surface))
        return;

    SRenderData renderdata = {pMonitor->output, time, g_pInputManager->m_sDrag.pos.x, g_pInputManager->m_sDrag.pos.y};
    renderdata.surface = g_pInputManager->m_sDrag.dragIcon->surface;
    renderdata.w = g_pInputManager->m_sDrag.dragIcon->surface->current.width;
    renderdata.h = g_pInputManager->m_sDrag.dragIcon->surface->current.height;

    wlr_surface_for_each_surface(g_pInputManager->m_sDrag.dragIcon->surface, renderSurface, &renderdata);

    wlr_box box = {g_pInputManager->m_sDrag.pos.x - 2, g_pInputManager->m_sDrag.pos.y - 2, g_pInputManager->m_sDrag.dragIcon->surface->current.width + 4, g_pInputManager->m_sDrag.dragIcon->surface->current.height + 4};
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

bool CHyprRenderer::applyMonitorRule(SMonitor* pMonitor, SMonitorRule* pMonitorRule, bool force) {

    Debug::log(LOG, "Applying monitor rule for %s", pMonitor->szName.c_str());

    // if it's disabled, disable and ignore
    if (pMonitorRule->disabled) {
        wlr_output_enable(pMonitor->output, 0);
        wlr_output_commit(pMonitor->output);

        Events::listener_monitorDestroy(nullptr, pMonitor->output);
        return false;
    }

    // Check if the rule isn't already applied
    if (!force && DELTALESSTHAN(pMonitor->vecPixelSize.x, pMonitorRule->resolution.x, 1) && DELTALESSTHAN(pMonitor->vecPixelSize.y, pMonitorRule->resolution.y, 1) && DELTALESSTHAN(pMonitor->refreshRate, pMonitorRule->refreshRate, 1) && pMonitor->scale == pMonitorRule->scale && DELTALESSTHAN(pMonitor->vecPosition.x, pMonitorRule->offset.x, 1) && DELTALESSTHAN(pMonitor->vecPosition.y, pMonitorRule->offset.y, 1) && pMonitor->transform == pMonitorRule->transform) {
        Debug::log(LOG, "Not applying a new rule to %s because it's already applied!", pMonitor->szName.c_str());
        return true;
    }

    wlr_output_set_scale(pMonitor->output, pMonitorRule->scale);
    pMonitor->scale = pMonitorRule->scale;

    pMonitor->vecPosition = pMonitorRule->offset;

    // loop over modes and choose an appropriate one.
    if (!wl_list_empty(&pMonitor->output->modes)) {
        wlr_output_mode* mode;
        bool found = false;

        wl_list_for_each(mode, &pMonitor->output->modes, link) {
            // if delta of refresh rate, w and h chosen and mode is < 1 we accept it
            if (DELTALESSTHAN(mode->width, pMonitorRule->resolution.x, 1) && DELTALESSTHAN(mode->height, pMonitorRule->resolution.y, 1) && DELTALESSTHAN(mode->refresh / 1000.f, pMonitorRule->refreshRate, 1)) {
                wlr_output_set_mode(pMonitor->output, mode);

                if (!wlr_output_test(pMonitor->output)) {
                    Debug::log(LOG, "Monitor %s: REJECTED available mode: %ix%i@%2f!",
                               pMonitor->output->name, (int)pMonitorRule->resolution.x, (int)pMonitorRule->resolution.y, (float)pMonitorRule->refreshRate,
                               mode->width, mode->height, mode->refresh / 1000.f);
                    continue;
                }

                Debug::log(LOG, "Monitor %s: requested %ix%i@%2f, found available mode: %ix%i@%imHz, applying.",
                           pMonitor->output->name, (int)pMonitorRule->resolution.x, (int)pMonitorRule->resolution.y, (float)pMonitorRule->refreshRate,
                           mode->width, mode->height, mode->refresh);

                found = true;

                pMonitor->refreshRate = mode->refresh / 1000.f;
                pMonitor->vecSize = Vector2D(mode->width, mode->height);

                break;
            }
        }

        if (!found) {
            wlr_output_set_custom_mode(pMonitor->output, (int)pMonitorRule->resolution.x, (int)pMonitorRule->resolution.y, (int)pMonitorRule->refreshRate * 1000);
            pMonitor->vecSize = pMonitorRule->resolution;

            if (!wlr_output_test(pMonitor->output)) {
                Debug::log(ERR, "Custom resolution FAILED, falling back to preferred");

                const auto PREFERREDMODE = wlr_output_preferred_mode(pMonitor->output);

                if (!PREFERREDMODE) {
                    Debug::log(ERR, "Monitor %s has NO PREFERRED MODE, and an INVALID one was requested: %ix%i@%2f",
                               (int)pMonitorRule->resolution.x, (int)pMonitorRule->resolution.y, (float)pMonitorRule->refreshRate);
                    return true;
                }

                // Preferred is valid
                wlr_output_set_mode(pMonitor->output, PREFERREDMODE);

                Debug::log(ERR, "Monitor %s got an invalid requested mode: %ix%i@%2f, using the preferred one instead: %ix%i@%2f",
                           pMonitor->output->name, (int)pMonitorRule->resolution.x, (int)pMonitorRule->resolution.y, (float)pMonitorRule->refreshRate,
                           PREFERREDMODE->width, PREFERREDMODE->height, PREFERREDMODE->refresh / 1000.f);

                pMonitor->refreshRate = PREFERREDMODE->refresh / 1000.f;
                pMonitor->vecSize = Vector2D(PREFERREDMODE->width, PREFERREDMODE->height);
            } else {
                Debug::log(LOG, "Set a custom mode %ix%i@%2f (mode not found in monitor modes)", (int)pMonitorRule->resolution.x, (int)pMonitorRule->resolution.y, (float)pMonitorRule->refreshRate);
            }
        }
    } else {
        wlr_output_set_custom_mode(pMonitor->output, (int)pMonitorRule->resolution.x, (int)pMonitorRule->resolution.y, (int)pMonitorRule->refreshRate * 1000);
        pMonitor->vecSize = pMonitorRule->resolution;
    }

    wlr_output_set_transform(pMonitor->output, pMonitorRule->transform);
    pMonitor->transform = pMonitorRule->transform;

    pMonitor->vecPixelSize = pMonitor->vecSize;

    // update renderer
    g_pHyprOpenGL->destroyMonitorResources(pMonitor);

    if (!wlr_output_commit(pMonitor->output)) {
        Debug::log(ERR, "Couldn't commit output named %s", pMonitor->output->name);
        return true;
    }

    int x, y;
    wlr_output_transformed_resolution(pMonitor->output, &x, &y);
    pMonitor->vecSize = (Vector2D(x, y) / pMonitor->scale).floor();
    pMonitor->vecTransformedSize = Vector2D(x,y);

    wlr_output_layout_add(g_pCompositor->m_sWLROutputLayout, pMonitor->output, (int)pMonitorRule->offset.x, (int)pMonitorRule->offset.y);

    //wlr_output_damage_add_whole(pMonitor->damage);

    wlr_output_enable(pMonitor->output, true);

    // updato wlroots
    Events::listener_change(nullptr, nullptr);

    // updato us
    arrangeLayersForMonitor(pMonitor->ID);

    // frame skip
    pMonitor->framesToSkip = 1;

    return true;
}

void CHyprRenderer::ensureCursorRenderingMode() {
    static auto *const PCURSORTIMEOUT = &g_pConfigManager->getConfigValuePtr("general:cursor_inactive_timeout")->intValue;

    const auto PASSEDCURSORSECONDS = g_pInputManager->m_tmrLastCursorMovement.getSeconds();

    if (*PCURSORTIMEOUT > 0) {
        if (*PCURSORTIMEOUT < PASSEDCURSORSECONDS && m_bHasARenderedCursor) {
            m_bHasARenderedCursor = false;

            wlr_cursor_set_surface(g_pCompositor->m_sWLRCursor, nullptr, 0, 0); // hide

            Debug::log(LOG, "Hiding the cursor (timeout)");

            for (auto& m : g_pCompositor->m_vMonitors)
                g_pHyprRenderer->damageMonitor(m.get());  // TODO: maybe just damage the cursor area?
        } else if (*PCURSORTIMEOUT > PASSEDCURSORSECONDS && !m_bHasARenderedCursor) {
            m_bHasARenderedCursor = true;

            if (!m_bWindowRequestedCursorHide)
                wlr_xcursor_manager_set_cursor_image(g_pCompositor->m_sWLRXCursorMgr, "left_ptr", g_pCompositor->m_sWLRCursor);

            Debug::log(LOG, "Showing the cursor (timeout)");

            for (auto& m : g_pCompositor->m_vMonitors)
                g_pHyprRenderer->damageMonitor(m.get());  // TODO: maybe just damage the cursor area?
        }
    } else {
        m_bHasARenderedCursor = true;
    }
}

bool CHyprRenderer::shouldRenderCursor() {
    return m_bHasARenderedCursor;
}
