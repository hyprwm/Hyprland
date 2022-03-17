#include "Events.hpp"
#include "../input/InputManager.hpp"

void Events::listener_activate(wl_listener* listener, void* data) {
    
}

void Events::listener_change(wl_listener* listener, void* data) {

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

void Events::listener_newOutput(wl_listener* listener, void* data) {

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