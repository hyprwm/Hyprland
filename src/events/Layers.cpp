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

    layerSurface->hyprListener_commitLayerSurface.initCallback(&WLRLAYERSURFACE->surface->events.commit, &Events::listener_commitLayerSurface, layerSurface, "layerSurface");
    layerSurface->hyprListener_destroyLayerSurface.initCallback(&WLRLAYERSURFACE->surface->events.destroy, &Events::listener_destroyLayerSurface, layerSurface, "layerSurface");
    layerSurface->hyprListener_mapLayerSurface.initCallback(&WLRLAYERSURFACE->events.map, &Events::listener_mapLayerSurface, layerSurface, "layerSurface");
    layerSurface->hyprListener_unmapLayerSurface.initCallback(&WLRLAYERSURFACE->events.unmap, &Events::listener_unmapLayerSurface, layerSurface, "layerSurface");
    layerSurface->hyprListener_newPopup.initCallback(&WLRLAYERSURFACE->events.new_popup, &Events::listener_newPopup, layerSurface, "layerSurface");

    layerSurface->layerSurface = WLRLAYERSURFACE;
    layerSurface->layer = WLRLAYERSURFACE->current.layer;
    WLRLAYERSURFACE->data = layerSurface;
    layerSurface->monitorID = PMONITOR->ID;

    Debug::log(LOG, "LayerSurface %x (namespace %s layer %d) created on monitor %s", layerSurface->layerSurface, layerSurface->layerSurface->_namespace, layerSurface->layer, PMONITOR->szName.c_str());
}

void Events::listener_destroyLayerSurface(void* owner, void* data) {
    SLayerSurface* layersurface = (SLayerSurface*)owner;

    Debug::log(LOG, "LayerSurface %x destroyed", layersurface->layerSurface);

    if (layersurface->layerSurface->mapped)
        layersurface->layerSurface->mapped = false;

    if (layersurface->layerSurface->surface == g_pCompositor->m_pLastFocus)
        g_pCompositor->m_pLastFocus = nullptr;

    layersurface->hyprListener_commitLayerSurface.removeCallback();
    layersurface->hyprListener_destroyLayerSurface.removeCallback();
    layersurface->hyprListener_mapLayerSurface.removeCallback();
    layersurface->hyprListener_unmapLayerSurface.removeCallback();
    layersurface->hyprListener_newPopup.removeCallback();

    const auto PMONITOR = g_pCompositor->getMonitorFromID(layersurface->monitorID);

    // remove the layersurface as it's not used anymore
    PMONITOR->m_aLayerSurfaceLists[layersurface->layer].remove(layersurface);
    delete layersurface;

    // rearrange to fix the reserved areas
    if (PMONITOR) {
        g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->ID);
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PMONITOR->ID);

        // and damage
        wlr_box geomFixed = {layersurface->geometry.x + PMONITOR->vecPosition.x, layersurface->geometry.y + PMONITOR->vecPosition.y, layersurface->geometry.width, layersurface->geometry.height};
        g_pHyprRenderer->damageBox(&geomFixed);
    }
}

void Events::listener_mapLayerSurface(void* owner, void* data) {
    SLayerSurface* layersurface = (SLayerSurface*)owner;

    Debug::log(LOG, "LayerSurface %x mapped", layersurface->layerSurface);

    layersurface->layerSurface->mapped = true;

    wlr_surface_send_enter(layersurface->layerSurface->surface, layersurface->layerSurface->output);

    // fix if it changed its mon
    const auto PMONITOR = g_pCompositor->getMonitorFromOutput(layersurface->layerSurface->output);

    if (!PMONITOR)
        return;

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

    wlr_box geomFixed = {layersurface->geometry.x + PMONITOR->vecPosition.x, layersurface->geometry.y + PMONITOR->vecPosition.y, layersurface->geometry.width, layersurface->geometry.height};
    g_pHyprRenderer->damageBox(&geomFixed);
}

void Events::listener_unmapLayerSurface(void* owner, void* data) {
    SLayerSurface* layersurface = (SLayerSurface*)owner;

    Debug::log(LOG, "LayerSurface %x unmapped", layersurface->layerSurface);

    if (layersurface->layerSurface->mapped)
        layersurface->layerSurface->mapped = false;

    if (layersurface->layerSurface->surface == g_pCompositor->m_pLastFocus)
        g_pCompositor->m_pLastFocus = nullptr;

    const auto PMONITOR = g_pCompositor->getMonitorFromOutput(layersurface->layerSurface->output);

    if (!PMONITOR)
        return;

    wlr_box geomFixed = {layersurface->geometry.x + PMONITOR->vecPosition.x, layersurface->geometry.y + PMONITOR->vecPosition.y, layersurface->geometry.width, layersurface->geometry.height};
    g_pHyprRenderer->damageBox(&geomFixed);
}

void Events::listener_commitLayerSurface(void* owner, void* data) {
    SLayerSurface* layersurface = (SLayerSurface*)owner;

    if (!layersurface->layerSurface || !layersurface->layerSurface->output)
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromOutput(layersurface->layerSurface->output);

    if (!PMONITOR)
        return;

    wlr_box geomFixed = {layersurface->geometry.x + PMONITOR->vecPosition.x, layersurface->geometry.y + PMONITOR->vecPosition.y, layersurface->geometry.width, layersurface->geometry.height};
    g_pHyprRenderer->damageBox(&geomFixed);

    // fix if it changed its mon
    if ((uint64_t)layersurface->monitorID != PMONITOR->ID) {
        const auto POLDMON = g_pCompositor->getMonitorFromID(layersurface->monitorID);
        POLDMON->m_aLayerSurfaceLists[layersurface->layer].remove(layersurface);
        PMONITOR->m_aLayerSurfaceLists[layersurface->layer].push_back(layersurface);
        layersurface->monitorID = PMONITOR->ID;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(POLDMON->ID);
        g_pHyprRenderer->arrangeLayersForMonitor(POLDMON->ID);
    }

    if (layersurface->layerSurface->current.committed != 0) {
        g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->ID);

        if (layersurface->layer != layersurface->layerSurface->current.layer) {
            PMONITOR->m_aLayerSurfaceLists[layersurface->layer].remove(layersurface);
            PMONITOR->m_aLayerSurfaceLists[layersurface->layerSurface->current.layer].push_back(layersurface);
            layersurface->layer = layersurface->layerSurface->current.layer;
        }

        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PMONITOR->ID);
    }

    layersurface->position = Vector2D(layersurface->geometry.x, layersurface->geometry.y);

    // TODO: optimize this. This does NOT need to be here but it prevents some issues with full DT.
    g_pHyprRenderer->damageMonitor(PMONITOR);
}