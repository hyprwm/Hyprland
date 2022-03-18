#include "InputManager.hpp"
#include "../Compositor.hpp"

void CInputManager::onMouseMoved(wlr_event_pointer_motion* e) {
    // TODO: sensitivity

    float sensitivity = 0.25f;

    m_vMouseCoords = m_vMouseCoords + Vector2D(e->delta_x * sensitivity, e->delta_y * sensitivity);

    if (m_vMouseCoords.floor() != m_vWLRMouseCoords) {
        Vector2D delta = m_vMouseCoords - m_vWLRMouseCoords;
        m_vWLRMouseCoords = m_vMouseCoords.floor();

        wlr_cursor_move(g_pCompositor->m_sWLRCursor, e->device, delta.floor().x, delta.floor().y);
    }


    if (e->time_msec)
        wlr_idle_notify_activity(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sWLRSeat);


    g_pCompositor->focusWindow(g_pCompositor->vectorToWindow(getMouseCoordsInternal()));
    // todo: pointer
}

void CInputManager::onMouseWarp(wlr_event_pointer_motion_absolute* e) {
    wlr_cursor_warp_absolute(g_pCompositor->m_sWLRCursor, e->device, e->x, e->y);

    if (e->time_msec)
        wlr_idle_notify_activity(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sWLRSeat);
}

void CInputManager::onMouseButton(wlr_event_pointer_button* e) {
    wlr_idle_notify_activity(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sWLRSeat);

    switch (e->state) {
        case WLR_BUTTON_PRESSED:
            // todo: keybinds
            break;
        case WLR_BUTTON_RELEASED:
            // todo: keybinds
            break;
    }

    // notify app if we didnt handle it
    wlr_seat_pointer_notify_button(g_pCompositor->m_sWLRSeat, e->time_msec, e->button, e->state);
}

Vector2D CInputManager::getMouseCoordsInternal() {
    return m_vMouseCoords;
}

void CInputManager::newKeyboard(wlr_input_device* keyboard) {
    m_dKeyboards.push_back(SKeyboard());

    const auto PNEWKEYBOARD = &m_dKeyboards.back();

    PNEWKEYBOARD->keyboard = keyboard;

    xkb_rule_names rules;

    const auto CONTEXT = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    const auto KEYMAP = xkb_keymap_new_from_names(CONTEXT, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

    wlr_keyboard_set_keymap(keyboard->keyboard, KEYMAP);
    xkb_keymap_unref(KEYMAP);
    xkb_context_unref(CONTEXT);
    wlr_keyboard_set_repeat_info(keyboard->keyboard, 25, 600);

    wl_signal_add(&keyboard->keyboard->events.modifiers, &PNEWKEYBOARD->listen_keyboardMod);
    wl_signal_add(&keyboard->keyboard->events.key, &PNEWKEYBOARD->listen_keyboardKey);
    wl_signal_add(&keyboard->events.destroy, &PNEWKEYBOARD->listen_keyboardDestroy);

    wlr_seat_set_keyboard(g_pCompositor->m_sWLRSeat, keyboard);
}

void CInputManager::newMouse(wlr_input_device* mouse) {
    if (wlr_input_device_is_libinput(mouse)) {
        const auto LIBINPUTDEV = (libinput_device*)wlr_libinput_get_device_handle(mouse);

        if (libinput_device_config_tap_get_finger_count(LIBINPUTDEV))  // this is for tapping (like on a laptop)
            libinput_device_config_tap_set_enabled(LIBINPUTDEV, LIBINPUT_CONFIG_TAP_ENABLED);

        if (libinput_device_config_scroll_has_natural_scroll(LIBINPUTDEV))
            libinput_device_config_scroll_set_natural_scroll_enabled(LIBINPUTDEV, 0 /* Natural */);
    }

    wlr_cursor_attach_input_device(g_pCompositor->m_sWLRCursor, mouse);
}

void CInputManager::onKeyboardKey(wlr_event_keyboard_key* event) {

}

void CInputManager::onKeyboardMod(void* data) {

}