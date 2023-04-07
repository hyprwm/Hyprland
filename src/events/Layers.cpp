#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/input/InputManager.hpp"
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

        Debug::log(LOG, "New LayerSurface has no preferred monitor. Assigning Monitor %s", PMONITOR->szName.c_str());

        WLRLAYERSURFACE->output = PMONITOR->output;
    }

    auto PMONITOR = (CMonitor*)g_pCompositor->getMonitorFromOutput(WLRLAYERSURFACE->output);

    if (!WLRLAYERSURFACE->output || !PMONITOR || PMONITOR->pMirrorOf) {
        PMONITOR                = g_pCompositor->m_vMonitors.front().get();
        WLRLAYERSURFACE->output = PMONITOR->output; // TODO: current mon
    }

    SLayerSurface* layerSurface = PMONITOR->m_aLayerSurfaceLayers[WLRLAYERSURFACE->pending.layer].emplace_back(std::make_unique<SLayerSurface>()).get();

    layerSurface->szNamespace = WLRLAYERSURFACE->_namespace;

    layerSurface->hyprListener_commitLayerSurface.initCallback(&WLRLAYERSURFACE->surface->events.commit, &Events::listener_commitLayerSurface, layerSurface, "layerSurface");
    layerSurface->hyprListener_destroyLayerSurface.initCallback(&WLRLAYERSURFACE->events.destroy, &Events::listener_destroyLayerSurface, layerSurface, "layerSurface");
    layerSurface->hyprListener_mapLayerSurface.initCallback(&WLRLAYERSURFACE->events.map, &Events::listener_mapLayerSurface, layerSurface, "layerSurface");
    layerSurface->hyprListener_unmapLayerSurface.initCallback(&WLRLAYERSURFACE->events.unmap, &Events::listener_unmapLayerSurface, layerSurface, "layerSurface");
    layerSurface->hyprListener_newPopup.initCallback(&WLRLAYERSURFACE->events.new_popup, &Events::listener_newPopup, layerSurface, "layerSurface");

    layerSurface->layerSurface = WLRLAYERSURFACE;
    layerSurface->layer        = WLRLAYERSURFACE->current.layer;
    WLRLAYERSURFACE->data      = layerSurface;
    layerSurface->monitorID    = PMONITOR->ID;

    layerSurface->forceBlur = g_pConfigManager->shouldBlurLS(layerSurface->szNamespace);

    Debug::log(LOG, "LayerSurface %x (namespace %s layer %d) created on monitor %s", layerSurface->layerSurface, layerSurface->layerSurface->_namespace, layerSurface->layer,
               PMONITOR->szName.c_str());
}

void Events::listener_destroyLayerSurface(void* owner, void* data) {
    SLayerSurface* layersurface = (SLayerSurface*)owner;

    Debug::log(LOG, "LayerSurface %x destroyed", layersurface->layerSurface);

    const auto PMONITOR = g_pCompositor->getMonitorFromID(layersurface->monitorID);

    if (!g_pCompositor->getMonitorFromID(layersurface->monitorID))
        Debug::log(WARN, "Layersurface destroyed on an invalid monitor (removed?)");

    if (!layersurface->fadingOut) {
        if (layersurface->mapped) {
            Debug::log(LOG, "Forcing an unmap of a LS that did a straight destroy!");
            listener_unmapLayerSurface(layersurface, nullptr);
        } else {
            Debug::log(LOG, "Removing LayerSurface that wasn't mapped.");
            layersurface->alpha.setValueAndWarp(0.f);
            layersurface->fadingOut = true;
            g_pCompositor->addToFadingOutSafe(layersurface);
        }
    }

    layersurface->noProcess = true;

    layersurface->hyprListener_commitLayerSurface.removeCallback();
    layersurface->hyprListener_destroyLayerSurface.removeCallback();
    layersurface->hyprListener_mapLayerSurface.removeCallback();
    layersurface->hyprListener_unmapLayerSurface.removeCallback();
    layersurface->hyprListener_newPopup.removeCallback();

    // rearrange to fix the reserved areas
    if (PMONITOR) {
        g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->ID);
        PMONITOR->scheduledRecalc = true;

        // and damage
        wlr_box geomFixed = {layersurface->geometry.x + PMONITOR->vecPosition.x, layersurface->geometry.y + PMONITOR->vecPosition.y, layersurface->geometry.width,
                             layersurface->geometry.height};
        g_pHyprRenderer->damageBox(&geomFixed);
    }

    layersurface->readyToDelete = true;
    layersurface->layerSurface  = nullptr;
}

void Events::listener_mapLayerSurface(void* owner, void* data) {
    SLayerSurface* layersurface = (SLayerSurface*)owner;

    Debug::log(LOG, "LayerSurface %x mapped", layersurface->layerSurface);

    layersurface->layerSurface->mapped = true;
    layersurface->mapped               = true;

    layersurface->surface = layersurface->layerSurface->surface;

    // anim
    layersurface->alpha.setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeIn"));

    // fix if it changed its mon
    const auto PMONITOR = g_pCompositor->getMonitorFromOutput(layersurface->layerSurface->output);

    if (!PMONITOR)
        return;

    layersurface->applyRules();

    if ((uint64_t)layersurface->monitorID != PMONITOR->ID) {
        const auto POLDMON = g_pCompositor->getMonitorFromID(layersurface->monitorID);
        for (auto it = POLDMON->m_aLayerSurfaceLayers[layersurface->layer].begin(); it != POLDMON->m_aLayerSurfaceLayers[layersurface->layer].end(); it++) {
            if (it->get() == layersurface) {
                PMONITOR->m_aLayerSurfaceLayers[layersurface->layer].emplace_back(std::move(*it));
                POLDMON->m_aLayerSurfaceLayers[layersurface->layer].erase(it);
                break;
            }
        }
        layersurface->monitorID   = PMONITOR->ID;
        PMONITOR->scheduledRecalc = true;
        g_pHyprRenderer->arrangeLayersForMonitor(POLDMON->ID);
    }

    g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->ID);

    wlr_surface_send_enter(layersurface->layerSurface->surface, layersurface->layerSurface->output);

    if (layersurface->layerSurface->current.keyboard_interactive &&
        (!g_pCompositor->m_sSeat.mouse || !g_pCompositor->m_sSeat.mouse->currentConstraint)) { // don't focus if constrained
        g_pCompositor->focusSurface(layersurface->layerSurface->surface);

        const auto LOCAL =
            g_pInputManager->getMouseCoordsInternal() - Vector2D(layersurface->geometry.x + PMONITOR->vecPosition.x, layersurface->geometry.y + PMONITOR->vecPosition.y);
        wlr_seat_pointer_notify_enter(g_pCompositor->m_sSeat.seat, layersurface->layerSurface->surface, LOCAL.x, LOCAL.y);
        wlr_seat_pointer_notify_motion(g_pCompositor->m_sSeat.seat, 0, LOCAL.x, LOCAL.y);
    }

    layersurface->position = Vector2D(layersurface->geometry.x, layersurface->geometry.y);

    wlr_box geomFixed = {layersurface->geometry.x + PMONITOR->vecPosition.x, layersurface->geometry.y + PMONITOR->vecPosition.y, layersurface->geometry.width,
                         layersurface->geometry.height};
    g_pHyprRenderer->damageBox(&geomFixed);

    layersurface->alpha.setValue(0);
    layersurface->alpha         = 1.f;
    layersurface->readyToDelete = false;
    layersurface->fadingOut     = false;

    g_pEventManager->postEvent(SHyprIPCEvent{"openlayer", std::string(layersurface->layerSurface->_namespace ? layersurface->layerSurface->_namespace : "")});
    EMIT_HOOK_EVENT("openLayer", layersurface);

    g_pProtocolManager->m_pFractionalScaleProtocolManager->setPreferredScaleForSurface(layersurface->layerSurface->surface, PMONITOR->scale);
}

void Events::listener_unmapLayerSurface(void* owner, void* data) {
    SLayerSurface* layersurface = (SLayerSurface*)owner;

    Debug::log(LOG, "LayerSurface %x unmapped", layersurface->layerSurface);

    g_pEventManager->postEvent(SHyprIPCEvent{"closelayer", std::string(layersurface->layerSurface->_namespace ? layersurface->layerSurface->_namespace : "")});
    EMIT_HOOK_EVENT("closeLayer", layersurface);

    if (!g_pCompositor->getMonitorFromID(layersurface->monitorID) || g_pCompositor->m_bUnsafeState) {
        Debug::log(WARN, "Layersurface unmapping on invalid monitor (removed?) ignoring.");

        g_pCompositor->addToFadingOutSafe(layersurface);

        layersurface->mapped = false;

        layersurface->fadingOut = true;

        layersurface->alpha.setValueAndWarp(0.f);
        return;
    }

    // anim
    layersurface->alpha.setConfig(g_pConfigManager->getAnimationPropertyConfig("fadeOut"));

    // make a snapshot and start fade
    g_pHyprOpenGL->makeLayerSnapshot(layersurface);
    layersurface->alpha = 0.f;

    layersurface->mapped = false;

    layersurface->fadingOut = true;

    g_pCompositor->addToFadingOutSafe(layersurface);

    if (layersurface->layerSurface->mapped)
        layersurface->layerSurface->mapped = false;

    const auto PMONITOR = g_pCompositor->getMonitorFromOutput(layersurface->layerSurface->output);

    const bool WASLASTFOCUS = g_pCompositor->m_pLastFocus == layersurface->layerSurface->surface;

    layersurface->surface = nullptr;

    if (!PMONITOR)
        return;

    // refocus if needed
    if (WASLASTFOCUS) {
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

    wlr_box geomFixed = {layersurface->geometry.x + PMONITOR->vecPosition.x, layersurface->geometry.y + PMONITOR->vecPosition.y, layersurface->geometry.width,
                         layersurface->geometry.height};
    g_pHyprRenderer->damageBox(&geomFixed);

    geomFixed = {layersurface->geometry.x + (int)PMONITOR->vecPosition.x, layersurface->geometry.y + (int)PMONITOR->vecPosition.y,
                 (int)layersurface->layerSurface->surface->current.width, (int)layersurface->layerSurface->surface->current.height};
    g_pHyprRenderer->damageBox(&geomFixed);
}

void Events::listener_commitLayerSurface(void* owner, void* data) {
    SLayerSurface* layersurface = (SLayerSurface*)owner;

    if (!layersurface->layerSurface || !layersurface->layerSurface->output)
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromOutput(layersurface->layerSurface->output);

    if (!PMONITOR)
        return;

    if (layersurface->layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND || layersurface->layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM)
        g_pHyprOpenGL->markBlurDirtyForMonitor(PMONITOR); // so that blur is recalc'd

    wlr_box geomFixed = {layersurface->geometry.x, layersurface->geometry.y, layersurface->geometry.width, layersurface->geometry.height};
    g_pHyprRenderer->damageBox(&geomFixed);

    // fix if it changed its mon
    if ((uint64_t)layersurface->monitorID != PMONITOR->ID) {
        const auto POLDMON = g_pCompositor->getMonitorFromID(layersurface->monitorID);

        for (auto it = POLDMON->m_aLayerSurfaceLayers[layersurface->layer].begin(); it != POLDMON->m_aLayerSurfaceLayers[layersurface->layer].end(); it++) {
            if (it->get() == layersurface) {
                PMONITOR->m_aLayerSurfaceLayers[layersurface->layer].emplace_back(std::move(*it));
                POLDMON->m_aLayerSurfaceLayers[layersurface->layer].erase(it);
                break;
            }
        }

        layersurface->monitorID   = PMONITOR->ID;
        PMONITOR->scheduledRecalc = true;
        g_pHyprRenderer->arrangeLayersForMonitor(POLDMON->ID);
    }

    if (layersurface->layerSurface->current.committed != 0) {
        if (layersurface->layer != layersurface->layerSurface->current.layer) {

            for (auto it = PMONITOR->m_aLayerSurfaceLayers[layersurface->layer].begin(); it != PMONITOR->m_aLayerSurfaceLayers[layersurface->layer].end(); it++) {
                if (it->get() == layersurface) {
                    PMONITOR->m_aLayerSurfaceLayers[layersurface->layerSurface->current.layer].emplace_back(std::move(*it));
                    PMONITOR->m_aLayerSurfaceLayers[layersurface->layer].erase(it);
                    break;
                }
            }

            layersurface->layer = layersurface->layerSurface->current.layer;

            if (layersurface->layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND || layersurface->layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM)
                g_pHyprOpenGL->markBlurDirtyForMonitor(PMONITOR); // so that blur is recalc'd
        }

        g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->ID);

        PMONITOR->scheduledRecalc = true;
    } else {
        layersurface->position = Vector2D(layersurface->geometry.x, layersurface->geometry.y);

        // update geom if it changed
        if (layersurface->layerSurface->surface->current.scale == 1 && PMONITOR->scale != 1.f && layersurface->layerSurface->surface->current.viewport.has_dst) {
            // fractional scaling. Dirty hack.
            layersurface->geometry = {layersurface->geometry.x, layersurface->geometry.y, (int)(layersurface->layerSurface->surface->current.viewport.dst_width),
                                      (int)(layersurface->layerSurface->surface->current.viewport.dst_height)};
        } else {
            // this is because some apps like e.g. rofi-lbonn can't fucking use the protocol correctly.
            layersurface->geometry = {layersurface->geometry.x, layersurface->geometry.y, (int)layersurface->layerSurface->surface->current.width,
                                      (int)layersurface->layerSurface->surface->current.height};
        }
    }

    g_pHyprRenderer->damageSurface(layersurface->layerSurface->surface, layersurface->position.x, layersurface->position.y);

    g_pProtocolManager->m_pFractionalScaleProtocolManager->setPreferredScaleForSurface(layersurface->layerSurface->surface, PMONITOR->scale);
}
