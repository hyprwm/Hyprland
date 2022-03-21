#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/InputManager.hpp"
#include "../render/Renderer.hpp"
#include "Events.hpp"

// --------------------------------------------- //
//    _           __     ________ _____   _____  //
//  | |        /\\ \   / /  ____|  __ \ / ____|  //
//  | |       /  \\ \_/ /| |__  | |__) | (___    //
//  | |      / /\ \\   / |  __| |  _  / \___ \   //
//  | |____ / ____ \| |  | |____| | \ \ ____) |  //
//  |______/_/    \_\_|  |______|_|  \_\_____/   //
//                                               //
// --------------------------------------------- //

void Events::listener_newLayerSurface(wl_listener* listener, void* data) {
    const auto WLRLAYERSURFACE = (wlr_layer_surface_v1*)data;

    if (!WLRLAYERSURFACE->output) {
        const auto PMONITOR = g_pCompositor->getMonitorFromCursor();

        if (!PMONITOR) {
            Debug::log(ERR, "No monitor at cursor on new layer without a monitor. Ignoring.");
            wlr_layer_surface_v1_destroy(WLRLAYERSURFACE);
            return;
        }

        Debug::log(LOG, "New LayerSurface has no preferred monitor. Assigning Monitor %s", PMONITOR->szName);

        WLRLAYERSURFACE->output = PMONITOR->output;
    }

    const auto PMONITOR = (SMonitor*)g_pCompositor->getMonitorFromOutput(WLRLAYERSURFACE->output);
    PMONITOR->m_aLayerSurfaceLists[WLRLAYERSURFACE->pending.layer].push_back(new SLayerSurface());
    SLayerSurface* layerSurface = PMONITOR->m_aLayerSurfaceLists[WLRLAYERSURFACE->pending.layer].back();

    if (!WLRLAYERSURFACE->output) {
        WLRLAYERSURFACE->output = g_pCompositor->m_lMonitors.front().output;  // TODO: current mon
    }

    wl_signal_add(&WLRLAYERSURFACE->surface->events.commit, &layerSurface->listen_commitLayerSurface);
    wl_signal_add(&WLRLAYERSURFACE->surface->events.destroy, &layerSurface->listen_destroyLayerSurface);
    wl_signal_add(&WLRLAYERSURFACE->events.map, &layerSurface->listen_mapLayerSurface);
    wl_signal_add(&WLRLAYERSURFACE->events.unmap, &layerSurface->listen_unmapLayerSurface);
    wl_signal_add(&WLRLAYERSURFACE->events.new_popup, &layerSurface->listen_newPopup);

    layerSurface->layerSurface = WLRLAYERSURFACE;
    WLRLAYERSURFACE->data = layerSurface;
    layerSurface->monitorID = PMONITOR->ID;

    Debug::log(LOG, "LayerSurface %x (namespace %s layer %d) created on monitor %s", layerSurface, layerSurface->layerSurface->_namespace, layerSurface->layer, PMONITOR->szName.c_str());
}

void Events::listener_destroyLayerSurface(wl_listener* listener, void* data) {
    SLayerSurface* layersurface = wl_container_of(listener, layersurface, listen_destroyLayerSurface);

    if (layersurface->layerSurface->mapped)
        layersurface->layerSurface->mapped = 0;

    if (layersurface->layerSurface->surface == g_pCompositor->m_pLastFocus)
        g_pCompositor->m_pLastFocus = nullptr;

    wl_list_remove(&layersurface->listen_destroyLayerSurface.link);
    wl_list_remove(&layersurface->listen_mapLayerSurface.link);
    wl_list_remove(&layersurface->listen_unmapLayerSurface.link);
    wl_list_remove(&layersurface->listen_commitLayerSurface.link);

    const auto PMONITOR = g_pCompositor->getMonitorFromID(layersurface->monitorID);

    // remove the layersurface as it's not used anymore
    PMONITOR->m_aLayerSurfaceLists[layersurface->layer].remove(layersurface);
    delete layersurface;

    Debug::log(LOG, "LayerSurface %x destroyed", layersurface);
}

void Events::listener_mapLayerSurface(wl_listener* listener, void* data) {
    SLayerSurface* layersurface = wl_container_of(listener, layersurface, listen_mapLayerSurface);

    wlr_surface_send_enter(layersurface->layerSurface->surface, layersurface->layerSurface->output);

    // fix if it changed its mon
    const auto PMONITOR = g_pCompositor->getMonitorFromOutput(layersurface->layerSurface->output);

    if ((uint64_t)layersurface->monitorID != PMONITOR->ID) {
        const auto POLDMON = g_pCompositor->getMonitorFromID(layersurface->monitorID);
        POLDMON->m_aLayerSurfaceLists[layersurface->layer].remove(layersurface);
        PMONITOR->m_aLayerSurfaceLists[layersurface->layer].push_back(layersurface);
        layersurface->monitorID = PMONITOR->ID;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(POLDMON->ID);
        g_pHyprRenderer->arrangeLayersForMonitor(POLDMON->ID);
    }

    g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->ID);

    if (layersurface->layer == ZWLR_LAYER_SHELL_V1_LAYER_TOP || layersurface->layer == ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY)
        g_pCompositor->focusSurface(layersurface->layerSurface->surface);

    Debug::log(LOG, "LayerSurface %x mapped", layersurface);
}

void Events::listener_unmapLayerSurface(wl_listener* listener, void* data) {
    SLayerSurface* layersurface = wl_container_of(listener, layersurface, listen_unmapLayerSurface);

    if (layersurface->layerSurface->mapped)
        layersurface->layerSurface->mapped = 0;

    if (layersurface->layerSurface->surface == g_pCompositor->m_pLastFocus)
        g_pCompositor->m_pLastFocus = nullptr;

    Debug::log(LOG, "LayerSurface %x unmapped", layersurface);
}

void Events::listener_commitLayerSurface(wl_listener* listener, void* data) {
    SLayerSurface* layersurface = wl_container_of(listener, layersurface, listen_commitLayerSurface);

    if (!layersurface->layerSurface || !layersurface->layerSurface->output)
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromOutput(layersurface->layerSurface->output);

    if (!PMONITOR)
        return;

    // fix if it changed its mon
    if ((uint64_t)layersurface->monitorID != PMONITOR->ID) {
        const auto POLDMON = g_pCompositor->getMonitorFromID(layersurface->monitorID);
        POLDMON->m_aLayerSurfaceLists[layersurface->layer].remove(layersurface);
        PMONITOR->m_aLayerSurfaceLists[layersurface->layer].push_back(layersurface);
        layersurface->monitorID = PMONITOR->ID;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(POLDMON->ID);
        g_pHyprRenderer->arrangeLayersForMonitor(POLDMON->ID);
    }

    g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->ID);

    if (layersurface->layer != layersurface->layerSurface->current.layer) {
        PMONITOR->m_aLayerSurfaceLists[layersurface->layer].remove(layersurface);
        PMONITOR->m_aLayerSurfaceLists[layersurface->layerSurface->current.layer].push_back(layersurface);
        layersurface->layer = layersurface->layerSurface->current.layer;
    }

    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PMONITOR->ID);
}

void Events::listener_surfaceXWayland(wl_listener* listener, void* data) {
    const auto XWSURFACE = (wlr_xwayland_surface*)data;

    g_pCompositor->m_lWindows.push_back(CWindow());
    const auto PNEWWINDOW = &g_pCompositor->m_lWindows.back();

    PNEWWINDOW->m_uSurface.xwayland = XWSURFACE;
    PNEWWINDOW->m_iX11Type = XWSURFACE->override_redirect ? 2 : 1;
    PNEWWINDOW->m_bIsX11 = true;

    wl_signal_add(&XWSURFACE->events.map, &PNEWWINDOW->listen_mapWindow);
    wl_signal_add(&XWSURFACE->events.unmap, &PNEWWINDOW->listen_unmapWindow);
    wl_signal_add(&XWSURFACE->events.request_activate, &PNEWWINDOW->listen_activateX11);
    wl_signal_add(&XWSURFACE->events.request_configure, &PNEWWINDOW->listen_configureX11);
    wl_signal_add(&XWSURFACE->events.set_title, &PNEWWINDOW->listen_setTitleWindow);
    wl_signal_add(&XWSURFACE->events.destroy, &PNEWWINDOW->listen_destroyWindow);
    wl_signal_add(&XWSURFACE->events.request_fullscreen, &PNEWWINDOW->listen_fullscreenWindow);

    Debug::log(LOG, "New XWayland Surface created.");
}

void Events::listener_newXDGSurface(wl_listener* listener, void* data) {
    // A window got opened
    const auto XDGSURFACE = (wlr_xdg_surface*)data;

    if (XDGSURFACE->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
        return;  // TODO: handle?

    g_pCompositor->m_lWindows.push_back(CWindow());
    const auto PNEWWINDOW = &g_pCompositor->m_lWindows.back();
    PNEWWINDOW->m_uSurface.xdg = XDGSURFACE;

    wl_signal_add(&XDGSURFACE->surface->events.commit, &PNEWWINDOW->listen_commitWindow);
    wl_signal_add(&XDGSURFACE->events.map, &PNEWWINDOW->listen_mapWindow);
    wl_signal_add(&XDGSURFACE->events.unmap, &PNEWWINDOW->listen_unmapWindow);
    wl_signal_add(&XDGSURFACE->events.destroy, &PNEWWINDOW->listen_destroyWindow);
    wl_signal_add(&XDGSURFACE->toplevel->events.set_title, &PNEWWINDOW->listen_setTitleWindow);
    wl_signal_add(&XDGSURFACE->toplevel->events.request_fullscreen, &PNEWWINDOW->listen_fullscreenWindow);

    Debug::log(LOG, "New XDG Surface created.");
}