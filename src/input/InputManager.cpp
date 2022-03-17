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
}

void CInputManager::onMouseWarp(wlr_event_pointer_motion_absolute* e) {
    wlr_cursor_warp_absolute(g_pCompositor->m_sWLRCursor, e->device, e->x, e->y);
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

void CInputManager::onKeyboardKey(wlr_event_keyboard_key* event) {

}

void CInputManager::onKeyboardMod(void* data) {

}