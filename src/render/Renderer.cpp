#include "Renderer.hpp"
#include "../Compositor.hpp"

void scaleBox(wlr_box* box, float scale) {
    box->width = std::round((box->x + box->width) * scale) - std::round(box->x * scale);
    box->height = std::round((box->y + box->height) * scale) - std::round(box->y * scale);
    box->x = std::round(box->x * scale);
    box->y = std::round(box->y * scale);
}

void renderSurface(struct wlr_surface* surface, int x, int y, void* data) {
    const auto TEXTURE = wlr_surface_get_texture(surface);
    const auto RDATA = (SRenderData*)data;

    if (!TEXTURE)
        return;

    double outputX = 0, outputY = 0;
    wlr_output_layout_output_coords(g_pCompositor->m_sWLROutputLayout, RDATA->output, &outputX, &outputY);

    wlr_box windowBox;
    if (RDATA->surface && surface == RDATA->surface) {
        windowBox = {(int)outputX + RDATA->x + x, (int)outputY + RDATA->y + y, RDATA->w, RDATA->h};
        g_pHyprOpenGL->scissor(&windowBox);
    } else {
        windowBox = {(int)outputX + RDATA->x + x, (int)outputY + RDATA->y + y, surface->current.width, surface->current.height};
    }
    scaleBox(&windowBox, RDATA->output->scale);

    const auto TRANSFORM = wlr_output_transform_invert(surface->current.transform);
    float matrix[9];
    wlr_matrix_project_box(matrix, &windowBox, TRANSFORM, 0, RDATA->output->transform_matrix);

    g_pHyprOpenGL->renderTexture(TEXTURE, matrix, 255.f, g_pConfigManager->getInt("decoration:rounding")); // TODO: fadeinout

    wlr_surface_send_frame_done(surface, RDATA->when);

    wlr_presentation_surface_sampled_on_output(g_pCompositor->m_sWLRPresentation, surface, RDATA->output);

    g_pHyprOpenGL->scissor(nullptr);
}

bool shouldRenderWindow(CWindow* pWindow, SMonitor* pMonitor) {
    wlr_box geometry = {pWindow->m_vRealPosition.x, pWindow->m_vRealPosition.y, pWindow->m_vRealSize.x, pWindow->m_vRealSize.y};

    if (!wlr_output_layout_intersects(g_pCompositor->m_sWLROutputLayout, pMonitor->output, &geometry))
        return false;

    // now check if it has the same workspace
    if (pWindow->m_iWorkspaceID == pMonitor->activeWorkspace)
        return true;

    // if not, check if it maybe is active on a different monitor.
    if (g_pCompositor->isWorkspaceVisible(pWindow->m_iWorkspaceID))
        return true;

    return false;
}

void CHyprRenderer::renderWorkspaceWithFullscreenWindow(SMonitor* pMonitor, SWorkspace* pWorkspace, timespec* time) {
    CWindow* pWorkspaceWindow = nullptr;

    for (auto& w : g_pCompositor->m_lWindows) {
        if (w.m_iWorkspaceID != pWorkspace->ID || !w.m_bIsFullscreen)
            continue;

        // found it!
        renderWindow(&w, pMonitor, time, false);

        pWorkspaceWindow = &w;
    }

    // then render windows over fullscreen
    for (auto& w : g_pCompositor->m_lWindows) {
        if (w.m_iWorkspaceID != pWorkspaceWindow->m_iWorkspaceID || !w.m_bCreatedOverFullscreen || !w.m_bIsMapped)
            continue;

        renderWindow(&w, pMonitor, time, true);
    }

    renderDragIcon(pMonitor, time);
}

void CHyprRenderer::renderWindow(CWindow* pWindow, SMonitor* pMonitor, timespec* time, bool decorate) {
    // border
    if (decorate && !pWindow->m_bX11DoesntWantBorders)
        drawBorderForWindow(pWindow, pMonitor);

    const auto REALPOS = pWindow->m_vRealPosition;
    SRenderData renderdata = {pMonitor->output, time, REALPOS.x, REALPOS.y};
    renderdata.surface = g_pXWaylandManager->getWindowSurface(pWindow);
    renderdata.w = pWindow->m_vRealSize.x;
    renderdata.h = pWindow->m_vRealSize.y;

    wlr_surface_for_each_surface(g_pXWaylandManager->getWindowSurface(pWindow), renderSurface, &renderdata);

    if (pWindow->m_bIsX11) {
        if (pWindow->m_uSurface.xwayland->surface) {
            wlr_surface_for_each_surface(pWindow->m_uSurface.xwayland->surface, renderSurface, &renderdata);
        }
    }
    else {
        renderdata.pMonitor = pMonitor;
        wlr_xdg_surface_for_each_popup_surface(pWindow->m_uSurface.xdg, renderSurface, &renderdata);
    } 
}

void CHyprRenderer::renderAllClientsForMonitor(const int& ID, timespec* time) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(ID);

    if (!PMONITOR)
        return;

    // Render layer surfaces below windows for monitor
    for (auto& ls : PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) {
        SRenderData renderdata = {PMONITOR->output, time, ls->geometry.x, ls->geometry.y};
        wlr_surface_for_each_surface(ls->layerSurface->surface, renderSurface, &renderdata);
    }
    for (auto& ls : PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) {
        SRenderData renderdata = {PMONITOR->output, time, ls->geometry.x, ls->geometry.y};
        wlr_surface_for_each_surface(ls->layerSurface->surface, renderSurface, &renderdata);
    }

    // if there is a fullscreen window, render it and then do not render anymore.
    // fullscreen window will hide other windows and top layers
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);

    if (PWORKSPACE->hasFullscreenWindow) {
        renderWorkspaceWithFullscreenWindow(PMONITOR, PWORKSPACE, time);
        return;
    }

    // Non-floating
    for (auto& w : g_pCompositor->m_lWindows) {
        if (!g_pCompositor->windowValidMapped(&w))
            continue;

        if (w.m_bIsFloating)
            continue;  // floating are in second pass

        if (!shouldRenderWindow(&w, PMONITOR))
            continue;

        // render the bad boy
        renderWindow(&w, PMONITOR, time, true);
    }

    // floating on top
    for (auto& w : g_pCompositor->m_lWindows) {
        if (!g_pCompositor->windowValidMapped(&w))
            continue;

        if (!w.m_bIsFloating)
            continue;

        if (!shouldRenderWindow(&w, PMONITOR))
            continue;

        // render the bad boy
        renderWindow(&w, PMONITOR, time, true);
    }

    // Render surfaces above windows for monitor
    for (auto& ls : PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        SRenderData renderdata = {PMONITOR->output, time, ls->geometry.x, ls->geometry.y};
        wlr_surface_for_each_surface(ls->layerSurface->surface, renderSurface, &renderdata);
    }
    for (auto& ls : PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
        SRenderData renderdata = {PMONITOR->output, time, ls->geometry.x, ls->geometry.y};
        wlr_surface_for_each_surface(ls->layerSurface->surface, renderSurface, &renderdata);
    }

    renderDragIcon(PMONITOR, time);
}

void CHyprRenderer::outputMgrApplyTest(wlr_output_configuration_v1* config, bool test) {
    wlr_output_configuration_head_v1* head;
    bool noError = true;

    wl_list_for_each(head, &config->heads, link) {
        const auto OUTPUT = head->state.output;

        wlr_output_enable(OUTPUT, head->state.enabled);
        if (head->state.enabled) {
            if (head->state.mode)
                wlr_output_set_mode(OUTPUT, head->state.mode);
            else
                wlr_output_set_custom_mode(OUTPUT, head->state.custom_mode.width, head->state.custom_mode.height, head->state.custom_mode.refresh);

            wlr_output_layout_move(g_pCompositor->m_sWLROutputLayout, OUTPUT, head->state.x, head->state.y);
            wlr_output_set_transform(OUTPUT, head->state.transform);
            wlr_output_set_scale(OUTPUT, head->state.scale);
        }

        noError = wlr_output_test(OUTPUT);

        if (!noError)
            break;
    }

    wl_list_for_each(head, &config->heads, link) {
        if (noError && !test)
            wlr_output_commit(head->state.output);
        else
            wlr_output_rollback(head->state.output);
    }

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

    Debug::log(LOG, "Monitor %s layers arranged: reserved: %f %f %f %f", PMONITOR->szName.c_str(), PMONITOR->vecReservedTopLeft.x, PMONITOR->vecReservedTopLeft.y, PMONITOR->vecReservedBottomRight.x, PMONITOR->vecReservedBottomRight.y);
}

void CHyprRenderer::drawBorderForWindow(CWindow* pWindow, SMonitor* pMonitor) {
    const auto BORDERSIZE = g_pConfigManager->getInt("general:border_size");

    if (BORDERSIZE < 1)
        return;

    const auto BORDERCOL = pWindow->m_cRealBorderColor;

    Vector2D correctPos = pWindow->m_vRealPosition - pMonitor->vecPosition;

    // top
    wlr_box border = {correctPos.x - BORDERSIZE, correctPos.y - BORDERSIZE, pWindow->m_vRealSize.x + 2 * BORDERSIZE, BORDERSIZE};
    g_pHyprOpenGL->renderRect(&border, BORDERCOL);

    // bottom
    border.y = correctPos.y + pWindow->m_vRealSize.y;
    g_pHyprOpenGL->renderRect(&border, BORDERCOL);

    // left
    border.y = correctPos.y;
    border.width = BORDERSIZE;
    border.height = pWindow->m_vRealSize.y;
    g_pHyprOpenGL->renderRect(&border, BORDERCOL);

    // right
    border.x = correctPos.x + pWindow->m_vRealSize.x;
    g_pHyprOpenGL->renderRect(&border, BORDERCOL);
}

void damageSurfaceIter(struct wlr_surface* surface, int x, int y, void* data) {
    auto* renderdata = (SRenderData*)data;
    bool entire = (bool*)renderdata->data;

    wlr_box box = {.x = renderdata->x, .y = renderdata->y, .width = renderdata->w, .height = renderdata->h};
    scaleBox(&box, renderdata->output->scale);

    pixman_region32_t damageRegion;
    pixman_region32_init(&damageRegion);
    wlr_surface_get_effective_damage(renderdata->surface, &damageRegion);
    wlr_region_scale(&damageRegion, &damageRegion, renderdata->output->scale);

    if (std::ceil(renderdata->output->scale) > renderdata->surface->current.scale) {
        wlr_region_expand(&damageRegion, &damageRegion, std::ceil(renderdata->output->scale) - renderdata->surface->current.scale);
    }

    const auto PMONITOR = g_pCompositor->getMonitorFromOutput(renderdata->output);

    pixman_region32_translate(&damageRegion, box.x, box.y);
    wlr_output_damage_add(PMONITOR->damage, &damageRegion);
    pixman_region32_fini(&damageRegion);

    if (entire)
        wlr_output_damage_add_box(PMONITOR->damage, &box);

    if (!wl_list_empty(&surface->current.frame_callback_list)) {
        wlr_output_schedule_frame(renderdata->output);
    }
}

void CHyprRenderer::damageSurface(SMonitor* pMonitor, double x, double y, wlr_surface* pSurface, void* data) {
    if (!pSurface || !pMonitor)
        return; // wut?

    SRenderData renderData = {
        .output = pMonitor->output,
        .x = x,
        .y = y,
        .data = data,
        .surface = pSurface,
        .w = pSurface->current.width,
        .h = pSurface->current.height
    };

    wlr_surface_for_each_surface(pSurface, damageSurfaceIter, &renderData);
}

void CHyprRenderer::renderDragIcon(SMonitor* pMonitor, timespec* time) {
    if (!(g_pInputManager->m_sDrag.dragIcon && g_pInputManager->m_sDrag.iconMapped && g_pInputManager->m_sDrag.dragIcon->surface))
        return;

    SRenderData renderdata = {pMonitor->output, time, g_pInputManager->m_sDrag.pos.x, g_pInputManager->m_sDrag.pos.y};
    renderdata.surface = g_pInputManager->m_sDrag.dragIcon->surface;
    renderdata.w = g_pInputManager->m_sDrag.dragIcon->surface->current.width;
    renderdata.h = g_pInputManager->m_sDrag.dragIcon->surface->current.height;

    wlr_surface_for_each_surface(g_pInputManager->m_sDrag.dragIcon->surface, renderSurface, &renderdata);
}