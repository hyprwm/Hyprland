#include "Events.hpp"
#include "../input/InputManager.hpp"
#include "../Compositor.hpp"

void Events::listener_activate(wl_listener* listener, void* data) {
    
}

void Events::listener_change(wl_listener* listener, void* data) {
    // layout got changed, let's update monitors.
    const auto CONFIG = wlr_output_configuration_v1_create();

    const auto GEOMETRY = wlr_output_layout_get_box(g_pCompositor->m_sWLROutputLayout, NULL);
}

void Events::listener_newOutput(wl_listener* listener, void* data) {
    // new monitor added, let's accomodate for that.
    const auto OUTPUT = (wlr_output*)data;

    SMonitor newMonitor;

    wlr_output_init_render(OUTPUT, g_pCompositor->m_sWLRAllocator, g_pCompositor->m_sWLRRenderer);

    // get monitor rule that matches
    SMonitorRule monitorRule = g_pConfigManager->getMonitorRuleFor(OUTPUT->name);

    wlr_output_set_scale(OUTPUT, monitorRule.scale);
    wlr_xcursor_manager_load(g_pCompositor->m_sWLRXCursorMgr, monitorRule.scale);
    wlr_output_set_transform(OUTPUT, WL_OUTPUT_TRANSFORM_NORMAL); // TODO: support other transforms

    wlr_output_set_mode(OUTPUT, wlr_output_preferred_mode(OUTPUT));
    wlr_output_enable_adaptive_sync(OUTPUT, 1);

    wl_signal_add(&OUTPUT->events.frame, &Events::listen_monitorFrame);
    wl_signal_add(&OUTPUT->events.destroy, &Events::listen_monitorDestroy);

    wlr_output_enable(OUTPUT, 1);
    if (!wlr_output_commit(OUTPUT)) {
        Debug::log(ERR, "Couldn't commit output named %s", OUTPUT->name);
        return;
    }

    wlr_output_layout_add(g_pCompositor->m_sWLROutputLayout, OUTPUT, monitorRule.offset.x, monitorRule.offset.y);

    // add new monitor to our internal arr
    newMonitor.ID = g_pCompositor->m_vMonitors.size();
    newMonitor.szName = OUTPUT->name;
    newMonitor.vecPosition = monitorRule.offset;
    newMonitor.vecSize = monitorRule.resolution;
    
    g_pCompositor->m_vMonitors.push_back(newMonitor);
}

void Events::listener_monitorFrame(wl_listener* listener, void* data) {

}

void Events::listener_monitorDestroy(wl_listener* listener, void* data) {

}

void Events::listener_mouseAxis(wl_listener* listener, void* data) {

}

void Events::listener_mouseButton(wl_listener* listener, void* data) {
    
}

void Events::listener_mouseFrame(wl_listener* listener, void* data) {

}

void Events::listener_mouseMove(wl_listener* listener, void* data) {
    g_pInputManager->onMouseMoved((wlr_event_pointer_motion*)data);
}

void Events::listener_mouseMoveAbsolute(wl_listener* listener, void* data) {
    
}

void Events::listener_newInput(wl_listener* listener, void* data) {

}

void Events::listener_newKeyboard(wl_listener* listener, void* data) {

}

void Events::listener_newLayerSurface(wl_listener* listener, void* data) {
    
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