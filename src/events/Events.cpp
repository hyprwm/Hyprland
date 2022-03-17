#include "Events.hpp"

#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../input/InputManager.hpp"

void Events::listener_activate(wl_listener* listener, void* data) {
    // TODO
}

void Events::listener_change(wl_listener* listener, void* data) {
    // layout got changed, let's update monitors.
    const auto CONFIG = wlr_output_configuration_v1_create();

    for (auto& m : g_pCompositor->m_vMonitors) {
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
    }

    wlr_output_manager_v1_set_configuration(g_pCompositor->m_sWLROutputMgr, CONFIG);
}

void Events::listener_newOutput(wl_listener* listener, void* data) {
    // new monitor added, let's accomodate for that.
    const auto OUTPUT = (wlr_output*)data;

    SMonitor newMonitor;
    newMonitor.output = OUTPUT;
    newMonitor.ID = g_pCompositor->m_vMonitors.size();
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
    g_pCompositor->m_vMonitors.push_back(newMonitor);
    //

    wl_signal_add(&OUTPUT->events.frame, &g_pCompositor->m_vMonitors[g_pCompositor->m_vMonitors.size() - 1].listen_monitorFrame);
    wl_signal_add(&OUTPUT->events.destroy, &g_pCompositor->m_vMonitors[g_pCompositor->m_vMonitors.size() - 1].listen_monitorDestroy);

    wlr_output_enable(OUTPUT, 1);
    if (!wlr_output_commit(OUTPUT)) {
        Debug::log(ERR, "Couldn't commit output named %s", OUTPUT->name);
        return;
    }

    wlr_output_layout_add(g_pCompositor->m_sWLROutputLayout, OUTPUT, monitorRule.offset.x, monitorRule.offset.y);

    Debug::log(LOG, "Added new monitor with name %s at %i,%i with size %ix%i, pointer %x", OUTPUT->name, (int)monitorRule.offset.x, (int)monitorRule.offset.y, (int)monitorRule.resolution.x, (int)monitorRule.resolution.y, OUTPUT);
}

void Events::listener_monitorFrame(wl_listener* listener, void* data) {
    SMonitor* const PMONITOR = wl_container_of(listener, PMONITOR, listen_monitorFrame);

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    const float bgcol[4] = {0.1f,0.1f,0.1f,1.f};
    const float rectcol[4] = {0.69f,0.1f,0.69f,1.f};

    if (!wlr_output_attach_render(PMONITOR->output, nullptr))
        return;

    wlr_renderer_begin(g_pCompositor->m_sWLRRenderer, PMONITOR->vecSize.x, PMONITOR->vecSize.y);
    wlr_renderer_clear(g_pCompositor->m_sWLRRenderer, bgcol);

    // TODO: render clients

    wlr_output_render_software_cursors(PMONITOR->output, NULL);

    wlr_renderer_end(g_pCompositor->m_sWLRRenderer);

    wlr_output_commit(PMONITOR->output);
}

void Events::listener_monitorDestroy(wl_listener* listener, void* data) {
    const auto OUTPUT = (wlr_output*)data;

    const auto clone = g_pCompositor->m_vMonitors;

    g_pCompositor->m_vMonitors.clear();

    for (auto& m : clone) {
        if (m.szName != OUTPUT->name) 
            g_pCompositor->m_vMonitors.push_back(m);
    }

    // TODO: cleanup windows
}

void Events::listener_newLayerSurface(wl_listener* listener, void* data) {
    const auto WLRLAYERSURFACE = (wlr_layer_surface_v1*)data;

    const auto PMONITOR = (SMonitor*)WLRLAYERSURFACE->output->data;
    PMONITOR->m_dLayerSurfaces.push_back(SLayerSurface());
    SLayerSurface* layerSurface = &PMONITOR->m_dLayerSurfaces[PMONITOR->m_dLayerSurfaces.size() - 1];
    wlr_layer_surface_v1_state oldState;

    if (!WLRLAYERSURFACE->output) {
        WLRLAYERSURFACE->output = g_pCompositor->m_vMonitors[0].output; // TODO: current mon
    }

    wl_signal_add(&WLRLAYERSURFACE->surface->events.commit, &layerSurface->listen_commitLayerSurface);
    wl_signal_add(&WLRLAYERSURFACE->surface->events.destroy, &layerSurface->listen_destroyLayerSurface);
    wl_signal_add(&WLRLAYERSURFACE->events.map, &layerSurface->listen_mapLayerSurface);
    wl_signal_add(&WLRLAYERSURFACE->events.unmap, &layerSurface->listen_unmapLayerSurface);

    layerSurface->layerSurface = WLRLAYERSURFACE;
    WLRLAYERSURFACE->data = layerSurface;

    // todo: arrange
}

void Events::listener_destroyLayerSurface(wl_listener* listener, void* data) {
    
}

void Events::listener_mapLayerSurface(wl_listener* listener, void* data) {

}

void Events::listener_unmapLayerSurface(wl_listener* listener, void* data) {

}

void Events::listener_commitLayerSurface(wl_listener* listener, void* data) {
    
}


void Events::listener_mouseAxis(wl_listener* listener, void* data) {
    // TODO:
}

void Events::listener_mouseButton(wl_listener* listener, void* data) {
    
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

void Events::listener_newInput(wl_listener* listener, void* data) {
    const auto DEVICE = (wlr_input_device*)data;

    switch(DEVICE->type) {
        case WLR_INPUT_DEVICE_KEYBOARD:
            Debug::log(LOG, "Attached a keyboard with name %s", DEVICE->name);
            // TODO:
            break;
        case WLR_INPUT_DEVICE_POINTER:
            Debug::log(LOG, "Attached a mouse with name %s", DEVICE->name);
            wlr_cursor_attach_input_device(g_pCompositor->m_sWLRCursor, DEVICE);
            break;
        default:
            break;
    }

    uint32_t capabilities = WL_SEAT_CAPABILITY_POINTER;
    
    // todo: keyboard caps

    wlr_seat_set_capabilities(g_pCompositor->m_sWLRSeat, capabilities);
}

void Events::listener_newKeyboard(wl_listener* listener, void* data) {

}

void Events::listener_newXDGSurface(wl_listener* listener, void* data) {
    
}

void Events::listener_outputMgrApply(wl_listener* listener, void* data) {

}

void Events::listener_outputMgrTest(wl_listener* listener, void* data) {

}

void Events::listener_requestMouse(wl_listener* listener, void* data) {
    
}

void Events::listener_requestSetPrimarySel(wl_listener* listener, void* data) {

}

void Events::listener_requestSetSel(wl_listener* listener, void* data) {

}