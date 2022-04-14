#include "KeybindManager.hpp"

void CKeybindManager::addKeybind(SKeybind kb) {
    m_dKeybinds.push_back(kb);
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

    for (auto& k : m_dKeybinds) {
        if (modmask != k.modmask) 
            continue;

        // oMg such performance hit!!11!
        // this little maneouver is gonna cost us 4µs
        const auto KBKEY = xkb_keysym_from_name(k.key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
        const auto KBKEYUPPER = xkb_keysym_to_upper(KBKEY);
        // small TODO: fix 0-9 keys and other modified ones with shift
        
        if (key != KBKEY && key != KBKEYUPPER)
            continue;

        // yes.
        if (k.handler == "exec") { spawn(k.arg); }
        else if (k.handler == "killactive") { killActive(k.arg); }
        else if (k.handler == "togglefloating") { toggleActiveFloating(k.arg); }
        else if (k.handler == "workspace") { changeworkspace(k.arg); }
        else if (k.handler == "fullscreen") { fullscreenActive(k.arg); }
        else if (k.handler == "movetoworkspace") { moveActiveToWorkspace(k.arg); }
        else if (k.handler == "pseudo") { toggleActivePseudo(k.arg); }
        else if (k.handler == "movefocus") { moveFocusTo(k.arg); }
        else if (k.handler == "togglegroup") { toggleGroup(k.arg); }
        else if (k.handler == "changegroupactive") { changeGroupActive(k.arg); }
        else {
            Debug::log(ERR, "Inavlid handler in a keybind! (handler %s does not exist)", k.handler.c_str());
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
        const auto TTY = keysym - XKB_KEY_XF86Switch_VT_1 + 1;
        wlr_session_change_vt(PSESSION, TTY);
        return true;
    }

    return false;
}

// Dispatchers

void CKeybindManager::spawn(std::string args) {
    args = "WAYLAND_DISPLAY=" + std::string(g_pCompositor->m_szWLDisplaySocket) + " DISPLAY=" + std::string(g_pXWaylandManager->m_sWLRXWayland->display_name) + " " + args;
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
    }
    
    g_pCompositor->focusWindow(g_pCompositor->windowFromCursor());
}

void CKeybindManager::clearKeybinds() {
    m_dKeybinds.clear();
}

void CKeybindManager::toggleActiveFloating(std::string args) {
    const auto ACTIVEWINDOW = g_pCompositor->m_pLastWindow;

    if (g_pCompositor->windowValidMapped(ACTIVEWINDOW)) {
        ACTIVEWINDOW->m_bIsFloating = !ACTIVEWINDOW->m_bIsFloating;

        ACTIVEWINDOW->m_vRealPosition = ACTIVEWINDOW->m_vRealPosition + Vector2D(5, 5);
        ACTIVEWINDOW->m_vSize = ACTIVEWINDOW->m_vRealPosition - Vector2D(10, 10);

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

    if (args.find_first_of("+") == 0) {
        try {
            workspaceToChangeTo = g_pCompositor->m_pLastMonitor->activeWorkspace + std::stoi(args.substr(1));
        } catch (...) {
            Debug::log(ERR, "Invalid arg \"%s\" in changeWorkspace!", args.c_str());
            return;
        }
    } else if (args.find_first_of("-") == 0) {
        try {
            workspaceToChangeTo = std::clamp(g_pCompositor->m_pLastMonitor->activeWorkspace - std::stoi(args.substr(1)), 1, INT_MAX);
        } catch (...) {
            Debug::log(ERR, "Invalid arg \"%s\" in changeWorkspace!", args.c_str());
            return;
        }
    } else {
        try {
            workspaceToChangeTo = stoi(args);
        } catch (...) {
            Debug::log(ERR, "Invalid arg \"%s\" in changeWorkspace!", args.c_str());
            return;
        }
    }

    // if it exists, we warp to it
    if (g_pCompositor->getWorkspaceByID(workspaceToChangeTo)) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(g_pCompositor->getWorkspaceByID(workspaceToChangeTo)->m_iMonitorID);

        // if it's not visible, make it visible.
        if (!g_pCompositor->isWorkspaceVisible(workspaceToChangeTo)) {
            const auto OLDWORKSPACEID = PMONITOR->activeWorkspace;

            // change it
            PMONITOR->activeWorkspace = workspaceToChangeTo;

            // we need to move XWayland windows to narnia or otherwise they will still process our cursor and shit
            // and that'd be annoying as hell
            g_pCompositor->fixXWaylandWindowsOnWorkspace(OLDWORKSPACEID);

            // and fix on the new workspace
            g_pCompositor->fixXWaylandWindowsOnWorkspace(PMONITOR->activeWorkspace);
        }
           

        // If the monitor is not the one our cursor's at, warp to it.
        if (PMONITOR != g_pCompositor->getMonitorFromCursor()) {
            Vector2D middle = PMONITOR->vecPosition + PMONITOR->vecSize / 2.f;
            wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, middle.x, middle.y);
        }

        // focus the first window
        g_pCompositor->focusWindow(g_pCompositor->getFirstWindowOnWorkspace(workspaceToChangeTo));

        // set active and deactivate all other in wlr
        g_pCompositor->deactivateAllWLRWorkspaces();
        wlr_ext_workspace_handle_v1_set_active(g_pCompositor->getWorkspaceByID(workspaceToChangeTo)->m_pWlrHandle, true);

        Debug::log(LOG, "Changed to workspace %i", workspaceToChangeTo);

        // focus
        g_pInputManager->refocus();

        return;
    }

    // Workspace doesn't exist, create and switch
    const auto PMONITOR = g_pCompositor->getMonitorFromCursor();

    const auto OLDWORKSPACE = PMONITOR->activeWorkspace;

    g_pCompositor->m_lWorkspaces.emplace_back(PMONITOR->ID);
    const auto PWORKSPACE = &g_pCompositor->m_lWorkspaces.back();

    // We are required to set the name here immediately
    wlr_ext_workspace_handle_v1_set_name(PWORKSPACE->m_pWlrHandle, std::to_string(workspaceToChangeTo).c_str());

    PWORKSPACE->m_iID = workspaceToChangeTo;
    PWORKSPACE->m_iMonitorID = PMONITOR->ID;
    
    PMONITOR->activeWorkspace = workspaceToChangeTo;

    // we need to move XWayland windows to narnia or otherwise they will still process our cursor and shit
    // and that'd be annoying as hell
    g_pCompositor->fixXWaylandWindowsOnWorkspace(OLDWORKSPACE);

    // set active and deactivate all other
    g_pCompositor->deactivateAllWLRWorkspaces();
    wlr_ext_workspace_handle_v1_set_active(PWORKSPACE->m_pWlrHandle, true);

    // focus (clears the last)
    g_pInputManager->refocus();

    Debug::log(LOG, "Changed to workspace %i", workspaceToChangeTo);
}

void CKeybindManager::fullscreenActive(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    g_pLayoutManager->getCurrentLayout()->fullscreenRequestForWindow(PWINDOW);

    g_pXWaylandManager->setWindowFullscreen(PWINDOW, PWINDOW->m_bIsFullscreen);

    // make all windows on the same workspace under the fullscreen window
    for (auto& w : g_pCompositor->m_lWindows) {
        if (w.m_iWorkspaceID == PWINDOW->m_iWorkspaceID)
            w.m_bCreatedOverFullscreen = false;
    }
}

void CKeybindManager::moveActiveToWorkspace(std::string args) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    int workspaceID;
    try {
        workspaceID = stoi(args);
    } catch( ... ) {
        Debug::log(ERR, "Invalid movetoworkspace: %s", args.c_str());
        return;
    }

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(PWINDOW);

    const auto OLDWORKSPACE = g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID);

    // hack
    g_pKeybindManager->changeworkspace(std::to_string(workspaceID));

    const auto NEWWORKSPACE = g_pCompositor->getWorkspaceByID(workspaceID);

    OLDWORKSPACE->m_bHasFullscreenWindow = false;

    PWINDOW->m_iWorkspaceID = workspaceID;
    PWINDOW->m_iMonitorID = NEWWORKSPACE->m_iMonitorID;
    PWINDOW->m_bIsFullscreen = false;

    if (NEWWORKSPACE->m_bHasFullscreenWindow) {
        g_pCompositor->getFullscreenWindowOnWorkspace(workspaceID)->m_bIsFullscreen = false;
        NEWWORKSPACE->m_bHasFullscreenWindow = false;
    }

    // Hack: So that the layout doesnt find our window at the cursor
    PWINDOW->m_vPosition = Vector2D(-42069, -42069);
    
    // Save the real position and size because the layout might set its own
    const auto PSAVEDSIZE = PWINDOW->m_vRealSize;
    const auto PSAVEDPOS = PWINDOW->m_vRealPosition;
    g_pLayoutManager->getCurrentLayout()->onWindowCreated(PWINDOW);
    // and restore it
    PWINDOW->m_vRealPosition = PSAVEDPOS;
    PWINDOW->m_vRealSize = PSAVEDSIZE;

    if (PWINDOW->m_bIsFloating) {
        PWINDOW->m_vRealPosition = PWINDOW->m_vRealPosition - g_pCompositor->getMonitorFromID(OLDWORKSPACE->m_iMonitorID)->vecPosition;
        PWINDOW->m_vRealPosition = PWINDOW->m_vRealPosition + g_pCompositor->getMonitorFromID(NEWWORKSPACE->m_iMonitorID)->vecPosition;
        PWINDOW->m_vEffectivePosition = PWINDOW->m_vRealPosition;
        PWINDOW->m_vPosition = PWINDOW->m_vRealPosition;
    }
}

void CKeybindManager::moveFocusTo(std::string args) {
    char arg = args[0];

    if (arg != 'l' && arg != 'r' && arg != 'u' && arg != 'd' && arg != 't' && arg != 'b') {
        Debug::log(ERR, "Cannot move window in direction %c, unsupported direction. Supported: l,r,u/t,d/b", arg);
        return;
    }

    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow;

    auto switchToWindow = [&](CWindow* PWINDOWTOCHANGETO) {
        g_pCompositor->focusWindow(PWINDOWTOCHANGETO);
        Vector2D middle = PWINDOWTOCHANGETO->m_vEffectivePosition + PWINDOWTOCHANGETO->m_vEffectiveSize / 2.f;
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

void CKeybindManager::toggleGroup(std::string args) {
    g_pLayoutManager->getCurrentLayout()->toggleWindowGroup(g_pCompositor->m_pLastWindow);
}

void CKeybindManager::changeGroupActive(std::string args) {
    g_pLayoutManager->getCurrentLayout()->switchGroupWindow(g_pCompositor->m_pLastWindow);
}