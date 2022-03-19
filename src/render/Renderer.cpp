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

    wlr_box windowBox = {outputX + RDATA->x + x, outputY + RDATA->y + y, surface->current.width, surface->current.height};
    scaleBox(&windowBox, RDATA->output->scale);

    const auto TRANSFORM = wlr_output_transform_invert(surface->current.transform);
    float matrix[9];
    wlr_matrix_project_box(matrix, &windowBox, TRANSFORM, 0, RDATA->output->transform_matrix);

    wlr_render_texture_with_matrix(g_pCompositor->m_sWLRRenderer, TEXTURE, matrix, 1); // TODO: fadein/out

    wlr_surface_send_frame_done(surface, RDATA->when);

    wlr_presentation_surface_sampled_on_output(g_pCompositor->m_sWLRPresentation, surface, RDATA->output);
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

    for (auto& w : g_pCompositor->m_lWindows) {

        if (w.m_bIsX11)
            continue;

        wlr_box geometry = { w.m_vPosition.x, w.m_vPosition.y, w.m_vSize.x, w.m_vSize.y };

        if (!wlr_output_layout_intersects(g_pCompositor->m_sWLROutputLayout, PMONITOR->output, &geometry))
            continue;

        // render the bad boy
        wlr_output_layout_output_coords(g_pCompositor->m_sWLROutputLayout, PMONITOR->output, &w.m_vPosition.x, &w.m_vPosition.y);

        SRenderData renderdata = {PMONITOR->output, time, w.m_vPosition.x, w.m_vPosition.y};

        wlr_surface_for_each_surface(g_pXWaylandManager->getWindowSurface(&w), renderSurface, &renderdata);
        wlr_xdg_surface_for_each_popup_surface(w.m_uSurface.xdg, renderSurface, &renderdata);
    }

    for (auto& w : g_pCompositor->m_lWindows) {

        if (!w.m_bIsX11)
            continue;

        if (!g_pCompositor->windowValidMapped(&w))
            continue;

        wlr_box geometry = {w.m_vPosition.x, w.m_vPosition.y, w.m_vSize.x, w.m_vSize.y};

        if (!wlr_output_layout_intersects(g_pCompositor->m_sWLROutputLayout, PMONITOR->output, &geometry)) 
            continue;

        // render the bad boy
        wlr_output_layout_output_coords(g_pCompositor->m_sWLROutputLayout, PMONITOR->output, &w.m_vPosition.x, &w.m_vPosition.y);

        SRenderData renderdata = {PMONITOR->output, time, w.m_vPosition.x, w.m_vPosition.y};

        if (w.m_uSurface.xwayland->surface)
            wlr_surface_for_each_surface(g_pXWaylandManager->getWindowSurface(&w), renderSurface, &renderdata);
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