#include "KeybindManager.hpp"

#include <regex>

CKeybindManager::CKeybindManager() {
    // initialize all dispatchers

    m_mDispatchers["exec"]                      = spawn;
    m_mDispatchers["killactive"]                = killActive;
    m_mDispatchers["closewindow"]               = kill;
    m_mDispatchers["togglefloating"]            = toggleActiveFloating;
    m_mDispatchers["workspace"]                 = changeworkspace;
    m_mDispatchers["fullscreen"]                = fullscreenActive;
    m_mDispatchers["movetoworkspace"]           = moveActiveToWorkspace;
    m_mDispatchers["movetoworkspacesilent"]     = moveActiveToWorkspaceSilent;
    m_mDispatchers["pseudo"]                    = toggleActivePseudo;
    m_mDispatchers["movefocus"]                 = moveFocusTo;
    m_mDispatchers["movewindow"]                = moveActiveTo;
    m_mDispatchers["togglegroup"]               = toggleGroup;
    m_mDispatchers["changegroupactive"]         = changeGroupActive;
    m_mDispatchers["togglesplit"]               = toggleSplit;
    m_mDispatchers["splitratio"]                = alterSplitRatio;
    m_mDispatchers["focusmonitor"]              = focusMonitor;
    m_mDispatchers["movecursortocorner"]        = moveCursorToCorner;
    m_mDispatchers["workspaceopt"]              = workspaceOpt;
    m_mDispatchers["exit"]                      = exitHyprland;
    m_mDispatchers["movecurrentworkspacetomonitor"] = moveCurrentWorkspaceToMonitor;
    m_mDispatchers["moveworkspacetomonitor"]    = moveWorkspaceToMonitor;
    m_mDispatchers["togglespecialworkspace"]    = toggleSpecialWorkspace;
    m_mDispatchers["forcerendererreload"]       = forceRendererReload;
    m_mDispatchers["resizeactive"]              = resizeActive;
    m_mDispatchers["moveactive"]                = moveActive;
    m_mDispatchers["cyclenext"]                 = circleNext;
    m_mDispatchers["focuswindowbyclass"]        = focusWindow;
    m_mDispatchers["focuswindow"]               = focusWindow;
    m_mDispatchers["submap"]                    = setSubmap;
    m_mDispatchers["pass"]                      = pass;
    m_mDispatchers["layoutmsg"]                 = layoutmsg;
    m_mDispatchers["toggleopaque"]              = toggleOpaque;
    m_mDispatchers["dpms"]                      = dpms;
    m_mDispatchers["movewindowpixel"]           = moveWindow;
    m_mDispatchers["resizewindowpixel"]         = resizeWindow;
    m_mDispatchers["swapnext"]                  = swapnext;
    m_mDispatchers["swapactiveworkspaces"]      = swapActiveWorkspaces;

    m_tScrollTimer.reset();
}

void CKeybindManager::addKeybind(SKeybind kb) {
    m_lKeybinds.push_back(kb);

    m_pActiveKeybind = nullptr;
}

void CKeybindManager::removeKeybind(uint32_t mod, const std::string& key) {
    for (auto it = m_lKeybinds.begin(); it != m_lKeybinds.end(); ++it) {
        if (isNumber(key) && std::stoi(key) > 9) {
            const auto KEYNUM = std::stoi(key);

            if (it->modmask == mod && it->keycode == KEYNUM) {
                it = m_lKeybinds.erase(it);

                if (it == m_lKeybinds.end())
                    break;
            }
        }
        else if (it->modmask == mod && it->key == key) {
            it = m_lKeybinds.erase(it);

            if (it == m_lKeybinds.end())
                break;
        }
    }

    m_pActiveKeybind = nullptr;
}

uint32_t CKeybindManager::stringToModMask(std::string mods) {
    uint32_t modMask = 0;
    if (mods.contains("SHIFT"))
        modMask |= WLR_MODIFIER_SHIFT;
    if (mods.contains("CAPS"))
        modMask |= WLR_MODIFIER_CAPS;
    if (mods.contains("CTRL") || mods.contains("CONTROL"))
        modMask |= WLR_MODIFIER_CTRL;
    if (mods.contains("ALT"))
        modMask |= WLR_MODIFIER_ALT;
    if (mods.contains("MOD2"))
        modMask |= WLR_MODIFIER_MOD2;
    if (mods.contains("MOD3"))
        modMask |= WLR_MODIFIER_MOD3;
    if (mods.contains("SUPER") || mods.contains("WIN") || mods.contains("LOGO") || mods.contains("MOD4"))
        modMask |= WLR_MODIFIER_LOGO;
    if (mods.contains("MOD5"))
        modMask |= WLR_MODIFIER_MOD5;

    return modMask;
}

void CKeybindManager::updateXKBTranslationState() {
    if (m_pXKBTranslationState) {
        xkb_keymap_unref(xkb_state_get_keymap(m_pXKBTranslationState));
        xkb_state_unref(m_pXKBTranslationState);

        m_pXKBTranslationState = nullptr;
    }

    const auto FILEPATH = g_pConfigManager->getString("input:kb_file");
    const auto RULES = g_pConfigManager->getString("input:kb_rules");
    const auto MODEL = g_pConfigManager->getString("input:kb_model");
    const auto LAYOUT = g_pConfigManager->getString("input:kb_layout");
    const auto VARIANT = g_pConfigManager->getString("input:kb_variant");
    const auto OPTIONS = g_pConfigManager->getString("input:kb_options");

    xkb_rule_names rules = {
        .rules = RULES.c_str(),
        .model = MODEL.c_str(),
        .layout = LAYOUT.c_str(),
        .variant = VARIANT.c_str(),
        .options = OPTIONS.c_str()};

    const auto PCONTEXT = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    const auto PKEYMAP = FILEPATH == "" ? xkb_keymap_new_from_names(PCONTEXT, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS) : xkb_keymap_new_from_file(PCONTEXT, fopen(FILEPATH.c_str(), "r"), XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);

    xkb_context_unref(PCONTEXT);
    m_pXKBTranslationState = xkb_state_new(PKEYMAP);
}

bool CKeybindManager::onKeyEvent(wlr_keyboard_key_event* e, SKeyboard* pKeyboard) {
    if (!g_pCompositor->m_bSessionActive) {
        m_dPressedKeycodes.clear();
        m_dPressedKeysyms.clear();
        return true;
    }

    if (pKeyboard->isVirtual)
        return true;

    if (!m_pXKBTranslationState) {
        Debug::log(ERR, "BUG THIS: m_pXKBTranslationState NULL!");
        updateXKBTranslationState();

        if (!m_pXKBTranslationState)
            return true;
    }

    const auto KEYCODE = e->keycode + 8;  // Because to xkbcommon it's +8 from libinput

    const xkb_keysym_t keysym = xkb_state_key_get_one_sym(m_pXKBTranslationState, KEYCODE);
    const xkb_keysym_t internalKeysym = xkb_state_key_get_one_sym(wlr_keyboard_from_input_device(pKeyboard->keyboard)->xkb_state, KEYCODE);

    if (handleInternalKeybinds(internalKeysym))
        return true;

    const auto MODS = g_pInputManager->accumulateModsFromAllKBs();

    m_uTimeLastMs = e->time_msec;
    m_uLastCode = KEYCODE;
    m_uLastMouseCode = 0;

    bool found = false;
    if (e->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        // clean repeat
        if (m_pActiveKeybindEventSource) {
            wl_event_source_remove(m_pActiveKeybindEventSource);
            m_pActiveKeybindEventSource = nullptr;
            m_pActiveKeybind = nullptr;
        }

        m_dPressedKeycodes.push_back(KEYCODE);
        m_dPressedKeysyms.push_back(keysym);

        found = g_pKeybindManager->handleKeybinds(MODS, "", keysym, 0, true, e->time_msec) || found;

        found = g_pKeybindManager->handleKeybinds(MODS, "", 0, KEYCODE, true, e->time_msec) || found;

        if (found)
            shadowKeybinds(keysym, KEYCODE);
    } else if (e->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        // clean repeat
        if (m_pActiveKeybindEventSource) {
            wl_event_source_remove(m_pActiveKeybindEventSource);
            m_pActiveKeybindEventSource = nullptr;
            m_pActiveKeybind = nullptr;
        }

        m_dPressedKeycodes.erase(std::remove(m_dPressedKeycodes.begin(), m_dPressedKeycodes.end(), KEYCODE));
        m_dPressedKeysyms.erase(std::remove(m_dPressedKeysyms.begin(), m_dPressedKeysyms.end(), keysym));

        found = g_pKeybindManager->handleKeybinds(MODS, "", keysym, 0, false, e->time_msec) || found;

        found = g_pKeybindManager->handleKeybinds(MODS, "", 0, KEYCODE, false, e->time_msec) || found;

        shadowKeybinds();
    }

    return !found;
}

bool CKeybindManager::onAxisEvent(wlr_pointer_axis_event* e) {
    const auto MODS = g_pInputManager->accumulateModsFromAllKBs();

    static auto *const PDELAY = &g_pConfigManager->getConfigValuePtr("binds:scroll_event_delay")->intValue;

    if (m_tScrollTimer.getMillis() < *PDELAY) {
        m_tScrollTimer.reset();
        return true; // timer hasn't passed yet!
    }

    m_tScrollTimer.reset();

    bool found = false;
    if (e->source == WLR_AXIS_SOURCE_WHEEL && e->orientation == WLR_AXIS_ORIENTATION_VERTICAL) {
        if (e->delta < 0) { 
            found = g_pKeybindManager->handleKeybinds(MODS, "mouse_down", 0, 0, true, 0);
        } else {
            found = g_pKeybindManager->handleKeybinds(MODS, "mouse_up", 0, 0, true, 0);
        }

        if (found)
            shadowKeybinds();
    }

    return !found;
}

bool CKeybindManager::onMouseEvent(wlr_pointer_button_event* e) {
    const auto MODS = g_pInputManager->accumulateModsFromAllKBs();

    bool found = false;

    m_uLastMouseCode = e->button;
    m_uLastCode = 0;
    m_uTimeLastMs = e->time_msec;

    if (e->state == WLR_BUTTON_PRESSED) {
        found = g_pKeybindManager->handleKeybinds(MODS, "mouse:" + std::to_string(e->button), 0, 0, true, 0);

        if (found)
            shadowKeybinds();
    } else {
        found = g_pKeybindManager->handleKeybinds(MODS, "mouse:" + std::to_string(e->button), 0, 0, false, 0);

        shadowKeybinds();
    }

    return !found;
}

int repeatKeyHandler(void* data) {
    SKeybind** ppActiveKeybind = (SKeybind**)data;

    if (!*ppActiveKeybind)
        return 0;

    const auto DISPATCHER = g_pKeybindManager->m_mDispatchers.find((*ppActiveKeybind)->handler);

    Debug::log(LOG, "Keybind repeat triggered, calling dispatcher.");
    DISPATCHER->second((*ppActiveKeybind)->arg);

    wl_event_source_timer_update(g_pKeybindManager->m_pActiveKeybindEventSource, 1000 / g_pInputManager->m_pActiveKeyboard->repeatRate);

    return 0;
}

bool CKeybindManager::handleKeybinds(const uint32_t& modmask, const std::string& key, const xkb_keysym_t& keysym, const int& keycode, bool pressed, uint32_t time) {
    bool found = false;

    if (g_pCompositor->m_sSeat.exclusiveClient)
        Debug::log(LOG, "Keybind handling only locked (inhibitor)");

    if (pressed && m_kHeldBack) {
        // release the held back event
        wlr_seat_keyboard_notify_key(g_pCompositor->m_sSeat.seat, time, m_kHeldBack, WL_KEYBOARD_KEY_STATE_PRESSED);
        m_kHeldBack = 0;
    }

    for (auto& k : m_lKeybinds) {
        if (modmask != k.modmask || (g_pCompositor->m_sSeat.exclusiveClient && !k.locked) || k.submap != m_szCurrentSelectedSubmap || (!pressed && !k.release && k.handler != "pass") || k.shadowed)
            continue;

        if (!key.empty()) {
            if (key != k.key)
                continue;
        } else if (k.keycode != -1) {
            if (keycode != k.keycode)
                continue;
        } else {
            if (keysym == 0)
                continue;  // this is a keycode check run

            // oMg such performance hit!!11!
            // this little maneouver is gonna cost us 4µs
            const auto KBKEY = xkb_keysym_from_name(k.key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
            const auto KBKEYUPPER = xkb_keysym_to_upper(KBKEY);
            // small TODO: fix 0-9 keys and other modified ones with shift

            if (keysym != KBKEY && keysym != KBKEYUPPER)
                continue;
        }

        if (pressed && k.release) {
            // suppress down event
            m_kHeldBack = keysym;
            return true;
        }

        const auto DISPATCHER = m_mDispatchers.find(k.handler);

        // Should never happen, as we check in the ConfigManager, but oh well
        if (DISPATCHER == m_mDispatchers.end()) {
            Debug::log(ERR, "Inavlid handler in a keybind! (handler %s does not exist)", k.handler.c_str());
        } else {
            // call the dispatcher
            Debug::log(LOG, "Keybind triggered, calling dispatcher (%d, %s, %d)", modmask, key.c_str(), keysym);

            m_iPassPressed = (int)pressed;

            DISPATCHER->second(k.arg);

            m_iPassPressed = -1;

            if (k.handler == "submap") {
                found = true; // don't process keybinds on submap change.
                break;
            }
        }

        if (k.repeat) {
            m_pActiveKeybind = &k;
            m_pActiveKeybindEventSource = wl_event_loop_add_timer(g_pCompositor->m_sWLEventLoop, repeatKeyHandler, &m_pActiveKeybind);

            const auto PACTIVEKEEB = g_pInputManager->m_pActiveKeyboard;

            wl_event_source_timer_update(m_pActiveKeybindEventSource, PACTIVEKEEB->repeatDelay);
        }

        found = true;
    }

    return found;
}

void CKeybindManager::shadowKeybinds(const xkb_keysym_t& doesntHave, const int& doesntHaveCode) {
    // shadow disables keybinds after one has been triggered

    for (auto& k : m_lKeybinds) {

        bool shadow = false;

        const auto KBKEY = xkb_keysym_from_name(k.key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
        const auto KBKEYUPPER = xkb_keysym_to_upper(KBKEY);

        for (auto& pk : m_dPressedKeysyms) {
            if ((pk == KBKEY || pk == KBKEYUPPER)) {
                shadow = true;
                
                if (pk == doesntHave && doesntHave != 0) {
                    shadow = false;
                    break;
                }
            }
        }

        for (auto& pk : m_dPressedKeycodes) {
            if (pk == k.keycode) {
                shadow = true;

                if (pk == doesntHaveCode && doesntHaveCode != 0 && doesntHaveCode != -1) {
                    shadow = false;
                    break;
                }
            }
        }

        k.shadowed = shadow;
    }
}

bool CKeybindManager::handleVT(xkb_keysym_t keysym) {
    // Handles the CTRL+ALT+FX TTY keybinds
    if (!(keysym >= XKB_KEY_XF86Switch_VT_1 && keysym <= XKB_KEY_XF86Switch_VT_12))
        return false;

    const auto PSESSION = wlr_backend_get_session(g_pCompositor->m_sWLRBackend);
    if (PSESSION) {
        const int TTY = keysym - XKB_KEY_XF86Switch_VT_1 + 1;
        wlr_session_change_vt(PSESSION, TTY);
        g_pCompositor->m_bSessionActive = false;

        for (auto& m : g_pCompositor->m_vMonitors) {
            m->noFrameSchedule = true;
            m->framesToSkip = 1;
        }

        Debug::log(LOG, "Switched to VT %i, destroyed all render data, frames to skip for each: 2", TTY);

        return true;
    }

    return false;
}

bool CKeybindManager::handleInternalKeybinds(xkb_keysym_t keysym) {
    if (handleVT(keysym))
        return true;

    // handle ESC while in kill mode
    if (g_pInputManager->getClickMode() == CLICKMODE_KILL) {
        const auto KBKEY = xkb_keysym_from_name("ESCAPE", XKB_KEYSYM_CASE_INSENSITIVE);

        if (keysym == KBKEY) {
            g_pInputManager->setClickMode(CLICKMODE_DEFAULT);
            return true;
        }
    }

    return false;
}

// Dispatchers

void CKeybindManager::spawn(std::string args) {
    if (g_pXWaylandManager->m_sWLRXWayland)
        args = "WAYLAND_DISPLAY=" + std::string(g_pCompositor->m_szWLDisplaySocket) + " DISPLAY=" + std::string(g_pXWaylandManager->m_sWLRXWayland->display_name) + " " + args;
    else
        args = "WAYLAND_DISPLAY=" + std::string(g_pCompositor->m_szWLDisplaySocket) + " " + args;

    Debug::log(LOG, "Executing %s", args.c_str());

    int socket[2];
    if (pipe(socket) != 0) {
        Debug::log(LOG, "Unable to create pipe for fork");
    }

    pid_t child, grandchild;
    child = fork();
    if (child < 0) {
        close(socket[0]);
        close(socket[1]);
        Debug::log(LOG, "Fail to create the first fork");
        return;
    }
    if (child == 0) {
        // run in child
        grandchild = fork();
        if (grandchild == 0) {
            // run in grandchild
            close(socket[0]);
            close(socket[1]);
            execl("/bin/sh", "/bin/sh", "-c", args.c_str(), nullptr);
            // exit grandchild
            _exit(0);
        }
        close(socket[0]);
        write(socket[1], &grandchild, sizeof(grandchild));
        close(socket[1]);
        // exit child
        _exit(0);
    }
    // run in parent
    close(socket[1]);
    read(socket[0], &grandchild, sizeof(grandchild));
    close(socket[0]);
    // clear child and leave child to init
    waitpid(child, NULL, 0);
    if (child < 0) {
        Debug::log(LOG, "Fail to create the second fork");
        return;
    }
    Debug::log(LOG, "Process Created with pid %d", grandchild);
}

void CKeybindManager::killActive(std::string args) {
    g_pCompositor->closeWindow(g_pCompositor->m_pLastWindow);
}

void CKeybindManager::kill(std::string args) {
    const auto PWINDOW = g_pCompositor->getWindowByRegex(args);

    if (!PWINDOW) {
        Debug::log(ERR, "kill: no window found");
        return;
    }

    g_pCompositor->closeWindow(PWINDOW);
}

void CKeybindManager::clearKeybinds() {
    m_lKeybinds.clear();
}

void CKeybindManager::toggleActiveFloating(std::string args) {
    CWindow* PWINDOW = nullptr;

    if (args != "" && args != "active" && args.length() > 1) {
        PWINDOW = g_pCompositor->getWindowByRegex(args);
    } else {
        PWINDOW = g_pCompositor->m_pLastWindow;
    }

    if (!PWINDOW)
        return;

    if (g_pCompositor->windowValidMapped(PWINDOW)) {
        // remove drag status
        g_pInputManager->currentlyDraggedWindow = nullptr;

        if (PWINDOW->m_iWorkspaceID == SPECIAL_WORKSPACE_ID)
            return;

        PWINDOW->m_bIsFloating = !PWINDOW->m_bIsFloating;

        g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(PWINDOW);
    }
}

void CKeybindManager::toggleActivePseudo(std::string args) {
    const auto ACTIVEWINDOW = g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(ACTIVEWINDOW))
        return;

    ACTIVEWINDOW->m_bIsPseudotiled = !ACTIVEWINDOW->m_bIsPseudotiled;

    g_pLayoutManager->getCurrentLayout()->recalculateWindow(ACTIVEWINDOW);
}

void CKeybindManager::changeworkspace(std::string args) {
    int workspaceToChangeTo = 0;
    std::string workspaceName = "";

    // Flag needed so that the previous workspace is not recorded when switching
    // to a previous workspace.
    bool isSwitchingToPrevious = false;

    bool internal = false;

    if (args.find("[internal]") == 0) {
        workspaceToChangeTo = std::stoi(args.substr(10));
        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(workspaceToChangeTo);
        if (PWORKSPACE)
            workspaceName = PWORKSPACE->m_szName;

        internal = true;
    } else if (args.find("previous") == 0) {
        const auto PCURRENTWORKSPACE = g_pCompositor->getWorkspaceByID(
            g_pCompositor->m_pLastMonitor->activeWorkspace);

        // Do nothing if there's no previous workspace, otherwise switch to it.
        if (PCURRENTWORKSPACE->m_iPrevWorkspaceID == -1) {
            Debug::log(LOG, "No previous workspace to change to");
            return;
        }
        else {
            workspaceToChangeTo = PCURRENTWORKSPACE->m_iPrevWorkspaceID;
            isSwitchingToPrevious = true;

            // If the previous workspace ID isn't reset, cycles can form when continually going
            // to the previous workspace again and again.
            static auto *const PALLOWWORKSPACECYCLES = &g_pConfigManager->getConfigValuePtr("binds:allow_workspace_cycles")->intValue;
            if (!*PALLOWWORKSPACECYCLES)
                PCURRENTWORKSPACE->m_iPrevWorkspaceID = -1;
        }
    } else {
        workspaceToChangeTo = getWorkspaceIDFromString(args, workspaceName);
    }

    if (workspaceToChangeTo == INT_MAX) {
        Debug::log(ERR, "Error in changeworkspace, invalid value");
        return;
    }

    // Workspace_back_and_forth being enabled means that an attempt to switch to 
    // the current workspace will instead switch to the previous.
    const auto PCURRENTWORKSPACE = g_pCompositor->getWorkspaceByID(
        g_pCompositor->m_pLastMonitor->activeWorkspace);
    static auto *const PBACKANDFORTH = &g_pConfigManager->getConfigValuePtr("binds:workspace_back_and_forth")->intValue;
    if (*PBACKANDFORTH 
        && PCURRENTWORKSPACE->m_iID == workspaceToChangeTo
        && PCURRENTWORKSPACE->m_iPrevWorkspaceID != -1
        && !internal) {

        workspaceToChangeTo = PCURRENTWORKSPACE->m_iPrevWorkspaceID;
        isSwitchingToPrevious = true;

        // If the previous workspace ID isn't reset, cycles can form when continually going
        // to the previous workspace again and again.
        static auto *const PALLOWWORKSPACECYCLES = &g_pConfigManager->getConfigValuePtr("binds:allow_workspace_cycles")->intValue;
        if (!*PALLOWWORKSPACECYCLES)
            PCURRENTWORKSPACE->m_iPrevWorkspaceID = -1;
    }

    // remove constraints 
    g_pInputManager->unconstrainMouse();

    // if it's not internal, we will unfocus to prevent stuck focus
    if (!internal)
        g_pCompositor->focusWindow(nullptr);

    // if it exists, we warp to it
    if (g_pCompositor->getWorkspaceByID(workspaceToChangeTo)) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(g_pCompositor->getWorkspaceByID(workspaceToChangeTo)->m_iMonitorID);

        const auto PWORKSPACETOCHANGETO = g_pCompositor->getWorkspaceByID(workspaceToChangeTo);

        if (!isSwitchingToPrevious && !internal)
            // Remember previous workspace.
            PWORKSPACETOCHANGETO->m_iPrevWorkspaceID = g_pCompositor->m_pLastMonitor->activeWorkspace;

        if (workspaceToChangeTo == SPECIAL_WORKSPACE_ID)
            PWORKSPACETOCHANGETO->m_iMonitorID = PMONITOR->ID;

        // if it's not visible, make it visible.
        if (!g_pCompositor->isWorkspaceVisible(workspaceToChangeTo)) {
            const auto OLDWORKSPACEID = PMONITOR->activeWorkspace;

            // change it
            if (workspaceToChangeTo != SPECIAL_WORKSPACE_ID)
                PMONITOR->activeWorkspace = workspaceToChangeTo;
            else
                PMONITOR->specialWorkspaceOpen = true;

            // here and only here begin anim. we don't want to anim visible workspaces on other monitors.
            // check if anim left or right
            const auto ANIMTOLEFT = workspaceToChangeTo > OLDWORKSPACEID;

            // start anim on old workspace
            g_pCompositor->getWorkspaceByID(OLDWORKSPACEID)->startAnim(false, ANIMTOLEFT);

            // start anim on new workspace
            PWORKSPACETOCHANGETO->startAnim(true, ANIMTOLEFT);

            g_pEventManager->postEvent(SHyprIPCEvent{"workspace", PWORKSPACETOCHANGETO->m_szName});
        }

        // If the monitor is not the one our cursor's at, warp to it.
        const bool anotherMonitor = PMONITOR != g_pCompositor->getMonitorFromCursor();
        if (anotherMonitor) {
            Vector2D middle = PMONITOR->vecPosition + PMONITOR->vecSize / 2.f;
            g_pCompositor->warpCursorTo(middle);
        }

        // set active and deactivate all other in wlr
        g_pCompositor->deactivateAllWLRWorkspaces(PWORKSPACETOCHANGETO->m_pWlrHandle);
        PWORKSPACETOCHANGETO->setActive(true);

        // recalc layout
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PWORKSPACETOCHANGETO->m_iMonitorID);

        Debug::log(LOG, "Changed to workspace %i", workspaceToChangeTo);

        // focus
        if (const auto PWINDOW = PWORKSPACETOCHANGETO->getLastFocusedWindow(); PWINDOW) {
            // warp and focus
            if (anotherMonitor)
                g_pCompositor->warpCursorTo(PWINDOW->m_vRealPosition.vec() + PWINDOW->m_vRealSize.vec() / 2.f);
            g_pCompositor->focusWindow(PWINDOW, g_pXWaylandManager->getWindowSurface(PWINDOW));
        } else
            g_pInputManager->refocus();

        // mark the monitor dirty
        g_pHyprRenderer->damageMonitor(PMONITOR);

        return;
    }

    // Workspace doesn't exist, create and switch
    const auto PMONITOR = g_pCompositor->getMonitorFromCursor();

    const auto OLDWORKSPACE = PMONITOR->activeWorkspace;

    // get anim direction
    const auto ANIMTOLEFT = workspaceToChangeTo > OLDWORKSPACE;

    // start anim on old workspace
    if (const auto POLDWORKSPACE = g_pCompositor->getWorkspaceByID(OLDWORKSPACE); POLDWORKSPACE)
        POLDWORKSPACE->startAnim(false, ANIMTOLEFT);

    const auto PWORKSPACE = g_pCompositor->m_vWorkspaces.emplace_back(std::make_unique<CWorkspace>(PMONITOR->ID, workspaceName, workspaceToChangeTo == SPECIAL_WORKSPACE_ID)).get();

    if (!isSwitchingToPrevious)
        // Remember previous workspace.
        PWORKSPACE->m_iPrevWorkspaceID = OLDWORKSPACE;

    // start anim on new workspace
    PWORKSPACE->startAnim(true, ANIMTOLEFT);

    // We are required to set the name here immediately
    if (workspaceToChangeTo != SPECIAL_WORKSPACE_ID)
        wlr_ext_workspace_handle_v1_set_name(PWORKSPACE->m_pWlrHandle, workspaceName.c_str());

    PWORKSPACE->m_iID = workspaceToChangeTo;
    PWORKSPACE->m_iMonitorID = PMONITOR->ID;

    PMONITOR->specialWorkspaceOpen = false;

    if (workspaceToChangeTo != SPECIAL_WORKSPACE_ID)
        PMONITOR->activeWorkspace = workspaceToChangeTo;
    else
        PMONITOR->specialWorkspaceOpen = true;

    // set active and deactivate all other
    g_pCompositor->deactivateAllWLRWorkspaces(PWORKSPACE->m_pWlrHandle);
    PWORKSPACE->setActive(true);

    // mark the monitor dirty
    g_pHyprRenderer->damageMonitor(PMONITOR);

    // focus (clears the last)
    g_pInputManager->refocus();

    // Event
    g_pEventManager->postEvent(SHyprIPCEvent{"workspace", PWORKSPACE->m_szName});

    Debug::log(LOG, "Changed to workspace %i", workspaceToChangeTo);
}

void CKeybindManager::fullscreenActive(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    if (PWINDOW->m_iWorkspaceID == SPECIAL_WORKSPACE_ID)
        return;

    g_pCompositor->setWindowFullscreen(PWINDOW, !PWINDOW->m_bIsFullscreen, args == "1" ? FULLSCREEN_MAXIMIZED : FULLSCREEN_FULL);
}

void CKeybindManager::moveActiveToWorkspace(std::string args) {

    CWindow* PWINDOW = nullptr;

    if (args.contains(',')) {
        PWINDOW = g_pCompositor->getWindowByRegex(args.substr(args.find_last_of(',') + 1));
        args = args.substr(0, args.find_last_of(','));
    } else {
        PWINDOW = g_pCompositor->m_pLastWindow;
    }

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    const auto OLDWORKSPACE = g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID);

    // hack
    std::string unusedName;
    const auto WORKSPACEID = getWorkspaceIDFromString(args, unusedName);

    if (WORKSPACEID == PWINDOW->m_iWorkspaceID) {
        Debug::log(LOG, "Not moving to workspace because it didn't change.");
        return;
    }

    auto PSAVEDSIZE = PWINDOW->m_vRealSize.vec();
    auto PSAVEDPOS = PWINDOW->m_vRealPosition.vec();

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(PWINDOW);

    g_pKeybindManager->changeworkspace(args);

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(WORKSPACEID);

    if (PWORKSPACE == OLDWORKSPACE) {
        Debug::log(LOG, "Not moving to workspace because it didn't change.");
        return;
    }

    if (!PWORKSPACE) {
        Debug::log(ERR, "Workspace null in moveActiveToWorkspace?");
        return;
    }

    OLDWORKSPACE->m_bHasFullscreenWindow = false;

    PWINDOW->moveToWorkspace(PWORKSPACE->m_iID);
    PWINDOW->m_iMonitorID = PWORKSPACE->m_iMonitorID;
    PWINDOW->m_bIsFullscreen = false;

    if (PWORKSPACE->m_bHasFullscreenWindow) {
        g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID)->m_bIsFullscreen = false;
        PWORKSPACE->m_bHasFullscreenWindow = false;
    }

    if (PWINDOW->m_bIsFullscreen) {
        PWINDOW->m_bIsFullscreen = false;
        PSAVEDPOS = PSAVEDPOS + Vector2D(10, 10);
        PSAVEDSIZE = PSAVEDSIZE - Vector2D(20, 20);
    }

    // Hack: So that the layout doesnt find our window at the cursor
    PWINDOW->m_vPosition = Vector2D(-42069, -42069);
    
    g_pLayoutManager->getCurrentLayout()->onWindowCreated(PWINDOW);

    // and restore it
    if (PWINDOW->m_bIsFloating) {
        PWINDOW->m_vRealSize.setValue(PSAVEDSIZE);
        PWINDOW->m_vRealPosition.setValueAndWarp(PSAVEDPOS - g_pCompositor->getMonitorFromID(OLDWORKSPACE->m_iMonitorID)->vecPosition + g_pCompositor->getMonitorFromID(PWORKSPACE->m_iMonitorID)->vecPosition);
        PWINDOW->m_vPosition = PWINDOW->m_vRealPosition.vec();
    }

    // undo the damage if we are moving to the special workspace
    if (WORKSPACEID == SPECIAL_WORKSPACE_ID) {
        changeworkspace("[internal]" + std::to_string(OLDWORKSPACE->m_iID));
        OLDWORKSPACE->startAnim(true, true, true);
        toggleSpecialWorkspace("");
        g_pCompositor->getWorkspaceByID(SPECIAL_WORKSPACE_ID)->startAnim(false, false, true);

        for (auto& m : g_pCompositor->m_vMonitors)
            m->specialWorkspaceOpen = false;
    }

    g_pInputManager->refocus();

    PWINDOW->updateToplevel();
}

void CKeybindManager::moveActiveToWorkspaceSilent(std::string args) {
    // hacky, but works lol

    CWindow* PWINDOW = nullptr;

    const auto ORIGINALARGS = args;

    if (args.contains(',')) {
        PWINDOW = g_pCompositor->getWindowByRegex(args.substr(args.find_last_of(',') + 1));
        args = args.substr(0, args.find_last_of(','));
    } else {
        PWINDOW = g_pCompositor->m_pLastWindow;
    }

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    int workspaceToMoveTo = 0;
    std::string workspaceName = "";

    workspaceToMoveTo = getWorkspaceIDFromString(args, workspaceName);

    if (workspaceToMoveTo == INT_MAX) {
        Debug::log(ERR, "Error in moveActiveToWorkspaceSilent, invalid value");
        return;
    }

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);

    if (workspaceToMoveTo == PWINDOW->m_iWorkspaceID)
        return;

    // may be null until later!
    auto PWORKSPACE = g_pCompositor->getWorkspaceByID(workspaceToMoveTo);

    const auto PMONITORNEW = PWORKSPACE ? g_pCompositor->getMonitorFromID(PWORKSPACE->m_iMonitorID) : PMONITOR;

    const auto OLDWORKSPACEIDONMONITOR = PMONITORNEW->activeWorkspace;
    const auto OLDWORKSPACEIDRETURN = PMONITOR->activeWorkspace;

    const auto POLDWORKSPACEONMON = g_pCompositor->getWorkspaceByID(OLDWORKSPACEIDONMONITOR);
    const auto POLDWORKSPACEIDRETURN = g_pCompositor->getWorkspaceByID(OLDWORKSPACEIDRETURN);

    g_pEventManager->m_bIgnoreEvents = true;

    moveActiveToWorkspace(ORIGINALARGS);

    PWORKSPACE = g_pCompositor->getWorkspaceByID(workspaceToMoveTo);

    changeworkspace("[internal]" + std::to_string(OLDWORKSPACEIDONMONITOR));
    changeworkspace("[internal]" + std::to_string(OLDWORKSPACEIDRETURN));

    // revert animations
    PWORKSPACE->m_vRenderOffset.setValueAndWarp(Vector2D(0,0));
    PWORKSPACE->m_fAlpha.setValueAndWarp(0.f);

    POLDWORKSPACEIDRETURN->m_vRenderOffset.setValueAndWarp(Vector2D(0, 0));
    POLDWORKSPACEIDRETURN->m_fAlpha.setValueAndWarp(255.f);

    POLDWORKSPACEONMON->m_vRenderOffset.setValueAndWarp(Vector2D(0, 0));
    POLDWORKSPACEONMON->m_fAlpha.setValueAndWarp(255.f);

    g_pEventManager->m_bIgnoreEvents = false;

    g_pInputManager->refocus();
}

void CKeybindManager::moveFocusTo(std::string args) {
    char arg = args[0];

    if (!isDirection(args)) {
        Debug::log(ERR, "Cannot move focus in direction %c, unsupported direction. Supported: l,r,u/t,d/b", arg);
        return;
    }

    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow;

    if (!PLASTWINDOW)
        return;

    // remove constraints
    g_pInputManager->unconstrainMouse();

    auto switchToWindow = [&](CWindow* PWINDOWTOCHANGETO) {

        if (PLASTWINDOW->m_iWorkspaceID == PWINDOWTOCHANGETO->m_iWorkspaceID && PLASTWINDOW->m_bIsFullscreen) {
            const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PLASTWINDOW->m_iWorkspaceID);
            const auto FSMODE = PWORKSPACE->m_efFullscreenMode;
            g_pCompositor->setWindowFullscreen(PLASTWINDOW, false, FULLSCREEN_FULL);

            g_pCompositor->focusWindow(PWINDOWTOCHANGETO);

            g_pCompositor->setWindowFullscreen(PWINDOWTOCHANGETO, true, FSMODE);
        } else {
            g_pCompositor->focusWindow(PWINDOWTOCHANGETO);
            Vector2D middle = PWINDOWTOCHANGETO->m_vRealPosition.goalv() + PWINDOWTOCHANGETO->m_vRealSize.goalv() / 2.f;
            g_pCompositor->warpCursorTo(middle);
            g_pCompositor->m_pLastMonitor = g_pCompositor->getMonitorFromID(PWINDOWTOCHANGETO->m_iMonitorID); // update last monitor
        }
    };

    if (!g_pCompositor->windowValidMapped(PLASTWINDOW)) {
        const auto PWINDOWTOCHANGETO = g_pCompositor->getFirstWindowOnWorkspace(g_pCompositor->m_pLastMonitor->activeWorkspace);
        if (!PWINDOWTOCHANGETO)
            return;

        switchToWindow(PWINDOWTOCHANGETO);

        return;
    }
    
    const auto PWINDOWTOCHANGETO = g_pCompositor->getWindowInDirection(PLASTWINDOW, arg);

    if (PWINDOWTOCHANGETO) {
        switchToWindow(PWINDOWTOCHANGETO);
    } else {
        const auto PWINDOWNEXT = g_pCompositor->getNextWindowOnWorkspace(PLASTWINDOW);
        if (PWINDOWNEXT) {
            switchToWindow(PWINDOWNEXT);
        }
    }
}

void CKeybindManager::moveActiveTo(std::string args) {
    char arg = args[0];

    const auto LASTMONITOR = g_pCompositor->m_pLastMonitor;

    if (args.find("mon:") == 0) {
        // hack: save the active window
        const auto PACTIVE = g_pCompositor->m_pLastWindow;

        // monitor
        focusMonitor(args.substr(4));

        if (LASTMONITOR == g_pCompositor->m_pLastMonitor) {
            Debug::log(ERR, "moveActiveTo: moving to an invalid mon");
            return;
        }

        // restore the active
        g_pCompositor->focusWindow(PACTIVE);

        moveActiveToWorkspace(std::to_string(g_pCompositor->m_pLastMonitor->activeWorkspace));

        return;
    }

    if (!isDirection(args)) {
        Debug::log(ERR, "Cannot move window in direction %c, unsupported direction. Supported: l,r,u/t,d/b", arg);
        return;
    }

    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PLASTWINDOW) || PLASTWINDOW->m_bIsFullscreen)
        return;

    const auto PWINDOWTOCHANGETO = g_pCompositor->getWindowInDirection(PLASTWINDOW, arg);

    if (!g_pCompositor->windowValidMapped(PWINDOWTOCHANGETO))
        return;

    g_pLayoutManager->getCurrentLayout()->switchWindows(PLASTWINDOW, PWINDOWTOCHANGETO);
}

void CKeybindManager::toggleGroup(std::string args) {
    SLayoutMessageHeader header;
    header.pWindow = g_pCompositor->m_pLastWindow;
    g_pLayoutManager->getCurrentLayout()->layoutMessage(header, "togglegroup");
}

void CKeybindManager::changeGroupActive(std::string args) {
    SLayoutMessageHeader header;
    header.pWindow = g_pCompositor->m_pLastWindow;
    if (args == "b")
        g_pLayoutManager->getCurrentLayout()->layoutMessage(header, "changegroupactiveb");
    else
        g_pLayoutManager->getCurrentLayout()->layoutMessage(header, "changegroupactivef");
}

void CKeybindManager::toggleSplit(std::string args) {
    SLayoutMessageHeader header;
    header.pWindow = g_pCompositor->m_pLastWindow;
    g_pLayoutManager->getCurrentLayout()->layoutMessage(header, "togglesplit");
}

void CKeybindManager::alterSplitRatio(std::string args) {
    float splitratio = 0;

    if (args == "+" || args == "-") {
        Debug::log(LOG, "alterSplitRatio: using LEGACY +/-, consider switching to the Hyprland syntax.");
        splitratio = (args == "+" ? 0.05f : -0.05f);
    }

    if (splitratio == 0) {
        splitratio = getPlusMinusKeywordResult(args, 0);
    }

    if (splitratio == INT_MAX) {
        Debug::log(ERR, "Splitratio invalid in alterSplitRatio!");
        return;
    }

    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PLASTWINDOW))
        return;

    g_pLayoutManager->getCurrentLayout()->alterSplitRatioBy(PLASTWINDOW, splitratio);
}

void CKeybindManager::focusMonitor(std::string arg) {
    const auto PMONITOR = g_pCompositor->getMonitorFromString(arg);

    if (!PMONITOR)
        return;

    changeworkspace("[internal]" + std::to_string(PMONITOR->activeWorkspace));
}

void CKeybindManager::moveCursorToCorner(std::string arg) {
    if (!isNumber(arg)) {
        Debug::log(ERR, "moveCursorToCorner, arg has to be a number.");
        return;
    }

    const auto CORNER = std::stoi(arg);

    if (CORNER < 0 || CORNER > 3) {
        Debug::log(ERR, "moveCursorToCorner, corner not 0 - 3.");
        return;
    }

    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    switch (CORNER) {
        case 0:
            // bottom left
            wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, PWINDOW->m_vRealPosition.vec().x, PWINDOW->m_vRealPosition.vec().y + PWINDOW->m_vRealSize.vec().y);
            break;
        case 1:
            // bottom right
            wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, PWINDOW->m_vRealPosition.vec().x + PWINDOW->m_vRealSize.vec().x, PWINDOW->m_vRealPosition.vec().y + PWINDOW->m_vRealSize.vec().y);
            break;
        case 2:
            // top right
            wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, PWINDOW->m_vRealPosition.vec().x + PWINDOW->m_vRealSize.vec().x, PWINDOW->m_vRealPosition.vec().y);
            break;
        case 3:
            // top left
            wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, PWINDOW->m_vRealPosition.vec().x, PWINDOW->m_vRealPosition.vec().y);
            break;
    }
}

void CKeybindManager::workspaceOpt(std::string args) {
    
    // current workspace
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(g_pCompositor->m_pLastMonitor->activeWorkspace);

    if (!PWORKSPACE)
        return; // ????

    if (args == "allpseudo") {
        PWORKSPACE->m_bDefaultPseudo = !PWORKSPACE->m_bDefaultPseudo;

        // apply
        for (auto& w : g_pCompositor->m_vWindows) {
            if (!w->m_bIsMapped || w->m_iWorkspaceID != PWORKSPACE->m_iID)
                continue;

            w->m_bIsPseudotiled = PWORKSPACE->m_bDefaultPseudo;
        }
    } else if (args == "allfloat") {
        PWORKSPACE->m_bDefaultFloating = !PWORKSPACE->m_bDefaultFloating;
        // apply

        // we make a copy because changeWindowFloatingMode might invalidate the iterator
        std::deque<CWindow*> ptrs;
        for (auto& w : g_pCompositor->m_vWindows)
            ptrs.push_back(w.get());

        for (auto& w : ptrs) {
            if (!w->m_bIsMapped || w->m_iWorkspaceID != PWORKSPACE->m_iID)
                continue;

            if (!w->m_bRequestsFloat && w->m_bIsFloating != PWORKSPACE->m_bDefaultFloating) {
                const auto SAVEDPOS = w->m_vRealPosition.vec();
                const auto SAVEDSIZE = w->m_vRealSize.vec();

                w->m_bIsFloating = PWORKSPACE->m_bDefaultFloating;
                g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(w);

                if (PWORKSPACE->m_bDefaultFloating) {
                    w->m_vRealPosition.setValueAndWarp(SAVEDPOS);
                    w->m_vRealSize.setValueAndWarp(SAVEDSIZE);
                    g_pXWaylandManager->setWindowSize(w, SAVEDSIZE);
                    w->m_vRealSize = w->m_vRealSize.vec() + Vector2D(4,4);
                    w->m_vRealPosition = w->m_vRealPosition.vec() - Vector2D(2,2);
                }
            }
        }
    } else {
        Debug::log(ERR, "Invalid arg in workspaceOpt, opt \"%s\" doesn't exist.", args.c_str());
        return;
    }

    // recalc mon
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(g_pCompositor->m_pLastMonitor->ID);
}

void CKeybindManager::exitHyprland(std::string argz) {
    g_pCompositor->cleanup();
}

void CKeybindManager::moveCurrentWorkspaceToMonitor(std::string args) {
    CMonitor* PMONITOR = g_pCompositor->getMonitorFromString(args);

    if (!PMONITOR)
        return;

    // get the current workspace
    const auto PCURRENTWORKSPACE = g_pCompositor->getWorkspaceByID(g_pCompositor->m_pLastMonitor->activeWorkspace);

    if (!PCURRENTWORKSPACE)
        return;

    g_pCompositor->moveWorkspaceToMonitor(PCURRENTWORKSPACE, PMONITOR);
}

void CKeybindManager::moveWorkspaceToMonitor(std::string args) {
    if (!args.contains(' '))
        return;

    std::string workspace = args.substr(0, args.find_first_of(' '));
    std::string monitor = args.substr(args.find_first_of(' ') + 1);

    const auto PMONITOR = g_pCompositor->getMonitorFromString(monitor);

    if (!PMONITOR){
        Debug::log(ERR, "Ignoring moveWorkspaceToMonitor: monitor doesnt exist");
        return;
    }

    std::string workspaceName;
    const int WORKSPACEID = getWorkspaceIDFromString(workspace, workspaceName);

    if (WORKSPACEID == INT_MAX) {
        Debug::log(ERR, "moveWorkspaceToMonitor invalid workspace!");
        return;
    }

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(WORKSPACEID);

    if (!PWORKSPACE) {
        Debug::log(ERR, "moveWorkspaceToMonitor workspace doesn't exist!");
        return;
    }

    g_pCompositor->moveWorkspaceToMonitor(PWORKSPACE, PMONITOR);
}

void CKeybindManager::toggleSpecialWorkspace(std::string args) {

    if (g_pCompositor->getWindowsOnWorkspace(SPECIAL_WORKSPACE_ID) == 0) {
        Debug::log(LOG, "Can't open empty special workspace!");
        return;
    }

    bool open = false;

    for (auto& m : g_pCompositor->m_vMonitors) {
        if (m->specialWorkspaceOpen) {
            open = true;
            break;
        }
    }

    if (open)
        Debug::log(LOG, "Toggling special workspace to closed");
    else
        Debug::log(LOG, "Toggling special workspace to open");

    if (open) {
        uint64_t monID = -1;

        for (auto& m : g_pCompositor->m_vMonitors) {
            if (m->specialWorkspaceOpen != !open) {
                m->specialWorkspaceOpen = !open;
                g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m->ID);

                g_pCompositor->getWorkspaceByID(SPECIAL_WORKSPACE_ID)->startAnim(false, false);

                monID = m->ID;
            }
        }

        if (const auto PWINDOW = g_pCompositor->getWorkspaceByID(g_pCompositor->m_pLastMonitor->activeWorkspace)->getLastFocusedWindow(); PWINDOW)
            g_pCompositor->focusWindow(PWINDOW);
        else
            g_pInputManager->refocus();
    } else {
        auto PSPECIALWORKSPACE = g_pCompositor->getWorkspaceByID(SPECIAL_WORKSPACE_ID);

        if (!PSPECIALWORKSPACE) {
            // ??? happens sometimes...?
            PSPECIALWORKSPACE = g_pCompositor->m_vWorkspaces.emplace_back(std::make_unique<CWorkspace>(g_pCompositor->m_pLastMonitor->ID, "special", true)).get();
        }

        g_pCompositor->m_pLastMonitor->specialWorkspaceOpen = true;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(g_pCompositor->m_pLastMonitor->ID);

        PSPECIALWORKSPACE->startAnim(true, true);
        PSPECIALWORKSPACE->m_iMonitorID = g_pCompositor->m_pLastMonitor->ID;

        if (const auto PWINDOW = PSPECIALWORKSPACE->getLastFocusedWindow(); PWINDOW)
            g_pCompositor->focusWindow(PWINDOW);
        else
            g_pInputManager->refocus();
    }
}

void CKeybindManager::forceRendererReload(std::string args) {
    bool overAgain = false;

    for (auto& m : g_pCompositor->m_vMonitors) {
        auto rule = g_pConfigManager->getMonitorRuleFor(m->szName);
        if (!g_pHyprRenderer->applyMonitorRule(m.get(), &rule, true)) {
            overAgain = true;
            break;
        }
    }

    if (overAgain)
        forceRendererReload(args);
}

void CKeybindManager::resizeActive(std::string args) {
    if (!g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow))
        return;

    const auto SIZ = g_pCompositor->parseWindowVectorArgsRelative(args, g_pCompositor->m_pLastWindow->m_vRealSize.goalv());

    g_pLayoutManager->getCurrentLayout()->resizeActiveWindow(SIZ - g_pCompositor->m_pLastWindow->m_vRealSize.goalv());

    if (g_pCompositor->m_pLastWindow->m_vRealSize.goalv().x > 1 && g_pCompositor->m_pLastWindow->m_vRealSize.goalv().y > 1)
        g_pCompositor->m_pLastWindow->m_bHidden = false;
}

void CKeybindManager::moveActive(std::string args) {
    if (!g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow))
        return;

    const auto POS = g_pCompositor->parseWindowVectorArgsRelative(args, g_pCompositor->m_pLastWindow->m_vRealPosition.goalv());

    g_pLayoutManager->getCurrentLayout()->moveActiveWindow(POS - g_pCompositor->m_pLastWindow->m_vRealPosition.goalv());
}

void CKeybindManager::moveWindow(std::string args) {

    const auto WINDOWREGEX = args.substr(args.find_first_of(',') + 1);
    const auto MOVECMD = args.substr(0, args.find_first_of(','));

    const auto PWINDOW = g_pCompositor->getWindowByRegex(WINDOWREGEX);

    if (!PWINDOW) {
        Debug::log(ERR, "moveWindow: no window");
        return;
    }

    const auto POS = g_pCompositor->parseWindowVectorArgsRelative(MOVECMD, PWINDOW->m_vRealPosition.goalv());

    g_pLayoutManager->getCurrentLayout()->moveActiveWindow(POS - PWINDOW->m_vRealPosition.goalv(), PWINDOW);
}

void CKeybindManager::resizeWindow(std::string args) {

    const auto WINDOWREGEX = args.substr(args.find_first_of(',') + 1);
    const auto MOVECMD = args.substr(0, args.find_first_of(','));

    const auto PWINDOW = g_pCompositor->getWindowByRegex(WINDOWREGEX);

    if (!PWINDOW) {
        Debug::log(ERR, "resizeWindow: no window");
        return;
    }

    const auto SIZ = g_pCompositor->parseWindowVectorArgsRelative(MOVECMD, PWINDOW->m_vRealSize.goalv());

    g_pLayoutManager->getCurrentLayout()->resizeActiveWindow(SIZ - PWINDOW->m_vRealSize.goalv(), PWINDOW);

    if (PWINDOW->m_vRealSize.goalv().x > 1 && PWINDOW->m_vRealSize.goalv().y > 1)
        PWINDOW->m_bHidden = false;
}

void CKeybindManager::circleNext(std::string arg) {
    if (!g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow))
        return;

    auto switchToWindow = [&](CWindow* PWINDOWTOCHANGETO) {
        if (PWINDOWTOCHANGETO == g_pCompositor->m_pLastWindow || !PWINDOWTOCHANGETO)
            return;

        if (g_pCompositor->m_pLastWindow->m_iWorkspaceID == PWINDOWTOCHANGETO->m_iWorkspaceID && g_pCompositor->m_pLastWindow->m_bIsFullscreen) {
            const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(g_pCompositor->m_pLastWindow->m_iWorkspaceID);
            const auto FSMODE = PWORKSPACE->m_efFullscreenMode;
            g_pCompositor->setWindowFullscreen(g_pCompositor->m_pLastWindow, false, FULLSCREEN_FULL);

            g_pCompositor->focusWindow(PWINDOWTOCHANGETO);

            g_pCompositor->setWindowFullscreen(PWINDOWTOCHANGETO, true, FSMODE);
        } else {
            g_pCompositor->focusWindow(PWINDOWTOCHANGETO);
            Vector2D middle = PWINDOWTOCHANGETO->m_vRealPosition.goalv() + PWINDOWTOCHANGETO->m_vRealSize.goalv() / 2.f;
            g_pCompositor->warpCursorTo(middle);
        }
    };

    if (arg == "last" || arg == "l" || arg == "prev" || arg == "p")
        switchToWindow(g_pCompositor->getPrevWindowOnWorkspace(g_pCompositor->m_pLastWindow));
    else
        switchToWindow(g_pCompositor->getNextWindowOnWorkspace(g_pCompositor->m_pLastWindow));
}

void CKeybindManager::focusWindow(std::string regexp) {
    const auto PWINDOW = g_pCompositor->getWindowByRegex(regexp);

    if (!PWINDOW)
        return;

    Debug::log(LOG, "Focusing to window name: %s", PWINDOW->m_szTitle.c_str());

    changeworkspace("[internal]" + std::to_string(PWINDOW->m_iWorkspaceID));

    g_pCompositor->focusWindow(PWINDOW);

    const auto MIDPOINT = PWINDOW->m_vRealPosition.goalv() + PWINDOW->m_vRealSize.goalv() / 2.f;

    g_pCompositor->warpCursorTo(MIDPOINT);
}

void CKeybindManager::setSubmap(std::string submap) {
    if (submap == "reset" || submap == "") {
        m_szCurrentSelectedSubmap = "";
        Debug::log(LOG, "Reset active submap to the default one.");
        return;
    }

    for (auto& k : g_pKeybindManager->m_lKeybinds) {
        if (k.submap == submap) {
            m_szCurrentSelectedSubmap = submap;
            Debug::log(LOG, "Changed keybind submap to %s", submap.c_str());
            return;
        }
    }

    Debug::log(ERR, "Cannot set submap %s, submap doesn't exist (wasn't registered!)", submap.c_str());
}

void CKeybindManager::pass(std::string regexp) {

    // find the first window passing the regex
    const auto PWINDOW = g_pCompositor->getWindowByRegex(regexp);

    if (!PWINDOW) {
        Debug::log(ERR, "pass: window not found");
        return;
    }

    const auto PLASTSRF = g_pCompositor->m_pLastFocus;

    const auto KEYBOARD = wlr_seat_get_keyboard(g_pCompositor->m_sSeat.seat);

    if (!KEYBOARD){
        Debug::log(ERR, "No kb in pass?");
        return;
    }

    const auto XWTOXW = PWINDOW->m_bIsX11 && g_pCompositor->m_pLastWindow && g_pCompositor->m_pLastWindow->m_bIsX11;
    const auto SL = Vector2D(g_pCompositor->m_sSeat.seat->pointer_state.sx, g_pCompositor->m_sSeat.seat->pointer_state.sy);

    // pass all mf shit
    if (!XWTOXW) {
        if (g_pKeybindManager->m_uLastCode != 0)
            wlr_seat_keyboard_enter(g_pCompositor->m_sSeat.seat, g_pXWaylandManager->getWindowSurface(PWINDOW), KEYBOARD->keycodes, KEYBOARD->num_keycodes, &KEYBOARD->modifiers);
        else
            wlr_seat_pointer_enter(g_pCompositor->m_sSeat.seat, g_pXWaylandManager->getWindowSurface(PWINDOW), 1, 1);
    }
        

    wlr_keyboard_modifiers kbmods = {g_pInputManager->accumulateModsFromAllKBs(), 0, 0, 0};
    wlr_seat_keyboard_notify_modifiers(g_pCompositor->m_sSeat.seat, &kbmods);

    if (g_pKeybindManager->m_iPassPressed == 1) {
        if (g_pKeybindManager->m_uLastCode != 0)
            wlr_seat_keyboard_notify_key(g_pCompositor->m_sSeat.seat, g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastCode - 8, WLR_BUTTON_PRESSED);
        else
            wlr_seat_pointer_notify_button(g_pCompositor->m_sSeat.seat, g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastMouseCode, WLR_BUTTON_PRESSED);
    } else if (g_pKeybindManager->m_iPassPressed == 0)
        if (g_pKeybindManager->m_uLastCode != 0)
            wlr_seat_keyboard_notify_key(g_pCompositor->m_sSeat.seat, g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastCode - 8, WLR_BUTTON_RELEASED);
        else
            wlr_seat_pointer_notify_button(g_pCompositor->m_sSeat.seat, g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastMouseCode, WLR_BUTTON_RELEASED);
    else {
        // dynamic call of the dispatcher
        if (g_pKeybindManager->m_uLastCode != 0) {
            wlr_seat_keyboard_notify_key(g_pCompositor->m_sSeat.seat, g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastCode - 8, WLR_BUTTON_PRESSED);
            wlr_seat_keyboard_notify_key(g_pCompositor->m_sSeat.seat, g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastCode - 8, WLR_BUTTON_RELEASED);
        } else {
            wlr_seat_pointer_notify_button(g_pCompositor->m_sSeat.seat, g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastMouseCode, WLR_BUTTON_PRESSED);
            wlr_seat_pointer_notify_button(g_pCompositor->m_sSeat.seat, g_pKeybindManager->m_uTimeLastMs, g_pKeybindManager->m_uLastMouseCode, WLR_BUTTON_RELEASED);
        }  
    }

    if (XWTOXW)
        return;

    // Massive hack:
    // this will make wlroots NOT send the leave event to XWayland apps, provided we are not on an XWayland window already.
    // please kill me
    if (PWINDOW->m_bIsX11) {
        if (g_pKeybindManager->m_uLastCode != 0) {
            g_pCompositor->m_sSeat.seat->keyboard_state.focused_client = nullptr;
            g_pCompositor->m_sSeat.seat->keyboard_state.focused_surface = nullptr;
        } else {
            g_pCompositor->m_sSeat.seat->pointer_state.focused_client = nullptr;
            g_pCompositor->m_sSeat.seat->pointer_state.focused_surface = nullptr;
        }
    }

    if (g_pKeybindManager->m_uLastCode != 0)
        wlr_seat_keyboard_enter(g_pCompositor->m_sSeat.seat, PLASTSRF, KEYBOARD->keycodes, KEYBOARD->num_keycodes, &KEYBOARD->modifiers);
    else
        wlr_seat_pointer_enter(g_pCompositor->m_sSeat.seat, g_pXWaylandManager->getWindowSurface(PWINDOW), SL.x, SL.y);
}

void CKeybindManager::layoutmsg(std::string msg) {
    SLayoutMessageHeader hd = {g_pCompositor->m_pLastWindow};
    g_pLayoutManager->getCurrentLayout()->layoutMessage(hd, msg);
}

void CKeybindManager::toggleOpaque(std::string unused) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    PWINDOW->m_sAdditionalConfigData.forceOpaque = !PWINDOW->m_sAdditionalConfigData.forceOpaque;

    g_pHyprRenderer->damageWindow(PWINDOW);
}

void CKeybindManager::dpms(std::string arg) {
    bool enable = arg == "on";

    for (auto& m : g_pCompositor->m_vMonitors) {
        wlr_output_enable(m->output, enable);
        
        if (!wlr_output_commit(m->output)) {
            Debug::log(ERR, "Couldn't commit output %s", m->szName.c_str());
        }

        if (enable)
            g_pHyprRenderer->damageMonitor(m.get());
    }

    g_pCompositor->m_bDPMSStateON = enable;
}

void CKeybindManager::swapnext(std::string arg) {

    CWindow* toSwap = nullptr;

    if (!g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow))
        return;

    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow;

    const auto PLASTCYCLED = g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow->m_pLastCycledWindow) && g_pCompositor->m_pLastWindow->m_pLastCycledWindow->m_iWorkspaceID == PLASTWINDOW->m_iWorkspaceID ? g_pCompositor->m_pLastWindow->m_pLastCycledWindow : nullptr;

    if (arg == "last" || arg == "l" || arg == "prev" || arg == "p")
        toSwap = g_pCompositor->getPrevWindowOnWorkspace(PLASTCYCLED ? PLASTCYCLED : PLASTWINDOW);
    else
        toSwap = g_pCompositor->getNextWindowOnWorkspace(PLASTCYCLED ? PLASTCYCLED : PLASTWINDOW);

    // sometimes we may come back to ourselves.
    if (toSwap == PLASTWINDOW) {
        if (arg == "last" || arg == "l" || arg == "prev" || arg == "p")
            toSwap = g_pCompositor->getPrevWindowOnWorkspace(PLASTWINDOW);
        else
            toSwap = g_pCompositor->getNextWindowOnWorkspace(PLASTWINDOW);
    }

    g_pLayoutManager->getCurrentLayout()->switchWindows(PLASTWINDOW, toSwap);

    PLASTWINDOW->m_pLastCycledWindow = toSwap;

    g_pCompositor->focusWindow(PLASTWINDOW);
}

void CKeybindManager::swapActiveWorkspaces(std::string args) {
    const auto MON1 = args.substr(0, args.find_first_of(' '));
    const auto MON2 = args.substr(args.find_first_of(' ') + 1);

    const auto PMON1 = g_pCompositor->getMonitorFromString(MON1);
    const auto PMON2 = g_pCompositor->getMonitorFromString(MON2);

    if (!PMON1 || !PMON2)
        return;

    g_pCompositor->swapActiveWorkspaces(PMON1, PMON2);
}
