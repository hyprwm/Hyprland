#include "InputManager.hpp"
#include "../../Compositor.hpp"

void CInputManager::onMouseMoved(wlr_pointer_motion_event* e) {
    float sensitivity = g_pConfigManager->getFloat("general:sensitivity");

    const auto DELTA = g_pConfigManager->getInt("input:force_no_accel") == 1 ? Vector2D(e->unaccel_dx, e->unaccel_dy) : Vector2D(e->delta_x, e->delta_y);

    if (g_pConfigManager->getInt("general:apply_sens_to_raw") == 1)
        wlr_relative_pointer_manager_v1_send_relative_motion(g_pCompositor->m_sWLRRelPointerMgr, g_pCompositor->m_sSeat.seat, (uint64_t)e->time_msec * 1000, DELTA.x * sensitivity, DELTA.y * sensitivity, e->unaccel_dx * sensitivity, e->unaccel_dy * sensitivity);
    else
        wlr_relative_pointer_manager_v1_send_relative_motion(g_pCompositor->m_sWLRRelPointerMgr, g_pCompositor->m_sSeat.seat, (uint64_t)e->time_msec * 1000, DELTA.x, DELTA.y, e->unaccel_dx, e->unaccel_dy);

    wlr_cursor_move(g_pCompositor->m_sWLRCursor, &e->pointer->base, DELTA.x * sensitivity, DELTA.y * sensitivity);

    mouseMoveUnified(e->time_msec);
}

void CInputManager::onMouseWarp(wlr_pointer_motion_absolute_event* e) {
    wlr_cursor_warp_absolute(g_pCompositor->m_sWLRCursor, &e->pointer->base, e->x, e->y);

    mouseMoveUnified(e->time_msec);
}

void CInputManager::mouseMoveUnified(uint32_t time, bool refocus) {

    if (!g_pCompositor->m_bReadyToProcess)
        return;

    if (!g_pCompositor->m_sSeat.mouse) {
        Debug::log(ERR, "BUG THIS: Mouse move on mouse nullptr!");
        return;
    }

    Vector2D mouseCoords = getMouseCoordsInternal();
    const auto PMONITOR = g_pCompositor->getMonitorFromCursor();

    // constraints
    // All constraints TODO: multiple mice?
    if (g_pCompositor->m_sSeat.mouse->currentConstraint) {
        // XWayland windows sometimes issue constraints weirdly.
        // TODO: We probably should search their parent. wlr_xwayland_surface->parent
        const auto CONSTRAINTWINDOW = g_pCompositor->getConstraintWindow(g_pCompositor->m_sSeat.mouse);

        if (!CONSTRAINTWINDOW) {
            g_pCompositor->m_sSeat.mouse->currentConstraint = nullptr;
        } else {
            // Native Wayland apps know how 2 constrain themselves.
            // XWayland, we just have to accept them. Might cause issues, but thats XWayland for ya.
            const auto CONSTRAINTPOS = CONSTRAINTWINDOW->m_bIsX11 ? Vector2D(CONSTRAINTWINDOW->m_uSurface.xwayland->x, CONSTRAINTWINDOW->m_uSurface.xwayland->y) : CONSTRAINTWINDOW->m_vRealPosition.vec();
            const auto CONSTRAINTSIZE = CONSTRAINTWINDOW->m_bIsX11 ? Vector2D(CONSTRAINTWINDOW->m_uSurface.xwayland->width, CONSTRAINTWINDOW->m_uSurface.xwayland->height) : CONSTRAINTWINDOW->m_vRealSize.vec();

            if (!VECINRECT(mouseCoords, CONSTRAINTPOS.x, CONSTRAINTPOS.y, CONSTRAINTPOS.x + CONSTRAINTSIZE.x, CONSTRAINTPOS.y + CONSTRAINTSIZE.y)) {
                if (g_pCompositor->m_sSeat.mouse->constraintActive) {
                    Vector2D deltaToFit;

                    if (mouseCoords.x < CONSTRAINTPOS.x)
                        deltaToFit.x = CONSTRAINTPOS.x - mouseCoords.x;
                    else if (mouseCoords.x > CONSTRAINTPOS.x + CONSTRAINTSIZE.x)
                        deltaToFit.x = CONSTRAINTPOS.x + CONSTRAINTSIZE.x - mouseCoords.x;

                    if (mouseCoords.y < CONSTRAINTPOS.y)
                        deltaToFit.y = CONSTRAINTPOS.y - mouseCoords.y;
                    else if (mouseCoords.y > CONSTRAINTPOS.y + CONSTRAINTSIZE.y)
                        deltaToFit.y = CONSTRAINTPOS.y + CONSTRAINTSIZE.y - mouseCoords.y;

                    wlr_cursor_move(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, deltaToFit.x, deltaToFit.y);

                    mouseCoords = mouseCoords + deltaToFit;
                }
            } else {
                if ((!CONSTRAINTWINDOW->m_bIsX11 && PMONITOR && CONSTRAINTWINDOW->m_iWorkspaceID == PMONITOR->activeWorkspace) || (CONSTRAINTWINDOW->m_bIsX11)) {
                    g_pCompositor->m_sSeat.mouse->constraintActive = true;
                }
            }
        }
    }

    // update stuff
    updateDragIcon();

    g_pLayoutManager->getCurrentLayout()->onMouseMove(getMouseCoordsInternal());

    // focus
    wlr_surface* foundSurface = nullptr;

    if (PMONITOR && PMONITOR != g_pCompositor->m_pLastMonitor) {
        g_pCompositor->m_pLastMonitor = PMONITOR;

        // set active workspace and deactivate all other in wlr
        const auto ACTIVEWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);
        g_pCompositor->deactivateAllWLRWorkspaces(ACTIVEWORKSPACE->m_pWlrHandle);
        ACTIVEWORKSPACE->setActive(true);

        // event
        g_pEventManager->postEvent(SHyprIPCEvent("activemon", PMONITOR->szName + "," + ACTIVEWORKSPACE->m_szName));
    }

    Vector2D surfaceCoords;
    Vector2D surfacePos = Vector2D(-1337, -1337);
    CWindow* pFoundWindow = nullptr;

    // overlay is above fullscreen
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], &surfaceCoords);

    // then, we check if the workspace doesnt have a fullscreen window
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);
    if (PWORKSPACE->m_bHasFullscreenWindow && !foundSurface && PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL) {
        pFoundWindow = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);
        foundSurface = g_pXWaylandManager->getWindowSurface(pFoundWindow);
        surfacePos = pFoundWindow->m_vRealPosition.vec();

        // only check floating because tiled cant be over fullscreen
        for (auto w = g_pCompositor->m_lWindows.rbegin(); w != g_pCompositor->m_lWindows.rend(); w++) {
            wlr_box box = {w->m_vRealPosition.vec().x, w->m_vRealPosition.vec().y, w->m_vRealSize.vec().x, w->m_vRealSize.vec().y};
            if (((w->m_bIsFloating && w->m_bIsMapped && w->m_bCreatedOverFullscreen) || (w->m_iWorkspaceID == SPECIAL_WORKSPACE_ID && PMONITOR->specialWorkspaceOpen)) && wlr_box_contains_point(&box, mouseCoords.x, mouseCoords.y) && g_pCompositor->isWorkspaceVisible(w->m_iWorkspaceID) && !w->m_bHidden) {
                pFoundWindow = &(*w);
                
                if (!pFoundWindow->m_bIsX11) {
                    foundSurface = g_pCompositor->vectorWindowToSurface(mouseCoords, pFoundWindow, surfaceCoords);
                } else {
                    foundSurface = g_pXWaylandManager->getWindowSurface(pFoundWindow);
                    surfacePos = pFoundWindow->m_vRealPosition.vec();
                }

                break;
            }
        }
    }

    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_TOP], &surfaceCoords);

    // then windows
    if (!foundSurface) {
        pFoundWindow = g_pCompositor->vectorToWindowIdeal(mouseCoords);
        if (pFoundWindow) {
            if (!pFoundWindow->m_bIsX11) {
                foundSurface = g_pCompositor->vectorWindowToSurface(mouseCoords, pFoundWindow, surfaceCoords);
            } else {
                foundSurface = g_pXWaylandManager->getWindowSurface(pFoundWindow);
                surfacePos = pFoundWindow->m_vRealPosition.vec();
            }
        }
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

    Vector2D surfaceLocal = surfacePos == Vector2D(-1337, -1337) ? surfaceCoords : mouseCoords - surfacePos;

    if (pFoundWindow) {
        static auto *const PFOLLOWMOUSE = &g_pConfigManager->getConfigValuePtr("input:follow_mouse")->intValue;
        if (*PFOLLOWMOUSE != 1 && !refocus) {
            if (pFoundWindow != g_pCompositor->m_pLastWindow && g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow) && (g_pCompositor->m_pLastWindow->m_bIsFloating != pFoundWindow->m_bIsFloating)) {
                // enter if change floating style
                g_pCompositor->focusWindow(pFoundWindow, foundSurface);
                wlr_seat_pointer_notify_enter(g_pCompositor->m_sSeat.seat, foundSurface, surfaceLocal.x, surfaceLocal.y);
            }
            else if (*PFOLLOWMOUSE == 2) {
                wlr_seat_pointer_notify_enter(g_pCompositor->m_sSeat.seat, foundSurface, surfaceLocal.x, surfaceLocal.y);
            }
            wlr_seat_pointer_notify_motion(g_pCompositor->m_sSeat.seat, time, surfaceLocal.x, surfaceLocal.y);
            return; // don't enter any new surfaces
        } else {
            g_pCompositor->focusWindow(pFoundWindow, foundSurface);
        }
    }
    else
        g_pCompositor->focusSurface(foundSurface);

    wlr_seat_pointer_notify_enter(g_pCompositor->m_sSeat.seat, foundSurface, surfaceLocal.x, surfaceLocal.y);
    wlr_seat_pointer_notify_motion(g_pCompositor->m_sSeat.seat, time, surfaceLocal.x, surfaceLocal.y);
}

void CInputManager::onMouseButton(wlr_pointer_button_event* e) {
    wlr_idle_notify_activity(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sSeat.seat);

    const auto PKEYBOARD = wlr_seat_get_keyboard(g_pCompositor->m_sSeat.seat);

    switch (e->state) {
        case WLR_BUTTON_PRESSED:
            if (!g_pCompositor->m_sSeat.mouse->currentConstraint)
                refocus();

            // if clicked on a floating window make it top
            if (g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow) && g_pCompositor->m_pLastWindow->m_bIsFloating)
                g_pCompositor->moveWindowToTop(g_pCompositor->m_pLastWindow);

            if ((e->button == BTN_LEFT || e->button == BTN_RIGHT) && wlr_keyboard_get_modifiers(PKEYBOARD) == (uint32_t)g_pConfigManager->getInt("general:main_mod_internal")) {
                currentlyDraggedWindow = g_pCompositor->windowFromCursor();
                dragButton = e->button;

                g_pLayoutManager->getCurrentLayout()->onBeginDragWindow();

                return;
            }
            break;
        case WLR_BUTTON_RELEASED:
            if (currentlyDraggedWindow) {
                g_pLayoutManager->getCurrentLayout()->onEndDragWindow();
                currentlyDraggedWindow = nullptr;
                dragButton = -1;
            }
            
            break;
    }

    // notify app if we didnt handle it
    if (g_pCompositor->doesSeatAcceptInput(g_pCompositor->m_pLastFocus)) {
        wlr_seat_pointer_notify_button(g_pCompositor->m_sSeat.seat, e->time_msec, e->button, e->state);
    }
        
}

Vector2D CInputManager::getMouseCoordsInternal() {
    return Vector2D(g_pCompositor->m_sWLRCursor->x, g_pCompositor->m_sWLRCursor->y);
}

void CInputManager::newKeyboard(wlr_input_device* keyboard) {
    m_lKeyboards.push_back(SKeyboard());

    const auto PNEWKEYBOARD = &m_lKeyboards.back();

    PNEWKEYBOARD->keyboard = keyboard;

    const auto REPEATRATE = g_pConfigManager->getInt("input:repeat_rate");
    const auto REPEATDELAY = g_pConfigManager->getInt("input:repeat_delay");

    wlr_keyboard_set_repeat_info(keyboard->keyboard, std::max(0, REPEATRATE), std::max(0, REPEATDELAY));

    PNEWKEYBOARD->hyprListener_keyboardMod.initCallback(&keyboard->keyboard->events.modifiers, &Events::listener_keyboardMod, PNEWKEYBOARD, "Keyboard");
    PNEWKEYBOARD->hyprListener_keyboardKey.initCallback(&keyboard->keyboard->events.key, &Events::listener_keyboardKey, PNEWKEYBOARD, "Keyboard");
    PNEWKEYBOARD->hyprListener_keyboardDestroy.initCallback(&keyboard->events.destroy, &Events::listener_keyboardDestroy, PNEWKEYBOARD, "Keyboard");

    if (m_pActiveKeyboard)
        m_pActiveKeyboard->active = false;
    m_pActiveKeyboard = PNEWKEYBOARD;

    setKeyboardLayout();

    wlr_seat_set_keyboard(g_pCompositor->m_sSeat.seat, keyboard->keyboard);

    Debug::log(LOG, "New keyboard created, pointers Hypr: %x and WLR: %x", PNEWKEYBOARD, keyboard);
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

    const auto PLASTKEEB = m_pActiveKeyboard->keyboard->keyboard;

    if (!PLASTKEEB) {
        xkb_keymap_unref(KEYMAP);
        xkb_context_unref(CONTEXT);

        Debug::log(ERR, "No Seat Keyboard???");
        return;
    }

    wlr_keyboard_set_keymap(PLASTKEEB, KEYMAP);

    wlr_keyboard_modifiers wlrMods = {0};

    if (g_pConfigManager->getInt("input:numlock_by_default") == 1) {
        // lock numlock
        const auto IDX = xkb_map_mod_get_index(KEYMAP, XKB_MOD_NAME_NUM);

        if (IDX != XKB_MOD_INVALID)
            wlrMods.locked |= (uint32_t)1 << IDX;
    }

    if (wlrMods.locked != 0) {
        wlr_seat_keyboard_notify_modifiers(g_pCompositor->m_sSeat.seat, &wlrMods);
    }

    xkb_keymap_unref(KEYMAP);
    xkb_context_unref(CONTEXT);

    Debug::log(LOG, "Set the keyboard layout to %s and variant to %s", rules.layout, rules.variant);
}

void CInputManager::newMouse(wlr_input_device* mouse) {
    m_lMice.emplace_back();
    const auto PMOUSE = &m_lMice.back();

    PMOUSE->mouse = mouse;

    if (wlr_input_device_is_libinput(mouse)) {
        const auto LIBINPUTDEV = (libinput_device*)wlr_libinput_get_device_handle(mouse);

        if (libinput_device_config_tap_get_finger_count(LIBINPUTDEV))  // this is for tapping (like on a laptop)
            libinput_device_config_tap_set_enabled(LIBINPUTDEV, LIBINPUT_CONFIG_TAP_ENABLED);

        if (libinput_device_config_scroll_has_natural_scroll(LIBINPUTDEV)) {
            double w = 0, h = 0;

            if (libinput_device_has_capability(LIBINPUTDEV, LIBINPUT_DEVICE_CAP_POINTER) && libinput_device_get_size(LIBINPUTDEV, &w, &h) == 0) // pointer with size is a touchpad
                libinput_device_config_scroll_set_natural_scroll_enabled(LIBINPUTDEV, g_pConfigManager->getInt("input:touchpad:natural_scroll"));
            else
                libinput_device_config_scroll_set_natural_scroll_enabled(LIBINPUTDEV, g_pConfigManager->getInt("input:natural_scroll"));
        }
        
        if (libinput_device_config_dwt_is_available(LIBINPUTDEV)) {
            const auto DWT = static_cast<enum libinput_config_dwt_state>(g_pConfigManager->getInt("input:touchpad:disable_while_typing") != 0);
            libinput_device_config_dwt_set_enabled(LIBINPUTDEV, DWT);
        }
    }

    PMOUSE->hyprListener_destroyMouse.initCallback(&mouse->events.destroy, &Events::listener_destroyMouse, PMOUSE, "Mouse");

    wlr_cursor_attach_input_device(g_pCompositor->m_sWLRCursor, mouse);

    g_pCompositor->m_sSeat.mouse = PMOUSE;

    Debug::log(LOG, "New mouse created, pointer WLR: %x", mouse);
}

void CInputManager::destroyKeyboard(SKeyboard* pKeyboard) {
    pKeyboard->hyprListener_keyboardDestroy.removeCallback();
    pKeyboard->hyprListener_keyboardMod.removeCallback();
    pKeyboard->hyprListener_keyboardKey.removeCallback();

    if (pKeyboard->active) {
        m_lKeyboards.remove(*pKeyboard);

        if (m_lKeyboards.size() > 0) {
            m_pActiveKeyboard = &m_lKeyboards.back();
            m_pActiveKeyboard->active = true;
        } else {
            m_pActiveKeyboard = nullptr;
        }
    }

    m_lKeyboards.remove(*pKeyboard);
}

void CInputManager::destroyMouse(wlr_input_device* mouse) {
    for (auto& m : m_lMice) {
        if (m.mouse == mouse) {
            m_lMice.remove(m);
            break;
        }
    }

    g_pCompositor->m_sSeat.mouse = m_lMice.size() > 0 ? &m_lMice.front() : nullptr;

    if (g_pCompositor->m_sSeat.mouse)
        g_pCompositor->m_sSeat.mouse->currentConstraint = nullptr;
}

void CInputManager::onKeyboardKey(wlr_keyboard_key_event* e, SKeyboard* pKeyboard) {
    const auto KEYCODE = e->keycode + 8; // Because to xkbcommon it's +8 from libinput

    const xkb_keysym_t* keysyms;
    int syms = xkb_state_key_get_syms(pKeyboard->keyboard->keyboard->xkb_state, KEYCODE, &keysyms);

    const auto MODS = wlr_keyboard_get_modifiers(pKeyboard->keyboard->keyboard);

    wlr_idle_notify_activity(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sSeat.seat);

    bool found = false;
    if (e->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
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
    mouseMoveUnified(0, true);
}

void CInputManager::updateDragIcon() {
    if (!g_pInputManager->m_sDrag.dragIcon)
        return;

    switch (g_pInputManager->m_sDrag.dragIcon->drag->grab_type) {
        case WLR_DRAG_GRAB_KEYBOARD:
            break;
        case WLR_DRAG_GRAB_KEYBOARD_POINTER:
            g_pInputManager->m_sDrag.pos = g_pInputManager->getMouseCoordsInternal();
            break;
        default:
            break;
    }
}

void CInputManager::recheckConstraint(SMouse* pMouse) {
    if (!pMouse->currentConstraint)
        return;

    const auto PREGION = &pMouse->currentConstraint->region;

    if (pMouse->currentConstraint->type == WLR_POINTER_CONSTRAINT_V1_CONFINED) {
        pixman_region32_copy(&pMouse->confinedTo, PREGION);
    } else {
        pixman_region32_clear(&pMouse->confinedTo);
    }

    const auto PWINDOW = g_pCompositor->getConstraintWindow(g_pCompositor->m_sSeat.mouse);
    const auto PWINDOWNAME = PWINDOW ? PWINDOW->m_szTitle : "";

    Debug::log(LOG, "Constraint rechecked: %i, %i to %i, %i for %x (window name: %s)", PREGION->extents.x1, PREGION->extents.y1, PREGION->extents.x2, PREGION->extents.y2, pMouse->currentConstraint->surface, PWINDOWNAME.c_str());
}

void CInputManager::constrainMouse(SMouse* pMouse, wlr_pointer_constraint_v1* constraint) {

    if (pMouse->currentConstraint == constraint)
        return;

    const auto PWINDOW = g_pCompositor->getWindowFromSurface(constraint->surface);
    const auto MOUSECOORDS = getMouseCoordsInternal();

    pMouse->hyprListener_commitConstraint.removeCallback();

    if (pMouse->currentConstraint) {
        if (!constraint) {
            // warpe to hint

            if (constraint->current.committed & WLR_POINTER_CONSTRAINT_V1_STATE_CURSOR_HINT) {
                if (PWINDOW) {
                    if (PWINDOW->m_bIsX11) {
                        wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr,
                                        constraint->current.cursor_hint.x + PWINDOW->m_uSurface.xwayland->x, PWINDOW->m_uSurface.xwayland->y + PWINDOW->m_vRealPosition.vec().y);

                        wlr_seat_pointer_warp(constraint->seat, constraint->current.cursor_hint.x, constraint->current.cursor_hint.y);
                    } else {
                        wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr,
                                        constraint->current.cursor_hint.x + PWINDOW->m_vRealPosition.vec().x, constraint->current.cursor_hint.y + PWINDOW->m_vRealPosition.vec().y);

                        wlr_seat_pointer_warp(constraint->seat, constraint->current.cursor_hint.x, constraint->current.cursor_hint.y);
                    }
                }
            }
        }
        
        wlr_pointer_constraint_v1_send_deactivated(pMouse->currentConstraint);
    }

    pMouse->currentConstraint = constraint;
    pMouse->constraintActive = true;

    if (pixman_region32_not_empty(&constraint->current.region)) {
        pixman_region32_intersect(&constraint->region, &constraint->surface->input_region, &constraint->current.region);
    } else {
        pixman_region32_copy(&constraint->region, &constraint->surface->input_region);
    }

    // warp to the constraint
    recheckConstraint(pMouse);

    wlr_pointer_constraint_v1_send_activated(pMouse->currentConstraint);

    pMouse->hyprListener_commitConstraint.initCallback(&pMouse->currentConstraint->surface->events.commit, &Events::listener_commitConstraint, pMouse, "Mouse constraint commit");

    Debug::log(LOG, "Constrained mouse to %x", pMouse->currentConstraint);
}

void Events::listener_commitConstraint(void* owner, void* data) {
    //g_pInputManager->recheckConstraint((SMouse*)owner);
}

void CInputManager::updateCapabilities(wlr_input_device* pDev) {
    // TODO: this is dumb

    switch (pDev->type) {
        case WLR_INPUT_DEVICE_KEYBOARD:
            m_uiCapabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
            break;
        case WLR_INPUT_DEVICE_POINTER:
            m_uiCapabilities |= WL_SEAT_CAPABILITY_POINTER;
            break;
        case WLR_INPUT_DEVICE_TOUCH:
            m_uiCapabilities |= WL_SEAT_CAPABILITY_TOUCH;
            break;
        default:
            break;
    }

    wlr_seat_set_capabilities(g_pCompositor->m_sSeat.seat, m_uiCapabilities);
}
