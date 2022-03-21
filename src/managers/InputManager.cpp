#include "InputManager.hpp"
#include "../Compositor.hpp"

void CInputManager::onMouseMoved(wlr_event_pointer_motion* e) {
    // TODO: sensitivity

    float sensitivity = g_pConfigManager->getFloat("general:sensitivity");

    wlr_cursor_move(g_pCompositor->m_sWLRCursor, e->device, e->delta_x * sensitivity, e->delta_y * sensitivity);

    mouseMoveUnified(e->time_msec);
    // todo: pointer
}

void CInputManager::onMouseWarp(wlr_event_pointer_motion_absolute* e) {
    wlr_cursor_warp_absolute(g_pCompositor->m_sWLRCursor, e->device, e->x, e->y);

    mouseMoveUnified(e->time_msec);
}

void CInputManager::mouseMoveUnified(uint32_t time) {

    wlr_surface* foundSurface = nullptr;
    Vector2D mouseCoords = getMouseCoordsInternal();
    const auto PMONITOR = g_pCompositor->getMonitorFromCursor();
    Vector2D surfacePos;

    // first, we check if the workspace doesnt have a fullscreen window
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);
    if (PWORKSPACE->hasFullscreenWindow) {
        const auto PFULLSCREENWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->ID);

        // should never ever happen but who knows
        if (PFULLSCREENWINDOW) {
            foundSurface = g_pXWaylandManager->getWindowSurface(PFULLSCREENWINDOW);
            if (foundSurface)
                surfacePos = PFULLSCREENWINDOW->m_vRealPosition;
        } 
    }

    // then surfaces on top
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], &surfacePos);
    
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_TOP], &surfacePos);

    // then windows
    if (!foundSurface && g_pCompositor->vectorToWindowIdeal(mouseCoords)) {
        foundSurface = g_pXWaylandManager->getWindowSurface(g_pCompositor->windowFromCursor());
        if (foundSurface)
            surfacePos = g_pCompositor->windowFromCursor()->m_vRealPosition;
    }
        
    // then surfaces below
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM], &surfacePos);

    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND], &surfacePos);


    if (!foundSurface) {
        wlr_xcursor_manager_set_cursor_image(g_pCompositor->m_sWLRXCursorMgr, "left_ptr", g_pCompositor->m_sWLRCursor);

        wlr_seat_pointer_clear_focus(g_pCompositor->m_sWLRSeat);

        return;
    }

    if (time)
        wlr_idle_notify_activity(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sWLRSeat);

    g_pCompositor->focusSurface(foundSurface);

    Vector2D surfaceLocal = Vector2D(g_pCompositor->m_sWLRCursor->x, g_pCompositor->m_sWLRCursor->y) - surfacePos;

    wlr_seat_pointer_notify_enter(g_pCompositor->m_sWLRSeat, foundSurface, surfaceLocal.x, surfaceLocal.y);
    wlr_seat_pointer_notify_motion(g_pCompositor->m_sWLRSeat, time, surfaceLocal.x, surfaceLocal.y);

    g_pCompositor->m_pLastMonitor = g_pCompositor->getMonitorFromCursor();
    g_pLayoutManager->getCurrentLayout()->onMouseMove(getMouseCoordsInternal());
}

void CInputManager::onMouseButton(wlr_event_pointer_button* e) {
    wlr_idle_notify_activity(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sWLRSeat);

    const auto PKEYBOARD = wlr_seat_get_keyboard(g_pCompositor->m_sWLRSeat);

    switch (e->state) {
        case WLR_BUTTON_PRESSED:
            if ((e->button == BTN_LEFT || e->button == BTN_RIGHT) && wlr_keyboard_get_modifiers(PKEYBOARD) == (uint32_t)g_pConfigManager->getInt("general:main_mod_internal")) {
                currentlyDraggedWindow = g_pCompositor->windowFloatingFromCursor();
                dragButton = e->button;

                g_pLayoutManager->getCurrentLayout()->onBeginDragWindow();

                return;
            }
            break;
        case WLR_BUTTON_RELEASED:
            currentlyDraggedWindow = nullptr;
            dragButton = -1;
            break;
    }

    // notify app if we didnt handle it
    wlr_seat_pointer_notify_button(g_pCompositor->m_sWLRSeat, e->time_msec, e->button, e->state);
}

Vector2D CInputManager::getMouseCoordsInternal() {
    return Vector2D(g_pCompositor->m_sWLRCursor->x, g_pCompositor->m_sWLRCursor->y);
}

void CInputManager::newKeyboard(wlr_input_device* keyboard) {
    m_lKeyboards.push_back(SKeyboard());

    const auto PNEWKEYBOARD = &m_lKeyboards.back();

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

    Debug::log(LOG, "New keyboard created, pointers Hypr: %x and WLR: %x", PNEWKEYBOARD, keyboard);
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

    Debug::log(LOG, "New mouse created, pointer WLR: %x", mouse);
}

void CInputManager::destroyKeyboard(SKeyboard* pKeyboard) {
    wl_list_remove(&pKeyboard->listen_keyboardMod.link);
    wl_list_remove(&pKeyboard->listen_keyboardKey.link);
    wl_list_remove(&pKeyboard->listen_keyboardDestroy.link);

    m_lKeyboards.remove(*pKeyboard);
}

void CInputManager::destroyMouse(wlr_input_device* mouse) {
    //
}

void CInputManager::onKeyboardKey(wlr_event_keyboard_key* e, SKeyboard* pKeyboard) {
    const auto KEYCODE = e->keycode + 8; // Because to xkbcommon it's +8 from libinput

    const xkb_keysym_t* keysyms;
    int syms = xkb_state_key_get_syms(pKeyboard->keyboard->keyboard->xkb_state, KEYCODE, &keysyms);

    const auto MODS = wlr_keyboard_get_modifiers(pKeyboard->keyboard->keyboard);

    wlr_idle_notify_activity(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sWLRSeat);

    bool found = false;
    if (e->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        for (int i = 0; i < syms; ++i)
            found = g_pKeybindManager->handleKeybinds(MODS, keysyms[i]) || found;
    } else if (e->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        // hee hee
    }

    if (!found) {
        wlr_seat_set_keyboard(g_pCompositor->m_sWLRSeat, pKeyboard->keyboard);
        wlr_seat_keyboard_notify_key(g_pCompositor->m_sWLRSeat, e->time_msec, e->keycode, e->state);
    }
}

void CInputManager::onKeyboardMod(void* data, SKeyboard* pKeyboard) {
    wlr_seat_set_keyboard(g_pCompositor->m_sWLRSeat, pKeyboard->keyboard);
    wlr_seat_keyboard_notify_modifiers(g_pCompositor->m_sWLRSeat, &pKeyboard->keyboard->keyboard->modifiers);
}