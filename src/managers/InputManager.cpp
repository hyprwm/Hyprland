#include "InputManager.hpp"
#include "../Compositor.hpp"

void CInputManager::onMouseMoved(wlr_pointer_motion_event* e) {
    // TODO: sensitivity

    float sensitivity = g_pConfigManager->getFloat("general:sensitivity");

    wlr_cursor_move(g_pCompositor->m_sWLRCursor, &e->pointer->base, e->delta_x * sensitivity, e->delta_y * sensitivity);

    mouseMoveUnified(e->time_msec);
    // todo: pointer
}

void CInputManager::onMouseWarp(wlr_pointer_motion_absolute_event* e) {
    wlr_cursor_warp_absolute(g_pCompositor->m_sWLRCursor, &e->pointer->base, e->x, e->y);

    mouseMoveUnified(e->time_msec);
}

void CInputManager::mouseMoveUnified(uint32_t time) {

    wlr_surface* foundSurface = nullptr;
    Vector2D mouseCoords = getMouseCoordsInternal();
    const auto PMONITOR = g_pCompositor->getMonitorFromCursor();
    if (PMONITOR)
        g_pCompositor->m_pLastMonitor = PMONITOR;
    Vector2D surfaceCoords;
    Vector2D surfacePos = Vector2D(-1337, -1337);

    // first, we check if the workspace doesnt have a fullscreen window
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);
    if (PWORKSPACE->hasFullscreenWindow) {
        const auto PFULLSCREENWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->ID);

        // should never ever happen but who knows
        if (PFULLSCREENWINDOW) {
            foundSurface = g_pXWaylandManager->getWindowSurface(PFULLSCREENWINDOW);
            if (foundSurface)
                surfacePos = PFULLSCREENWINDOW->m_vRealPosition;

            for (auto& w : g_pCompositor->m_lWindows) {
                wlr_box box = {w.m_vRealPosition.x, w.m_vRealPosition.y, w.m_vRealSize.x, w.m_vRealSize.y};
                if (w.m_iWorkspaceID == PFULLSCREENWINDOW->m_iWorkspaceID && w.m_bIsMapped && w.m_bCreatedOverFullscreen && wlr_box_contains_point(&box, mouseCoords.x, mouseCoords.y)) {
                    foundSurface = g_pXWaylandManager->getWindowSurface(&w);
                    if (foundSurface)
                        surfacePos = w.m_vRealPosition;
                    break;
                }
            }
        } 
    }

    // then surfaces on top
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], &surfaceCoords);
    
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_TOP], &surfaceCoords);

    // then windows
    const auto PWINDOWIDEAL = g_pCompositor->vectorToWindowIdeal(mouseCoords);
    if (!foundSurface && PWINDOWIDEAL) {
        foundSurface = g_pXWaylandManager->getWindowSurface(PWINDOWIDEAL);
        if (foundSurface)
            surfacePos = PWINDOWIDEAL->m_vRealPosition;
    }
        
    // then surfaces below
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM], &surfaceCoords);

    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND], &surfaceCoords);


    if (!foundSurface) {
        wlr_xcursor_manager_set_cursor_image(g_pCompositor->m_sWLRXCursorMgr, "left_ptr", g_pCompositor->m_sWLRCursor);

        wlr_seat_pointer_clear_focus(g_pCompositor->m_sSeat.seat);

        return;
    }

    if (time)
        wlr_idle_notify_activity(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sSeat.seat);

    Vector2D surfaceLocal = surfacePos == Vector2D(-1337, -1337) ? surfaceCoords : Vector2D(g_pCompositor->m_sWLRCursor->x, g_pCompositor->m_sWLRCursor->y) - surfacePos;

    wlr_seat_pointer_notify_enter(g_pCompositor->m_sSeat.seat, foundSurface, surfaceLocal.x, surfaceLocal.y);
    wlr_seat_pointer_notify_motion(g_pCompositor->m_sSeat.seat, time, surfaceLocal.x, surfaceLocal.y);

    g_pCompositor->focusSurface(foundSurface);

    g_pLayoutManager->getCurrentLayout()->onMouseMove(getMouseCoordsInternal());
}

void CInputManager::onMouseButton(wlr_pointer_button_event* e) {
    wlr_idle_notify_activity(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sSeat.seat);

    const auto PKEYBOARD = wlr_seat_get_keyboard(g_pCompositor->m_sSeat.seat);

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

    refocus();

    // notify app if we didnt handle it
    if (g_pCompositor->doesSeatAcceptInput(g_pCompositor->m_pLastFocus)) {
        wlr_seat_pointer_notify_button(g_pCompositor->m_sSeat.seat, e->time_msec, e->button, e->state);
        Debug::log(LOG, "Seat notified of button %i (state %i) on surface %x", e->button, e->state, g_pCompositor->m_pLastFocus);
    }
        
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

    PNEWKEYBOARD->hyprListener_keyboardMod.initCallback(&keyboard->keyboard->events.modifiers, &Events::listener_keyboardMod, PNEWKEYBOARD, "Keyboard");
    PNEWKEYBOARD->hyprListener_keyboardKey.initCallback(&keyboard->keyboard->events.key, &Events::listener_keyboardKey, PNEWKEYBOARD, "Keyboard");
    PNEWKEYBOARD->hyprListener_keyboardDestroy.initCallback(&keyboard->events.destroy, &Events::listener_keyboardDestroy, PNEWKEYBOARD, "Keyboard");

    wlr_seat_set_keyboard(g_pCompositor->m_sSeat.seat, keyboard->keyboard);

    Debug::log(LOG, "New keyboard created, pointers Hypr: %x and WLR: %x", PNEWKEYBOARD, keyboard);

    setKeyboardLayout();
}

void CInputManager::setKeyboardLayout() {

    const auto RULES    = g_pConfigManager->getString("input:kb_rules");
    const auto MODEL    = g_pConfigManager->getString("input:kb_model");
    const auto LAYOUT   = g_pConfigManager->getString("input:kb_layout");
    const auto VARIANT  = g_pConfigManager->getString("input:kb_variant");
    const auto OPTIONS  = g_pConfigManager->getString("input:kb_options");

    xkb_rule_names rules = {
        .rules = RULES.c_str(),
        .model = MODEL.c_str(),
        .layout = LAYOUT.c_str(),
        .variant = VARIANT.c_str(),
        .options = OPTIONS.c_str()
    };

    const auto CONTEXT = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    const auto KEYMAP = xkb_keymap_new_from_names(CONTEXT, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

    if (!KEYMAP) {
        Debug::log(ERR, "Keyboard layout %s with variant %s (rules: %s, model: %s, options: %s) couldn't have been loaded.", rules.layout, rules.variant, rules.rules, rules.model, rules.options);
        xkb_context_unref(CONTEXT);
        return;
    }

    // TODO: configure devices one by one
    for (auto& k : m_lKeyboards)
        wlr_keyboard_set_keymap(k.keyboard->keyboard, KEYMAP);

    xkb_keymap_unref(KEYMAP);
    xkb_context_unref(CONTEXT);

    Debug::log(LOG, "Set the keyboard layout to %s and variant to %s", rules.layout, rules.variant);
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
    pKeyboard->hyprListener_keyboardDestroy.removeCallback();
    pKeyboard->hyprListener_keyboardMod.removeCallback();
    pKeyboard->hyprListener_keyboardKey.removeCallback();

    m_lKeyboards.remove(*pKeyboard);
}

void CInputManager::destroyMouse(wlr_input_device* mouse) {
    //
}

void CInputManager::onKeyboardKey(wlr_keyboard_key_event* e, SKeyboard* pKeyboard) {
    const auto KEYCODE = e->keycode + 8; // Because to xkbcommon it's +8 from libinput

    const xkb_keysym_t* keysyms;
    int syms = xkb_state_key_get_syms(pKeyboard->keyboard->keyboard->xkb_state, KEYCODE, &keysyms);

    const auto MODS = wlr_keyboard_get_modifiers(pKeyboard->keyboard->keyboard);

    wlr_idle_notify_activity(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sSeat.seat);

    bool found = false;
    if (e->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        Debug::log(LOG, "Pressed key %i, with the MODMASK being %i", e->keycode, MODS);

        for (int i = 0; i < syms; ++i)
            found = g_pKeybindManager->handleKeybinds(MODS, keysyms[i]) || found;
    } else if (e->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        // hee hee
    }

    if (!found) {
        wlr_seat_set_keyboard(g_pCompositor->m_sSeat.seat, pKeyboard->keyboard->keyboard);
        wlr_seat_keyboard_notify_key(g_pCompositor->m_sSeat.seat, e->time_msec, e->keycode, e->state);
    }
}

void CInputManager::onKeyboardMod(void* data, SKeyboard* pKeyboard) {
    wlr_seat_set_keyboard(g_pCompositor->m_sSeat.seat, pKeyboard->keyboard->keyboard);
    wlr_seat_keyboard_notify_modifiers(g_pCompositor->m_sSeat.seat, &pKeyboard->keyboard->keyboard->modifiers);
}

void CInputManager::refocus() {
    mouseMoveUnified(0);
}