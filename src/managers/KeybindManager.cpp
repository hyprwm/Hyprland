#include "KeybindManager.hpp"

#include <regex>

CKeybindManager::CKeybindManager() {
    // initialize all dispatchers

    m_mDispatchers["exec"]                      = spawn;
    m_mDispatchers["killactive"]                = killActive;
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
}

void CKeybindManager::addKeybind(SKeybind kb) {
    m_lKeybinds.push_back(kb);
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

bool CKeybindManager::handleKeybinds(const uint32_t& modmask, const xkb_keysym_t& key, const int& keycode) {
    bool found = false;

    if (handleInternalKeybinds(key))
        return true;

    if (g_pCompositor->m_sSeat.exclusiveClient)
        Debug::log(LOG, "Keybind handling only locked (inhibitor)");

    for (auto& k : m_lKeybinds) {
        if (modmask != k.modmask || (g_pCompositor->m_sSeat.exclusiveClient && !k.locked) || k.submap != m_szCurrentSelectedSubmap)
            continue;


        if (k.keycode != -1) {
            if (keycode != k.keycode)
                continue;

        } else {
            if (key == 0)
                continue;  // this is a keycode check run

            // oMg such performance hit!!11!
            // this little maneouver is gonna cost us 4µs
            const auto KBKEY = xkb_keysym_from_name(k.key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
            const auto KBKEYUPPER = xkb_keysym_to_upper(KBKEY);
            // small TODO: fix 0-9 keys and other modified ones with shift

            if (key != KBKEY && key != KBKEYUPPER)
                continue;
        }

        const auto DISPATCHER = m_mDispatchers.find(k.handler);

        // Should never happen, as we check in the ConfigManager, but oh well
        if (DISPATCHER == m_mDispatchers.end()) {
            Debug::log(ERR, "Inavlid handler in a keybind! (handler %s does not exist)", k.handler.c_str());
        } else {
            // call the dispatcher
            Debug::log(LOG, "Keybind triggered, calling dispatcher (%d, %d)", modmask, key);
            DISPATCHER->second(k.arg);
        }

        found = true;
    }

    return found;
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
    if (g_pCompositor->m_pLastWindow && g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow)) {
        g_pXWaylandManager->sendCloseWindow(g_pCompositor->m_pLastWindow);
        g_pCompositor->m_pLastFocus = nullptr;
        g_pCompositor->m_pLastWindow = nullptr;
        g_pEventManager->postEvent(SHyprIPCEvent("activewindow", ",")); // post an activewindow event to empty, as we are currently unfocused
    }
    
    g_pCompositor->focusWindow(g_pCompositor->windowFromCursor());
}

void CKeybindManager::clearKeybinds() {
    m_lKeybinds.clear();
}

void CKeybindManager::toggleActiveFloating(std::string args) {
    const auto ACTIVEWINDOW = g_pCompositor->m_pLastWindow;

    if (g_pCompositor->windowValidMapped(ACTIVEWINDOW)) {
        // remove drag status
        g_pInputManager->currentlyDraggedWindow = nullptr;

        ACTIVEWINDOW->m_bIsFloating = !ACTIVEWINDOW->m_bIsFloating;

        if (ACTIVEWINDOW->m_iWorkspaceID == SPECIAL_WORKSPACE_ID) {
            moveActiveToWorkspace(std::to_string(g_pCompositor->getMonitorFromID(ACTIVEWINDOW->m_iMonitorID)->activeWorkspace));
        }

        g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(ACTIVEWINDOW);
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

    if (args.find("[internal]") == 0) {
        workspaceToChangeTo = std::stoi(args.substr(10));
        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(workspaceToChangeTo);
        if (PWORKSPACE)
            workspaceName = PWORKSPACE->m_szName;
    } else {
        workspaceToChangeTo = getWorkspaceIDFromString(args, workspaceName);
    }

    if (workspaceToChangeTo == INT_MAX) {
        Debug::log(ERR, "Error in changeworkspace, invalid value");
        return;
    }

    // remove constraints 
    g_pCompositor->m_sSeat.mouse->constraintActive = false;

    // if it exists, we warp to it
    if (g_pCompositor->getWorkspaceByID(workspaceToChangeTo)) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(g_pCompositor->getWorkspaceByID(workspaceToChangeTo)->m_iMonitorID);

        const auto PWORKSPACETOCHANGETO = g_pCompositor->getWorkspaceByID(workspaceToChangeTo);

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

            // we need to move XWayland windows to narnia or otherwise they will still process our cursor and shit
            // and that'd be annoying as hell
            g_pCompositor->fixXWaylandWindowsOnWorkspace(OLDWORKSPACEID);

            // and fix on the new workspace
            g_pCompositor->fixXWaylandWindowsOnWorkspace(PMONITOR->activeWorkspace);

            // here and only here begin anim. we don't want to anim visible workspaces on other monitors.
            // check if anim left or right
            const auto ANIMTOLEFT = workspaceToChangeTo > OLDWORKSPACEID;

            // start anim on old workspace
            g_pCompositor->getWorkspaceByID(OLDWORKSPACEID)->startAnim(false, ANIMTOLEFT);

            // start anim on new workspace
            PWORKSPACETOCHANGETO->startAnim(true, ANIMTOLEFT);

            g_pEventManager->postEvent(SHyprIPCEvent("workspace", PWORKSPACETOCHANGETO->m_szName));
        }

        // If the monitor is not the one our cursor's at, warp to it.
        if (PMONITOR != g_pCompositor->getMonitorFromCursor()) {
            Vector2D middle = PMONITOR->vecPosition + PMONITOR->vecSize / 2.f;
            wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, middle.x, middle.y);
        }

        // set active and deactivate all other in wlr
        g_pCompositor->deactivateAllWLRWorkspaces(PWORKSPACETOCHANGETO->m_pWlrHandle);
        PWORKSPACETOCHANGETO->setActive(true);

        // recalc layout
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PWORKSPACETOCHANGETO->m_iMonitorID);

        Debug::log(LOG, "Changed to workspace %i", workspaceToChangeTo);

        // focus
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

    // we need to move XWayland windows to narnia or otherwise they will still process our cursor and shit
    // and that'd be annoying as hell
    g_pCompositor->fixXWaylandWindowsOnWorkspace(OLDWORKSPACE);

    // set active and deactivate all other
    g_pCompositor->deactivateAllWLRWorkspaces(PWORKSPACE->m_pWlrHandle);
    PWORKSPACE->setActive(true);

    // mark the monitor dirty
    g_pHyprRenderer->damageMonitor(PMONITOR);

    // focus (clears the last)
    g_pInputManager->refocus();

    // Event
    g_pEventManager->postEvent(SHyprIPCEvent("workspace", PWORKSPACE->m_szName));

    Debug::log(LOG, "Changed to workspace %i", workspaceToChangeTo);
}

void CKeybindManager::fullscreenActive(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    g_pCompositor->setWindowFullscreen(PWINDOW, !PWINDOW->m_bIsFullscreen, args == "1" ? FULLSCREEN_MAXIMIZED : FULLSCREEN_FULL);
}

void CKeybindManager::moveActiveToWorkspace(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

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

    PWINDOW->m_iWorkspaceID = PWORKSPACE->m_iID;
    PWINDOW->m_iMonitorID = PWORKSPACE->m_iMonitorID;
    PWINDOW->m_bIsFullscreen = false;

    if (PWORKSPACE->m_bHasFullscreenWindow) {
        g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID)->m_bIsFullscreen = false;
        PWORKSPACE->m_bHasFullscreenWindow = false;
    }

    // Hack: So that the layout doesnt find our window at the cursor
    PWINDOW->m_vPosition = Vector2D(-42069, -42069);
    
    // Save the real position and size because the layout might set its own
    const auto PSAVEDSIZE = PWINDOW->m_vRealSize.vec();
    const auto PSAVEDPOS = PWINDOW->m_vRealPosition.vec();
    g_pLayoutManager->getCurrentLayout()->onWindowCreated(PWINDOW);
    // and restore it
    PWINDOW->m_vRealPosition.setValue(PSAVEDPOS);
    PWINDOW->m_vRealSize.setValue(PSAVEDSIZE);

    if (PWINDOW->m_bIsFloating) {
        PWINDOW->m_vRealPosition.setValue(PWINDOW->m_vRealPosition.vec() - g_pCompositor->getMonitorFromID(OLDWORKSPACE->m_iMonitorID)->vecPosition);
        PWINDOW->m_vRealPosition.setValue(PWINDOW->m_vRealPosition.vec() + g_pCompositor->getMonitorFromID(PWORKSPACE->m_iMonitorID)->vecPosition);
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
}

void CKeybindManager::moveActiveToWorkspaceSilent(std::string args) {
    // hacky, but works lol

    int workspaceToMoveTo = 0;
    std::string workspaceName = "";

    workspaceToMoveTo = getWorkspaceIDFromString(args, workspaceName);

    if (workspaceToMoveTo == INT_MAX) {
        Debug::log(ERR, "Error in moveActiveToWorkspaceSilent, invalid value");
        return;
    }

    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);

    if (workspaceToMoveTo == PMONITOR->activeWorkspace)
        return;

    // may be null until later!
    auto PWORKSPACE = g_pCompositor->getWorkspaceByID(workspaceToMoveTo);

    const auto PMONITORNEW = PWORKSPACE ? g_pCompositor->getMonitorFromID(PWORKSPACE->m_iMonitorID) : PMONITOR;

    const auto OLDWORKSPACEIDONMONITOR = PMONITORNEW->activeWorkspace;
    const auto OLDWORKSPACEIDRETURN = PMONITOR->activeWorkspace;

    const auto POLDWORKSPACEONMON = g_pCompositor->getWorkspaceByID(OLDWORKSPACEIDONMONITOR);
    const auto POLDWORKSPACEIDRETURN = g_pCompositor->getWorkspaceByID(OLDWORKSPACEIDRETURN);

    g_pEventManager->m_bIgnoreEvents = true;

    moveActiveToWorkspace(args);

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

    // remove constraints
    g_pCompositor->m_sSeat.mouse->constraintActive = false;

    auto switchToWindow = [&](CWindow* PWINDOWTOCHANGETO) {
        g_pCompositor->focusWindow(PWINDOWTOCHANGETO);
        Vector2D middle = PWINDOWTOCHANGETO->m_vRealPosition.goalv() + PWINDOWTOCHANGETO->m_vRealSize.goalv() / 2.f;
        wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, middle.x, middle.y);
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

    if (!g_pCompositor->windowValidMapped(PLASTWINDOW))
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
    if (isNumber(arg)) {
        // change by ID
        int monID = -1;
        try {
            monID = std::stoi(arg);
        } catch (std::exception& e) {
            // shouldn't happen but jic
            Debug::log(ERR, "Error in focusMonitor: invalid num");
        }

        if (monID > -1 && monID < (int)g_pCompositor->m_vMonitors.size()) {
            changeworkspace("[internal]" + std::to_string(g_pCompositor->getMonitorFromID(monID)->activeWorkspace));
        } else {
            Debug::log(ERR, "Error in focusMonitor: invalid arg 1");
        }
    } else {

        if (isDirection(arg)) {
            const auto PMONITOR = g_pCompositor->getMonitorInDirection(arg[0]);
            if (PMONITOR) {
                if (PMONITOR->activeWorkspace < 0) {
                    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);
                    changeworkspace("name:" + PWORKSPACE->m_szName);
                }
                else
                    changeworkspace(std::to_string(PMONITOR->activeWorkspace));
                return;
            }
        } else {
            for (auto& m : g_pCompositor->m_vMonitors) {
                if (m->szName == arg) {
                    changeworkspace("[internal]" + std::to_string(m->activeWorkspace));
                    return;
                }
            }
        }        

        Debug::log(ERR, "Error in focusMonitor: no such monitor");
    }
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
    SMonitor* PMONITOR = nullptr;

    try {
        if (!isNumber(args) && !isDirection(args)) {
            PMONITOR = g_pCompositor->getMonitorFromName(args);
        } else {
            PMONITOR = isDirection(args) ? g_pCompositor->getMonitorInDirection(args[0]) : g_pCompositor->getMonitorFromID(std::stoi(args));
        }
    } catch (std::exception& e) {
        Debug::log(LOG, "moveCurrentWorkspaceToMonitor: caught exception in monitor", e.what());
        return;
    }

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

    SMonitor* PMONITOR = nullptr;

    try {
        if (!isNumber(monitor) && !isDirection(monitor)) {
            PMONITOR = g_pCompositor->getMonitorFromName(monitor);
        } else {
            PMONITOR = isDirection(monitor) ? g_pCompositor->getMonitorInDirection(monitor[0]) : g_pCompositor->getMonitorFromID(std::stoi(monitor));
        }
    } catch (std::exception& e) {
        Debug::log(LOG, "moveWorkspaceToMonitor: caught exception in monitor", e.what());
        return;
    }
    

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
        for (auto& m : g_pCompositor->m_vMonitors) {
            if (m->specialWorkspaceOpen != !open) {
                m->specialWorkspaceOpen = !open;
                g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m->ID);

                g_pCompositor->getWorkspaceByID(SPECIAL_WORKSPACE_ID)->startAnim(false, false);
            }
        }
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
    }

    g_pInputManager->refocus();
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
    if (!args.contains(' '))
        return;

    std::string x = args.substr(0, args.find_first_of(' '));
    std::string y = args.substr(args.find_first_of(' ') + 1);

    if (x == "exact") {
        std::string newX = y.substr(0, y.find_first_of(' '));
        std::string newY = y.substr(y.find_first_of(' ') + 1);

        if (!isNumber(newX) || !isNumber(newY)) {
            Debug::log(ERR, "resizeTiledWindow: exact args not numbers");
            return;
        }

        const int X = std::stoi(newX);
        const int Y = std::stoi(newY);

        if (X < 10 || Y < 10) {
            Debug::log(ERR, "resizeTiledWindow: exact args cannot be < 10");
            return;
        }

        // calc the delta
        if (!g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow))
            return; // ignore

        const auto PWINDOW = g_pCompositor->m_pLastWindow;

        const int DX = X - PWINDOW->m_vRealSize.goalv().x;
        const int DY = Y - PWINDOW->m_vRealSize.goalv().y;

        g_pLayoutManager->getCurrentLayout()->resizeActiveWindow(Vector2D(DX, DY));

        return;
    }

    if (!isNumber(x) || !isNumber(y)) {
        Debug::log(ERR, "resizeTiledWindow: args not numbers");
        return;
    }

    const int X = std::stoi(x);
    const int Y = std::stoi(y);

    g_pLayoutManager->getCurrentLayout()->resizeActiveWindow(Vector2D(X, Y));
}

void CKeybindManager::moveActive(std::string args) {
    if (!args.contains(' '))
        return;

    std::string x = args.substr(0, args.find_first_of(' '));
    std::string y = args.substr(args.find_first_of(' ') + 1);

    if (x == "exact") {
        std::string newX = y.substr(0, y.find_first_of(' '));
        std::string newY = y.substr(y.find_first_of(' ') + 1);

        if (!isNumber(newX) || !isNumber(newY)) {
            Debug::log(ERR, "moveActive: exact args not numbers");
            return;
        }

        const int X = std::stoi(newX);
        const int Y = std::stoi(newY);

        if (X < 0 || Y < 0) {
            Debug::log(ERR, "moveActive: exact args cannot be < 0");
            return;
        }

        // calc the delta
        if (!g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow))
            return;  // ignore

        const auto PWINDOW = g_pCompositor->m_pLastWindow;

        const int DX = X - PWINDOW->m_vRealPosition.goalv().x;
        const int DY = Y - PWINDOW->m_vRealPosition.goalv().y;

        g_pLayoutManager->getCurrentLayout()->moveActiveWindow(Vector2D(DX, DY));

        return;
    }

    if (!isNumber(x) || !isNumber(y)) {
        Debug::log(ERR, "moveActive: args not numbers");
        return;
    }

    const int X = std::stoi(x);
    const int Y = std::stoi(y);

    g_pLayoutManager->getCurrentLayout()->moveActiveWindow(Vector2D(X, Y));
}

void CKeybindManager::circleNext(std::string arg) {
    if (!g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow))
        return;

    if (arg == "last" || arg == "l" || arg == "prev" || arg == "p")
        g_pCompositor->focusWindow(g_pCompositor->getPrevWindowOnWorkspace(g_pCompositor->m_pLastWindow));
    else
        g_pCompositor->focusWindow(g_pCompositor->getNextWindowOnWorkspace(g_pCompositor->m_pLastWindow));

    const auto MIDPOINT = g_pCompositor->m_pLastWindow->m_vRealPosition.goalv() + g_pCompositor->m_pLastWindow->m_vRealSize.goalv() / 2.f;

    wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, MIDPOINT.x, MIDPOINT.y);
}

void CKeybindManager::focusWindow(std::string regexp) {
    bool titleRegex = false;
    std::regex regexCheck(regexp);
    if (regexp.find("title:") == 0) {
        titleRegex = true;
        regexCheck = std::regex(regexp.substr(6));
    }

    for (auto& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped || w->m_bHidden)
            continue;

        if (titleRegex) {
            const auto windowTitle = g_pXWaylandManager->getTitle(w.get());
            if (!std::regex_search(windowTitle, regexCheck))
                continue;
        }
        else {
            const auto windowClass = g_pXWaylandManager->getAppIDClass(w.get());
            if (!std::regex_search(windowClass, regexCheck))
                continue;
        }

        Debug::log(LOG, "Focusing to window name: %s", w->m_szTitle.c_str());

        changeworkspace("[internal]" + std::to_string(w->m_iWorkspaceID));

        g_pCompositor->focusWindow(w.get());

        const auto MIDPOINT = w->m_vRealPosition.goalv() + w->m_vRealSize.goalv() / 2.f;

        wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, MIDPOINT.x, MIDPOINT.y);

        break;
    }
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
