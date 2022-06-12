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
    m_mDispatchers["movewindowinv"]             = moveInactiveTo;
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
    m_mDispatchers["cyclenext"]                 = circleNext;
    m_mDispatchers["focuswindowbyclass"]        = focusWindowByClass;
}

void CKeybindManager::addKeybind(SKeybind kb) {
    m_lKeybinds.push_back(kb);
}

void CKeybindManager::removeKeybind(uint32_t mod, const std::string& key) {
    for (auto it = m_lKeybinds.begin(); it != m_lKeybinds.end(); ++it) {
        if (it->modmask == mod && it->key == key) {
            it = m_lKeybinds.erase(it);
        }
    }
}

uint32_t CKeybindManager::stringToModMask(std::string mods) {
    uint32_t modMask = 0;
    if (mods.find("SHIFT") != std::string::npos)
        modMask |= WLR_MODIFIER_SHIFT;
    if (mods.find("CAPS") != std::string::npos)
        modMask |= WLR_MODIFIER_CAPS;
    if (mods.find("CTRL") != std::string::npos || mods.find("CONTROL") != std::string::npos)
        modMask |= WLR_MODIFIER_CTRL;
    if (mods.find("ALT") != std::string::npos)
        modMask |= WLR_MODIFIER_ALT;
    if (mods.find("MOD2") != std::string::npos)
        modMask |= WLR_MODIFIER_MOD2;
    if (mods.find("MOD3") != std::string::npos)
        modMask |= WLR_MODIFIER_MOD3;
    if (mods.find("SUPER") != std::string::npos || mods.find("WIN") != std::string::npos || mods.find("LOGO") != std::string::npos || mods.find("MOD4") != std::string::npos)
        modMask |= WLR_MODIFIER_LOGO;
    if (mods.find("MOD5") != std::string::npos)
        modMask |= WLR_MODIFIER_MOD5;

    return modMask;
}

bool CKeybindManager::handleKeybinds(const uint32_t& modmask, const xkb_keysym_t& key) {
    bool found = false;

    if (handleInternalKeybinds(key))
        return true;

    if (g_pCompositor->m_sSeat.exclusiveClient){
        Debug::log(LOG, "Not handling keybinds due to there being an exclusive inhibited client.");
        return false;
    }

    for (auto& k : m_lKeybinds) {
        if (modmask != k.modmask) 
            continue;

        // oMg such performance hit!!11!
        // this little maneouver is gonna cost us 4µs
        const auto KBKEY = xkb_keysym_from_name(k.key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
        const auto KBKEYUPPER = xkb_keysym_to_upper(KBKEY);
        // small TODO: fix 0-9 keys and other modified ones with shift
        
        if (key != KBKEY && key != KBKEYUPPER)
            continue;


        const auto DISPATCHER = m_mDispatchers.find(k.handler);

        // Should never happen, as we check in the ConfigManager, but oh well
        if (DISPATCHER == m_mDispatchers.end()) {
            Debug::log(ERR, "Inavlid handler in a keybind! (handler %s does not exist)", k.handler.c_str());
        } else {
            // call the dispatcher
            Debug::log(LOG, "Keybind triggered, calling dispatcher (%d, %d)", modmask, KBKEYUPPER);
            DISPATCHER->second(k.arg);
        }

        found = true;
    }

    return found;
}

bool CKeybindManager::handleInternalKeybinds(xkb_keysym_t keysym) {
    // Handles the CTRL+ALT+FX TTY keybinds
    if (!(keysym >= XKB_KEY_XF86Switch_VT_1 && keysym <= XKB_KEY_XF86Switch_VT_12))
        return false;

    const auto PSESSION = wlr_backend_get_session(g_pCompositor->m_sWLRBackend);
    if (PSESSION) {
        const int TTY = keysym - XKB_KEY_XF86Switch_VT_1 + 1;
        wlr_session_change_vt(PSESSION, TTY);

        for (auto& m : g_pCompositor->m_lMonitors) {
            g_pHyprOpenGL->destroyMonitorResources(&m); // mark resources as unusable anymore
            m.noFrameSchedule = true;
            m.framesToSkip = 2;
        }

        Debug::log(LOG, "Switched to VT %i, destroyed all render data, frames to skip for each: 2", TTY);
        
        return true;
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
    if (fork() == 0) {
        execl("/bin/sh", "/bin/sh", "-c", args.c_str(), nullptr);

        _exit(0);
    }
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
        ACTIVEWINDOW->m_bIsFloating = !ACTIVEWINDOW->m_bIsFloating;

        if (ACTIVEWINDOW->m_iWorkspaceID == SPECIAL_WORKSPACE_ID) {
            moveActiveToWorkspace(std::to_string(g_pCompositor->getMonitorFromID(ACTIVEWINDOW->m_iMonitorID)->activeWorkspace));
        }

        ACTIVEWINDOW->m_vRealPosition.setValue(ACTIVEWINDOW->m_vRealPosition.vec() + Vector2D(5, 5));
        ACTIVEWINDOW->m_vSize = ACTIVEWINDOW->m_vRealPosition.vec() - Vector2D(10, 10);

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

    workspaceToChangeTo = getWorkspaceIDFromString(args, workspaceName);

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
            PMONITOR->specialWorkspaceOpen = false;

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

            // Event ONLY if workspace is actually "changed" and we arent just focusing
            if (!m_bSuppressWorkspaceChangeEvents)
                g_pEventManager->postEvent(SHyprIPCEvent("workspace", PWORKSPACETOCHANGETO->m_szName));
        }

        // If the monitor is not the one our cursor's at, warp to it.
        if (PMONITOR != g_pCompositor->getMonitorFromCursor()) {
            Vector2D middle = PMONITOR->vecPosition + PMONITOR->vecSize / 2.f;
            wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, middle.x, middle.y);
        }

        // focus the first window
        g_pCompositor->focusWindow(g_pCompositor->getFirstWindowOnWorkspace(workspaceToChangeTo));

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

    g_pCompositor->m_lWorkspaces.emplace_back(PMONITOR->ID, workspaceToChangeTo == SPECIAL_WORKSPACE_ID);
    const auto PWORKSPACE = &g_pCompositor->m_lWorkspaces.back();

    // start anim on new workspace
    PWORKSPACE->startAnim(true, ANIMTOLEFT);

    // We are required to set the name here immediately
    if (workspaceToChangeTo != SPECIAL_WORKSPACE_ID)
        wlr_ext_workspace_handle_v1_set_name(PWORKSPACE->m_pWlrHandle, workspaceName.c_str());

    PWORKSPACE->m_iID = workspaceToChangeTo;
    PWORKSPACE->m_iMonitorID = PMONITOR->ID;
    PWORKSPACE->m_szName = workspaceName;

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
    if (!m_bSuppressWorkspaceChangeEvents)
        g_pEventManager->postEvent(SHyprIPCEvent("workspace", PWORKSPACE->m_szName));

    Debug::log(LOG, "Changed to workspace %i", workspaceToChangeTo);
}

void CKeybindManager::fullscreenActive(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    g_pLayoutManager->getCurrentLayout()->fullscreenRequestForWindow(PWINDOW, args == "1" ? eFullscreenMode::FULLSCREEN_MAXIMIZED : eFullscreenMode::FULLSCREEN_FULL);

    g_pXWaylandManager->setWindowFullscreen(PWINDOW, PWINDOW->m_bIsFullscreen && (args == "0" || args == ""));
    // make all windows on the same workspace under the fullscreen window
    for (auto& w : g_pCompositor->m_lWindows) {
        if (w.m_iWorkspaceID == PWINDOW->m_iWorkspaceID)
            w.m_bCreatedOverFullscreen = false;
    }
}

void CKeybindManager::moveWindowToWorkspace(CWindow* PWINDOW, int WORKSPACEID) {
    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    const auto OLDWORKSPACE = g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID);

    if (WORKSPACEID == PWINDOW->m_iWorkspaceID) {
        Debug::log(LOG, "Not moving to workspace because it didn't change.");
        return;
    }

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(PWINDOW);

    g_pKeybindManager->changeworkspace(std::to_string(WORKSPACEID));

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(WORKSPACEID);

    if (PWORKSPACE == OLDWORKSPACE) {
        Debug::log(LOG, "Not moving to workspace because it didn't change.");
        return;
    }

    if (!PWORKSPACE) {
        Debug::log(ERR, "Workspace null in moveWindowToWorkspace?");
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
        changeworkspace(std::to_string(OLDWORKSPACE->m_iID));
        OLDWORKSPACE->startAnim(true, true, true);
        toggleSpecialWorkspace("");
        g_pCompositor->getWorkspaceByID(SPECIAL_WORKSPACE_ID)->startAnim(false, false, true);

        for (auto& m : g_pCompositor->m_lMonitors)
            m.specialWorkspaceOpen = false;
    }

    g_pInputManager->refocus();
    g_pCompositor->focusWindow(PWINDOW);
}

void CKeybindManager::moveActiveToWorkspace(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    // hack
    std::string unusedName;
    const auto WORKSPACEID = getWorkspaceIDFromString(args, unusedName);

    if (WORKSPACEID == PWINDOW->m_iWorkspaceID) {
        Debug::log(LOG, "Not moving to workspace because it didn't change.");
        return;
    }

    moveWindowToWorkspace(PWINDOW, WORKSPACEID);
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

    m_bSuppressWorkspaceChangeEvents = true;

    moveActiveToWorkspace(args);

    PWORKSPACE = g_pCompositor->getWorkspaceByID(workspaceToMoveTo);

    changeworkspace(std::to_string(OLDWORKSPACEIDONMONITOR));
    changeworkspace(std::to_string(OLDWORKSPACEIDRETURN));

    // revert animations
    PWORKSPACE->m_vRenderOffset.setValueAndWarp(Vector2D(0,0));
    PWORKSPACE->m_fAlpha.setValueAndWarp(0.f);

    POLDWORKSPACEIDRETURN->m_vRenderOffset.setValueAndWarp(Vector2D(0, 0));
    POLDWORKSPACEIDRETURN->m_fAlpha.setValueAndWarp(255.f);

    POLDWORKSPACEONMON->m_vRenderOffset.setValueAndWarp(Vector2D(0, 0));
    POLDWORKSPACEONMON->m_fAlpha.setValueAndWarp(255.f);

    m_bSuppressWorkspaceChangeEvents = false;

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

void CKeybindManager::moveInactiveTo(std::string args) {
    const auto LASTMONITOR = g_pCompositor->m_pLastMonitor;
    const auto PACTIVE = g_pCompositor->m_pLastWindow;
    const auto CURSOR_COORDS = Vector2D(g_pCompositor->m_sWLRCursor->x, g_pCompositor->m_sWLRCursor->y);

    focusMonitor(args);

    if (LASTMONITOR == g_pCompositor->m_pLastMonitor) {
        Debug::log(ERR, "moveInactiveTo: moving to an invalid mon");
        return;
    }

    for (auto& window : g_pCompositor->m_lWindows) {
        if (window == *PACTIVE || window.m_iWorkspaceID != PACTIVE->m_iWorkspaceID) {
            continue;
        }
        moveWindowToWorkspace(&window, g_pCompositor->m_pLastMonitor->activeWorkspace);
    }

    g_pCompositor->focusWindow(PACTIVE);
    changeworkspace(std::to_string(PACTIVE->m_iWorkspaceID));
    wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, CURSOR_COORDS.x, CURSOR_COORDS.y);
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

        if (monID > -1 && monID < (int)g_pCompositor->m_lMonitors.size()) {
            changeworkspace(std::to_string(g_pCompositor->getMonitorFromID(monID)->activeWorkspace));
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
            for (auto& m : g_pCompositor->m_lMonitors) {
                if (m.szName == arg) {
                    changeworkspace(std::to_string(m.activeWorkspace));
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
        for (auto& w : g_pCompositor->m_lWindows) {
            if (!w.m_bIsMapped || w.m_iWorkspaceID != PWORKSPACE->m_iID)
                continue;

            w.m_bIsPseudotiled = PWORKSPACE->m_bDefaultPseudo;
        }
    } else if (args == "allfloat") {
        PWORKSPACE->m_bDefaultFloating = !PWORKSPACE->m_bDefaultFloating;
        // apply

        // we make a copy because changeWindowFloatingMode might invalidate the iterator
        std::deque<CWindow*> ptrs;
        for (auto& w : g_pCompositor->m_lWindows)
            ptrs.push_back(&w);

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
    g_pCompositor->cleanupExit();
    exit(0);
}

void CKeybindManager::moveCurrentWorkspaceToMonitor(std::string args) {
    if (!isNumber(args) && !isDirection(args)) {
        Debug::log(ERR, "moveCurrentWorkspaceToMonitor arg not a number or direction!");
        return;
    }

    const auto PMONITOR = isDirection(args) ? g_pCompositor->getMonitorInDirection(args[0]) : g_pCompositor->getMonitorFromID(std::stoi(args));

    if (!PMONITOR) {
        Debug::log(ERR, "Ignoring moveCurrentWorkspaceToMonitor: monitor doesnt exist");
        return;
    }

    // get the current workspace
    const auto PCURRENTWORKSPACE = g_pCompositor->getWorkspaceByID(g_pCompositor->m_pLastMonitor->activeWorkspace);

    if (!PCURRENTWORKSPACE)
        return;

    g_pCompositor->moveWorkspaceToMonitor(PCURRENTWORKSPACE, PMONITOR);
}

void CKeybindManager::moveWorkspaceToMonitor(std::string args) {
    if (args.find_first_of(' ') == std::string::npos)
        return;

    std::string workspace = args.substr(0, args.find_first_of(' '));
    std::string monitor = args.substr(args.find_first_of(' ') + 1);

    if (!isNumber(monitor) && !isDirection(monitor)) {
        Debug::log(ERR, "moveWorkspaceToMonitor monitor arg not a number or direction!");
        return;
    }

    const auto PMONITOR = isDirection(monitor) ? g_pCompositor->getMonitorInDirection(monitor[0]) : g_pCompositor->getMonitorFromID(std::stoi(monitor));

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

    for (auto& m : g_pCompositor->m_lMonitors) {
        if (m.specialWorkspaceOpen) {
            open = true;
            break;
        }
    }

    if (open)
        Debug::log(LOG, "Toggling special workspace to closed");
    else
        Debug::log(LOG, "Toggling special workspace to open");

    if (open) {
        for (auto& m : g_pCompositor->m_lMonitors) {
            if (m.specialWorkspaceOpen != !open) {
                m.specialWorkspaceOpen = !open;
                g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m.ID);

                g_pCompositor->getWorkspaceByID(SPECIAL_WORKSPACE_ID)->startAnim(false, false);
            }
        }
    } else {
        g_pCompositor->m_pLastMonitor->specialWorkspaceOpen = true;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(g_pCompositor->m_pLastMonitor->ID);

        g_pCompositor->getWorkspaceByID(SPECIAL_WORKSPACE_ID)->startAnim(true, true);
    }

    g_pInputManager->refocus();
}

void CKeybindManager::forceRendererReload(std::string args) {
    for (auto& m : g_pCompositor->m_lMonitors) {
        auto rule = g_pConfigManager->getMonitorRuleFor(m.szName);
        g_pHyprRenderer->applyMonitorRule(&m, &rule, true);
    }
}

void CKeybindManager::resizeActive(std::string args) {
    if (args.find_first_of(' ') == std::string::npos)
        return;

    std::string x = args.substr(0, args.find_first_of(' '));
    std::string y = args.substr(args.find_first_of(' ') + 1);

    if (!isNumber(x) || !isNumber(y)) {
        Debug::log(ERR, "resizeTiledWindow: args not numbers");
        return;
    }

    const int X = std::stoi(x);
    const int Y = std::stoi(y);

    g_pLayoutManager->getCurrentLayout()->resizeActiveWindow(Vector2D(X, Y));
}

void CKeybindManager::circleNext(std::string) {
    if (!g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow))
        return;

    g_pCompositor->focusWindow(g_pCompositor->getNextWindowOnWorkspace(g_pCompositor->m_pLastWindow));

    const auto MIDPOINT = g_pCompositor->m_pLastWindow->m_vRealPosition.goalv() + g_pCompositor->m_pLastWindow->m_vRealSize.goalv() / 2.f;

    wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, MIDPOINT.x, MIDPOINT.y);
}

void CKeybindManager::focusWindowByClass(std::string clazz) {
    std::regex classCheck(clazz);

    for (auto& w : g_pCompositor->m_lWindows) {
        const auto windowClass = g_pXWaylandManager->getAppIDClass(&w);

        if (!std::regex_search(windowClass, classCheck))
            continue;

        Debug::log(LOG, "Focusing to window name: %s", w.m_szTitle.c_str());

        changeworkspace(std::to_string(w.m_iWorkspaceID));

        g_pCompositor->focusWindow(&w);

        const auto MIDPOINT = w.m_vRealPosition.goalv() + w.m_vRealSize.goalv() / 2.f;

        wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, MIDPOINT.x, MIDPOINT.y);

        break;
    }
}
