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

    addWLSignal(&WLRLAYERSURFACE->surface->events.commit, &layerSurface->listen_commitLayerSurface, layerSurface, "layerSurface");
    addWLSignal(&WLRLAYERSURFACE->surface->events.destroy, &layerSurface->listen_destroyLayerSurface, layerSurface, "layerSurface");
    addWLSignal(&WLRLAYERSURFACE->events.map, &layerSurface->listen_mapLayerSurface, layerSurface, "layerSurface");
    addWLSignal(&WLRLAYERSURFACE->events.unmap, &layerSurface->listen_unmapLayerSurface, layerSurface, "layerSurface");
    addWLSignal(&WLRLAYERSURFACE->events.new_popup, &layerSurface->listen_newPopup, layerSurface, "layerSurface");

    layerSurface->layerSurface = WLRLAYERSURFACE;
    layerSurface->layer = WLRLAYERSURFACE->current.layer;
    WLRLAYERSURFACE->data = layerSurface;
    layerSurface->monitorID = PMONITOR->ID;

    Debug::log(LOG, "LayerSurface %x (namespace %s layer %d) created on monitor %s", layerSurface->layerSurface, layerSurface->layerSurface->_namespace, layerSurface->layer, PMONITOR->szName.c_str());
}

void Events::listener_destroyLayerSurface(wl_listener* listener, void* data) {
    SLayerSurface* layersurface = wl_container_of(listener, layersurface, listen_destroyLayerSurface);

    Debug::log(LOG, "LayerSurface %x destroyed", layersurface->layerSurface);

    if (layersurface->layerSurface->mapped)
        layersurface->layerSurface->mapped = false;

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

    // rearrange to fix the reserved areas
    if (PMONITOR) {
        g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->ID);
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PMONITOR->ID);
    }
}

void Events::listener_mapLayerSurface(wl_listener* listener, void* data) {
    SLayerSurface* layersurface = wl_container_of(listener, layersurface, listen_mapLayerSurface);

    Debug::log(LOG, "LayerSurface %x mapped", layersurface->layerSurface);

    layersurface->layerSurface->mapped = true;

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

    if (layersurface->layerSurface->current.keyboard_interactive)
        g_pCompositor->focusSurface(layersurface->layerSurface->surface);

    layersurface->position = Vector2D(layersurface->geometry.x, layersurface->geometry.y);
}

void Events::listener_unmapLayerSurface(wl_listener* listener, void* data) {
    SLayerSurface* layersurface = wl_container_of(listener, layersurface, listen_unmapLayerSurface);

    Debug::log(LOG, "LayerSurface %x unmapped", layersurface->layerSurface);

    if (layersurface->layerSurface->mapped)
        layersurface->layerSurface->mapped = false;

    if (layersurface->layerSurface->surface == g_pCompositor->m_pLastFocus)
        g_pCompositor->m_pLastFocus = nullptr;
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

    layersurface->position = Vector2D(layersurface->geometry.x, layersurface->geometry.y);
}