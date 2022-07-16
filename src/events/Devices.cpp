#include "Events.hpp"

#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/input/InputManager.hpp"
#include "../render/Renderer.hpp"

// ---------------------------------------------------- //
//   _____  ________      _______ _____ ______  _____   //
//  |  __ \|  ____\ \    / /_   _/ ____|  ____|/ ____|  //
//  | |  | | |__   \ \  / /  | || |    | |__  | (___    //
//  | |  | |  __|   \ \/ /   | || |    |  __|  \___ \   //
//  | |__| | |____   \  /   _| || |____| |____ ____) |  //
//  |_____/|______|   \/   |_____\_____|______|_____/   //
//                                                      //
// ---------------------------------------------------- //

void Events::listener_keyboardDestroy(void* owner, void* data) {
    SKeyboard* PKEYBOARD = (SKeyboard*)owner;
    g_pInputManager->destroyKeyboard(PKEYBOARD);

    Debug::log(LOG, "Destroyed keyboard %x", PKEYBOARD);
}

void Events::listener_keyboardKey(void* owner, void* data) {
    SKeyboard* PKEYBOARD = (SKeyboard*)owner;
    g_pInputManager->onKeyboardKey((wlr_keyboard_key_event*)data, PKEYBOARD);
}

void Events::listener_keyboardMod(void* owner, void* data) {
    SKeyboard* PKEYBOARD = (SKeyboard*)owner;
    g_pInputManager->onKeyboardMod(data, PKEYBOARD);
}

void Events::listener_mouseFrame(wl_listener* listener, void* data) {
    wlr_seat_pointer_notify_frame(g_pCompositor->m_sSeat.seat);
}

void Events::listener_mouseMove(wl_listener* listener, void* data) {
    g_pInputManager->onMouseMoved((wlr_pointer_motion_event*)data);
}

void Events::listener_mouseMoveAbsolute(wl_listener* listener, void* data) {
    g_pInputManager->onMouseWarp((wlr_pointer_motion_absolute_event*)data);
}

void Events::listener_mouseButton(wl_listener* listener, void* data) {
    g_pInputManager->onMouseButton((wlr_pointer_button_event*)data);
}

void Events::listener_mouseAxis(wl_listener* listener, void* data) {
    const auto E = (wlr_pointer_axis_event*)data;

    wlr_seat_pointer_notify_axis(g_pCompositor->m_sSeat.seat, E->time_msec, E->orientation, E->delta, E->delta_discrete, E->source);
}

void Events::listener_requestMouse(wl_listener* listener, void* data) {
    const auto EVENT = (wlr_seat_pointer_request_set_cursor_event*)data;

    g_pInputManager->processMouseRequest(EVENT);
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
        case WLR_INPUT_DEVICE_TOUCH:
            Debug::log(LOG, "Attached a touch device with name %s", DEVICE->name);
            Debug::log(WARN, "!!!! Hyprland does not directly support touchscreens, bugs may occur !!!!");
            wlr_cursor_attach_input_device(g_pCompositor->m_sWLRCursor, DEVICE);
            break;
        case WLR_INPUT_DEVICE_TABLET_TOOL:
            Debug::log(LOG, "Attached a tablet tool with name %s", DEVICE->name);
            g_pInputManager->newTabletTool(DEVICE);
            break;
        case WLR_INPUT_DEVICE_TABLET_PAD:
            Debug::log(LOG, "Attached a tablet pad with name %s", DEVICE->name);
            g_pInputManager->newTabletPad(DEVICE);
            break;
        default:
            Debug::log(WARN, "Unrecognized input device plugged in: %s", DEVICE->name);
            break;
    }

    g_pInputManager->updateCapabilities(DEVICE);
}

void Events::listener_newConstraint(wl_listener* listener, void* data) {
    const auto PCONSTRAINT = (wlr_pointer_constraint_v1*)data;

    Debug::log(LOG, "New mouse constraint at %x", PCONSTRAINT);

    g_pInputManager->m_lConstraints.emplace_back();
    const auto CONSTRAINT = &g_pInputManager->m_lConstraints.back();

    CONSTRAINT->pMouse = g_pCompositor->m_sSeat.mouse;
    CONSTRAINT->constraint = PCONSTRAINT;

    CONSTRAINT->hyprListener_destroyConstraint.initCallback(&PCONSTRAINT->events.destroy, &Events::listener_destroyConstraint, CONSTRAINT, "Constraint");
    CONSTRAINT->hyprListener_setConstraintRegion.initCallback(&PCONSTRAINT->events.set_region, &Events::listener_setConstraintRegion, CONSTRAINT, "Constraint");

    if (g_pCompositor->m_pLastFocus == PCONSTRAINT->surface) {
        g_pInputManager->constrainMouse(CONSTRAINT->pMouse, PCONSTRAINT);
    }
}

void Events::listener_destroyConstraint(void* owner, void* data) {
    const auto PCONSTRAINT = (SConstraint*)owner;

    if (PCONSTRAINT->pMouse->currentConstraint == PCONSTRAINT->constraint) {
        PCONSTRAINT->pMouse->hyprListener_commitConstraint.removeCallback();

        const auto PWINDOW = g_pCompositor->getConstraintWindow(g_pCompositor->m_sSeat.mouse);

        if (PWINDOW) {
            if (PWINDOW->m_bIsX11) {
                wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr,
                                PCONSTRAINT->constraint->current.cursor_hint.x + PWINDOW->m_uSurface.xwayland->x, PWINDOW->m_uSurface.xwayland->y + PCONSTRAINT->constraint->current.cursor_hint.y);

                wlr_seat_pointer_warp(PCONSTRAINT->constraint->seat, PCONSTRAINT->constraint->current.cursor_hint.x, PCONSTRAINT->constraint->current.cursor_hint.y);
            } else {
                wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr,
                                PCONSTRAINT->constraint->current.cursor_hint.x + PWINDOW->m_vRealPosition.vec().x, PCONSTRAINT->constraint->current.cursor_hint.y + PWINDOW->m_vRealPosition.vec().y);

                wlr_seat_pointer_warp(PCONSTRAINT->constraint->seat, PCONSTRAINT->constraint->current.cursor_hint.x, PCONSTRAINT->constraint->current.cursor_hint.y);
            }
        }

        PCONSTRAINT->pMouse->currentConstraint = nullptr;
    }

    Debug::log(LOG, "Unconstrained mouse from %x", PCONSTRAINT->constraint);

    g_pInputManager->m_lConstraints.remove(*PCONSTRAINT);
}

void Events::listener_setConstraintRegion(void* owner, void* data) {
    // no
}

void Events::listener_newVirtPtr(wl_listener* listener, void* data) {
    const auto EV = (wlr_virtual_pointer_v1_new_pointer_event*)data;
    const auto POINTER = EV->new_pointer;
    const auto DEVICE = &POINTER->pointer.base;

    g_pInputManager->newMouse(DEVICE, true);
}

void Events::listener_destroyMouse(void* owner, void* data) {
    const auto PMOUSE = (SMouse*)owner;

    g_pInputManager->destroyMouse(PMOUSE->mouse);
}

void Events::listener_swipeBegin(wl_listener* listener, void* data) {
    const auto EVENT = (wlr_pointer_swipe_begin_event*)data;

    g_pInputManager->onSwipeBegin(EVENT);
}

void Events::listener_swipeUpdate(wl_listener* listener, void* data) {
    const auto EVENT = (wlr_pointer_swipe_update_event*)data;

    g_pInputManager->onSwipeUpdate(EVENT);
}

void Events::listener_swipeEnd(wl_listener* listener, void* data) {
    const auto EVENT = (wlr_pointer_swipe_end_event*)data;

    g_pInputManager->onSwipeEnd(EVENT);
}

void Events::listener_pinchBegin(wl_listener* listener, void* data) {
    const auto EV = (wlr_pointer_pinch_begin_event*)data;
    wlr_pointer_gestures_v1_send_pinch_begin(g_pCompositor->m_sWLRPointerGestures, g_pCompositor->m_sSeat.seat, EV->time_msec, EV->fingers);
}

void Events::listener_pinchUpdate(wl_listener* listener, void* data) {
    const auto EV = (wlr_pointer_pinch_update_event*)data;
    wlr_pointer_gestures_v1_send_pinch_update(g_pCompositor->m_sWLRPointerGestures, g_pCompositor->m_sSeat.seat, EV->time_msec, EV->dx, EV->dy, EV->scale, EV->rotation);
}

void Events::listener_pinchEnd(wl_listener* listener, void* data) {
    const auto EV = (wlr_pointer_pinch_end_event*)data;
    wlr_pointer_gestures_v1_send_pinch_end(g_pCompositor->m_sWLRPointerGestures, g_pCompositor->m_sSeat.seat, EV->time_msec, EV->cancelled);
}