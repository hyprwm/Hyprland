#include "Events.hpp"

#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/InputManager.hpp"
#include "../render/Renderer.hpp"

void Events::listener_activate(wl_listener* listener, void* data) {
    // TODO
}

void Events::listener_change(wl_listener* listener, void* data) {
    // layout got changed, let's update monitors.
    const auto CONFIG = wlr_output_configuration_v1_create();

    for (auto& m : g_pCompositor->m_lMonitors) {
        const auto CONFIGHEAD = wlr_output_configuration_head_v1_create(CONFIG, m.output);

        // TODO: clients off of disabled
        wlr_box BOX;
        wlr_output_layout_get_box(g_pCompositor->m_sWLROutputLayout, m.output, &BOX);

        m.vecSize.x = BOX.width;
        m.vecSize.y = BOX.height;
        m.vecPosition.x = BOX.x;
        m.vecPosition.y = BOX.y;

        CONFIGHEAD->state.enabled = m.output->enabled;
        CONFIGHEAD->state.mode = m.output->current_mode;
        CONFIGHEAD->state.x = m.vecPosition.x;
        CONFIGHEAD->state.y = m.vecPosition.y;

        wlr_output_set_custom_mode(m.output, m.vecSize.x, m.vecSize.y, (int)(round(m.refreshRate * 1000)));
    }

    wlr_output_manager_v1_set_configuration(g_pCompositor->m_sWLROutputMgr, CONFIG);
}

void Events::listener_newOutput(wl_listener* listener, void* data) {
    // new monitor added, let's accomodate for that.
    const auto OUTPUT = (wlr_output*)data;

    SMonitor newMonitor;
    newMonitor.output = OUTPUT;
    newMonitor.ID = g_pCompositor->m_lMonitors.size();
    newMonitor.szName = OUTPUT->name;

    wlr_output_init_render(OUTPUT, g_pCompositor->m_sWLRAllocator, g_pCompositor->m_sWLRRenderer);

    // get monitor rule that matches
    SMonitorRule monitorRule = g_pConfigManager->getMonitorRuleFor(OUTPUT->name);

    wlr_output_set_scale(OUTPUT, monitorRule.scale);
    wlr_xcursor_manager_load(g_pCompositor->m_sWLRXCursorMgr, monitorRule.scale);
    wlr_output_set_transform(OUTPUT, WL_OUTPUT_TRANSFORM_NORMAL); // TODO: support other transforms

    wlr_output_set_mode(OUTPUT, wlr_output_preferred_mode(OUTPUT));
    wlr_output_enable_adaptive_sync(OUTPUT, 1);

    // create it in the arr
    newMonitor.vecPosition = monitorRule.offset;
    newMonitor.vecSize = monitorRule.resolution;
    newMonitor.refreshRate = monitorRule.refreshRate;
    g_pCompositor->m_lMonitors.push_back(newMonitor);
    //

    wl_signal_add(&OUTPUT->events.frame, &g_pCompositor->m_lMonitors.back().listen_monitorFrame);
    wl_signal_add(&OUTPUT->events.destroy, &g_pCompositor->m_lMonitors.back().listen_monitorDestroy);

    wlr_output_enable(OUTPUT, 1);
    if (!wlr_output_commit(OUTPUT)) {
        Debug::log(ERR, "Couldn't commit output named %s", OUTPUT->name);
        return;
    }

    wlr_output_layout_add(g_pCompositor->m_sWLROutputLayout, OUTPUT, monitorRule.offset.x, monitorRule.offset.y);

    wlr_output_set_custom_mode(OUTPUT, OUTPUT->width, OUTPUT->height, (int)(round(monitorRule.refreshRate * 1000)));

    Debug::log(LOG, "Added new monitor with name %s at %i,%i with size %ix%i@%i, pointer %x", OUTPUT->name, (int)monitorRule.offset.x, (int)monitorRule.offset.y, (int)monitorRule.resolution.x, (int)monitorRule.resolution.y, (int)monitorRule.refreshRate, OUTPUT);
}

void Events::listener_monitorFrame(wl_listener* listener, void* data) {
    SMonitor* const PMONITOR = wl_container_of(listener, PMONITOR, listen_monitorFrame);

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    const float bgcol[4] = {0.1f,0.1f,0.1f,1.f};

    if (!wlr_output_attach_render(PMONITOR->output, nullptr))
        return;

    wlr_renderer_begin(g_pCompositor->m_sWLRRenderer, PMONITOR->vecSize.x, PMONITOR->vecSize.y);
    wlr_renderer_clear(g_pCompositor->m_sWLRRenderer, bgcol);

    g_pHyprRenderer->renderAllClientsForMonitor(PMONITOR->ID, &now);

    wlr_output_render_software_cursors(PMONITOR->output, NULL);

    wlr_renderer_end(g_pCompositor->m_sWLRRenderer);

    wlr_output_commit(PMONITOR->output);
}

void Events::listener_monitorDestroy(wl_listener* listener, void* data) {
    const auto OUTPUT = (wlr_output*)data;

    SMonitor* pMonitor = nullptr;

    for (auto& m : g_pCompositor->m_lMonitors) {
        if (m.szName == OUTPUT->name) {
            pMonitor = &m;
            break;
        }
    }

    if (!pMonitor)
        return;

    g_pCompositor->m_lMonitors.remove(*pMonitor);

    // TODO: cleanup windows
}

void Events::listener_newLayerSurface(wl_listener* listener, void* data) {
    const auto WLRLAYERSURFACE = (wlr_layer_surface_v1*)data;

    if (!WLRLAYERSURFACE->output->data) {
        Debug::log(LOG, "New LayerSurface has no preferred monitor.");
    }

    const auto PMONITOR = (SMonitor*)(WLRLAYERSURFACE->output->data ? WLRLAYERSURFACE->output->data : g_pCompositor->getMonitorFromCursor());
    PMONITOR->m_aLayerSurfaceLists[WLRLAYERSURFACE->pending.layer].push_back(new SLayerSurface());
    SLayerSurface* layerSurface = PMONITOR->m_aLayerSurfaceLists[WLRLAYERSURFACE->pending.layer].back();

    if (!WLRLAYERSURFACE->output) {
        WLRLAYERSURFACE->output = g_pCompositor->m_lMonitors.front().output; // TODO: current mon
    }

    wl_signal_add(&WLRLAYERSURFACE->surface->events.commit, &layerSurface->listen_commitLayerSurface);
    wl_signal_add(&WLRLAYERSURFACE->surface->events.destroy, &layerSurface->listen_destroyLayerSurface);
    wl_signal_add(&WLRLAYERSURFACE->events.map, &layerSurface->listen_mapLayerSurface);
    wl_signal_add(&WLRLAYERSURFACE->events.unmap, &layerSurface->listen_unmapLayerSurface);

    layerSurface->layerSurface = WLRLAYERSURFACE;
    WLRLAYERSURFACE->data = layerSurface;
    layerSurface->monitorID = PMONITOR->ID;

    // todo: arrange
    Debug::log(LOG, "LayerSurface %x created on monitor %s", layerSurface, PMONITOR->szName.c_str());
}

void Events::listener_destroyLayerSurface(wl_listener* listener, void* data) {
    SLayerSurface* layersurface = wl_container_of(listener, layersurface, listen_destroyLayerSurface);

    if (layersurface->layerSurface->mapped)
        layersurface->layerSurface->mapped = 0;


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

    Debug::log(LOG, "LayerSurface %x mapped", layersurface);
}

void Events::listener_unmapLayerSurface(wl_listener* listener, void* data) {
    SLayerSurface* layersurface = wl_container_of(listener, layersurface, listen_unmapLayerSurface);

    if (layersurface->layerSurface->mapped)
        layersurface->layerSurface->mapped = 0;

    Debug::log(LOG, "LayerSurface %x unmapped", layersurface);
}

void Events::listener_commitLayerSurface(wl_listener* listener, void* data) {
    SLayerSurface* layersurface = wl_container_of(listener, layersurface, listen_commitLayerSurface);

    if (!layersurface->layerSurface->output)
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromOutput(layersurface->layerSurface->output);

    if (!PMONITOR)
        return;

    // fix if it changed its mon
    if (layersurface->monitorID != PMONITOR->ID) {
        const auto POLDMON = g_pCompositor->getMonitorFromID(layersurface->monitorID);
        POLDMON->m_aLayerSurfaceLists[layersurface->layer].remove(layersurface);
        PMONITOR->m_aLayerSurfaceLists[layersurface->layer].push_back(layersurface);
        layersurface->monitorID = PMONITOR->ID;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(POLDMON->ID);
    }

    g_pHyprRenderer->arrangeLayersForMonitor(PMONITOR->ID);

    if (layersurface->layer != layersurface->layerSurface->current.layer) {
        PMONITOR->m_aLayerSurfaceLists[layersurface->layer].remove(layersurface);
        PMONITOR->m_aLayerSurfaceLists[layersurface->layerSurface->current.layer].push_back(layersurface);
        layersurface->layer = layersurface->layerSurface->current.layer;
    }

    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PMONITOR->ID);
}

void Events::listener_mapWindow(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_mapWindow);

    const auto PMONITOR = g_pCompositor->getMonitorFromCursor();
    PWINDOW->m_iMonitorID = PMONITOR->ID;
    PWINDOW->m_bMappedX11 = true;

    g_pLayoutManager->getCurrentLayout()->onWindowCreated(PWINDOW);
    
    g_pCompositor->focusWindow(PWINDOW);

    Debug::log(LOG, "Map request dispatched, monitor %s, xywh: %f %f %f %f", PMONITOR->szName.c_str(), PWINDOW->m_vRealPosition.x, PWINDOW->m_vRealPosition.y, PWINDOW->m_vRealSize.x, PWINDOW->m_vRealSize.y);
}

void Events::listener_unmapWindow(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_unmapWindow);

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(PWINDOW);

    g_pCompositor->removeWindowFromVectorSafe(PWINDOW);

    Debug::log(LOG, "Window %x unmapped", PWINDOW);
}

void Events::listener_commitWindow(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_commitWindow);

    PWINDOW;
}

void Events::listener_destroyWindow(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_destroyWindow);

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(PWINDOW);

    g_pCompositor->removeWindowFromVectorSafe(PWINDOW);

    Debug::log(LOG, "Window %x destroyed", PWINDOW);
}

void Events::listener_setTitleWindow(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_setTitleWindow);

    PWINDOW->m_szTitle = g_pXWaylandManager->getTitle(PWINDOW);

    Debug::log(LOG, "Window %x set title to %s", PWINDOW, PWINDOW->m_szTitle.c_str());
}

void Events::listener_fullscreenWindow(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_fullscreenWindow);

    PWINDOW->m_bIsFullscreen = !PWINDOW->m_bIsFullscreen;

    // todo: do it

    Debug::log(LOG, "Window %x fullscreen to %i", PWINDOW, PWINDOW->m_bIsFullscreen);
}

void Events::listener_mouseAxis(wl_listener* listener, void* data) {
    const auto E = (wlr_event_pointer_axis*)data;

    wlr_seat_pointer_notify_axis(g_pCompositor->m_sWLRSeat, E->time_msec, E->orientation, E->delta, E->delta_discrete, E->source);
}

void Events::listener_activateX11(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_activateX11);

    if (PWINDOW->m_iX11Type == 1 /* Managed */) {
        wlr_xwayland_surface_activate(PWINDOW->m_uSurface.xwayland, 1);
    }
}

void Events::listener_configureX11(wl_listener* listener, void* data) {
    CWindow* PWINDOW = wl_container_of(listener, PWINDOW, listen_configureX11);

    const auto E = (wlr_xwayland_surface_configure_event*)data;

    // TODO: ignore if tiled?
    wlr_xwayland_surface_configure(PWINDOW->m_uSurface.xwayland, E->x, E->y, E->width, E->height);
}

void Events::listener_readyXWayland(wl_listener* listener, void* data) {
    const auto XCBCONNECTION = xcb_connect(g_pXWaylandManager->m_sWLRXWayland->display_name, NULL);
    const auto ERR = xcb_connection_has_error(XCBCONNECTION);
    if (ERR) {
        Debug::log(LogLevel::ERR, "xcb_connection_has_error failed with %i", ERR);
        return;
    }

    // TODO: atoms

    wlr_xwayland_set_seat(g_pXWaylandManager->m_sWLRXWayland, g_pCompositor->m_sWLRSeat);

    const auto XCURSOR = wlr_xcursor_manager_get_xcursor(g_pCompositor->m_sWLRXCursorMgr, "left_ptr", 1);
    if (XCURSOR) {
        wlr_xwayland_set_cursor(g_pXWaylandManager->m_sWLRXWayland, XCURSOR->images[0]->buffer, XCURSOR->images[0]->width * 4, XCURSOR->images[0]->width, XCURSOR->images[0]->height, XCURSOR->images[0]->hotspot_x, XCURSOR->images[0]->hotspot_y);
    }

    xcb_disconnect(XCBCONNECTION);
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

void Events::listener_keyboardDestroy(wl_listener* listener, void* data) {
    SKeyboard* PKEYBOARD = wl_container_of(listener, PKEYBOARD, listen_keyboardDestroy);
    g_pInputManager->destroyKeyboard(PKEYBOARD);

    Debug::log(LOG, "Destroyed keyboard %x", PKEYBOARD);
}

void Events::listener_keyboardKey(wl_listener* listener, void* data) {
    SKeyboard* PKEYBOARD = wl_container_of(listener, PKEYBOARD, listen_keyboardKey);
    g_pInputManager->onKeyboardKey((wlr_event_keyboard_key*)data, PKEYBOARD);
}

void Events::listener_keyboardMod(wl_listener* listener, void* data) {
    SKeyboard* PKEYBOARD = wl_container_of(listener, PKEYBOARD, listen_keyboardMod);
    g_pInputManager->onKeyboardMod(data, PKEYBOARD);
}

void Events::listener_mouseFrame(wl_listener* listener, void* data) {
    wlr_seat_pointer_notify_frame(g_pCompositor->m_sWLRSeat);
}

void Events::listener_mouseMove(wl_listener* listener, void* data) {
    g_pInputManager->onMouseMoved((wlr_event_pointer_motion*)data);
}

void Events::listener_mouseMoveAbsolute(wl_listener* listener, void* data) {
    g_pInputManager->onMouseWarp((wlr_event_pointer_motion_absolute*)data);
}

void Events::listener_mouseButton(wl_listener* listener, void* data) {
    g_pInputManager->onMouseButton((wlr_event_pointer_button*)data);
}

void Events::listener_newInput(wl_listener* listener, void* data) {
    const auto DEVICE = (wlr_input_device*)data;

    switch(DEVICE->type) {
        case WLR_INPUT_DEVICE_KEYBOARD:
            Debug::log(LOG, "Attached a keyboard with name %s", DEVICE->name);
            g_pInputManager->newKeyboard(DEVICE);
            break;
        case WLR_INPUT_DEVICE_POINTER:
            Debug::log(LOG, "Attached a mouse with name %s", DEVICE->name);
            g_pInputManager->newMouse(DEVICE);
            break;
        default:
            break;
    }

    uint32_t capabilities = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD;

    wlr_seat_set_capabilities(g_pCompositor->m_sWLRSeat, capabilities);
}

void Events::listener_newXDGSurface(wl_listener* listener, void* data) {
    // A window got opened
    const auto XDGSURFACE = (wlr_xdg_surface*)data;

    if (XDGSURFACE->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
        return; // TODO: handle?

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

void Events::listener_outputMgrApply(wl_listener* listener, void* data) {
    const auto CONFIG = (wlr_output_configuration_v1*)data;
    g_pHyprRenderer->outputMgrApplyTest(CONFIG, false);
}

void Events::listener_outputMgrTest(wl_listener* listener, void* data) {
    const auto CONFIG = (wlr_output_configuration_v1*)data;
    g_pHyprRenderer->outputMgrApplyTest(CONFIG, true);
}

void Events::listener_requestMouse(wl_listener* listener, void* data) {
    const auto EVENT = (wlr_seat_pointer_request_set_cursor_event*)data;

    if (EVENT->seat_client == g_pCompositor->m_sWLRSeat->pointer_state.focused_client)
        wlr_cursor_set_surface(g_pCompositor->m_sWLRCursor, EVENT->surface, EVENT->hotspot_x, EVENT->hotspot_y);
}

void Events::listener_requestSetPrimarySel(wl_listener* listener, void* data) {
    const auto EVENT = (wlr_seat_request_set_primary_selection_event*)data;
    wlr_seat_set_primary_selection(g_pCompositor->m_sWLRSeat, EVENT->source, EVENT->serial);
}

void Events::listener_requestSetSel(wl_listener* listener, void* data) {
    const auto EVENT = (wlr_seat_request_set_selection_event*)data;
    wlr_seat_set_selection(g_pCompositor->m_sWLRSeat, EVENT->source, EVENT->serial);
}