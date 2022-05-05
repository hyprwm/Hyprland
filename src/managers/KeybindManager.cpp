#include "KeybindManager.hpp"

CKeybindManager::CKeybindManager() {
    // initialize all dispatchers

    m_mDispatchers["exec"]              = spawn;
    m_mDispatchers["killactive"]        = killActive;
    m_mDispatchers["togglefloating"]    = toggleActiveFloating;
    m_mDispatchers["workspace"]         = changeworkspace;
    m_mDispatchers["fullscreen"]        = fullscreenActive;
    m_mDispatchers["movetoworkspace"]   = moveActiveToWorkspace;
    m_mDispatchers["pseudo"]            = toggleActivePseudo;
    m_mDispatchers["movefocus"]         = moveFocusTo;
    m_mDispatchers["movewindow"]        = moveActiveTo;
    m_mDispatchers["togglegroup"]       = toggleGroup;
    m_mDispatchers["changegroupactive"] = changeGroupActive;
    m_mDispatchers["splitratio"]        = alterSplitRatio;
    m_mDispatchers["focusmonitor"]      = focusMonitor;
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
        const auto TTY = keysym - XKB_KEY_XF86Switch_VT_1 + 1;
        wlr_session_change_vt(PSESSION, TTY);
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

    if (args.find("name:") == 0) {
        const auto WORKSPACENAME = args.substr(args.find_first_of(':') + 1);
        const auto WORKSPACE = g_pCompositor->getWorkspaceByName(WORKSPACENAME);
        if (!WORKSPACE) {
            workspaceToChangeTo = g_pCompositor->getNextAvailableNamedWorkspace();
        } else {
            workspaceToChangeTo = WORKSPACE->m_iID;
        }
        workspaceName = WORKSPACENAME;
    } else {
        workspaceToChangeTo = std::clamp((int)getPlusMinusKeywordResult(args, g_pCompositor->m_pLastMonitor->activeWorkspace), 1, INT_MAX);
        workspaceName = std::to_string(workspaceToChangeTo);
    }

    if (workspaceToChangeTo == INT_MAX) {
        Debug::log(ERR, "Error in changeworkspace, invalid value");
        return;
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
        g_pCompositor->deactivateAllWLRWorkspaces(g_pCompositor->getWorkspaceByID(workspaceToChangeTo)->m_pWlrHandle);
        wlr_ext_workspace_handle_v1_set_active(g_pCompositor->getWorkspaceByID(workspaceToChangeTo)->m_pWlrHandle, true);

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

    g_pCompositor->m_lWorkspaces.emplace_back(PMONITOR->ID);
    const auto PWORKSPACE = &g_pCompositor->m_lWorkspaces.back();

    // We are required to set the name here immediately
    wlr_ext_workspace_handle_v1_set_name(PWORKSPACE->m_pWlrHandle, workspaceName.c_str());

    PWORKSPACE->m_iID = workspaceToChangeTo;
    PWORKSPACE->m_iMonitorID = PMONITOR->ID;
    PWORKSPACE->m_szName = workspaceName;
    
    PMONITOR->activeWorkspace = workspaceToChangeTo;

    // we need to move XWayland windows to narnia or otherwise they will still process our cursor and shit
    // and that'd be annoying as hell
    g_pCompositor->fixXWaylandWindowsOnWorkspace(OLDWORKSPACE);

    // set active and deactivate all other
    g_pCompositor->deactivateAllWLRWorkspaces(PWORKSPACE->m_pWlrHandle);
    wlr_ext_workspace_handle_v1_set_active(PWORKSPACE->m_pWlrHandle, true);

    // mark the monitor dirty
    g_pHyprRenderer->damageMonitor(PMONITOR);

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

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(PWINDOW);

    const auto OLDWORKSPACE = g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID);

    // hack
    g_pKeybindManager->changeworkspace(args);

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByString(args);

    if (PWORKSPACE == OLDWORKSPACE) {
        Debug::log(LOG, "Not moving to workspace because it didn't change.");
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
}

void CKeybindManager::moveFocusTo(std::string args) {
    char arg = args[0];

    if (!isDirection(args)) {
        Debug::log(ERR, "Cannot move focus in direction %c, unsupported direction. Supported: l,r,u/t,d/b", arg);
        return;
    }

    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow;

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
    g_pLayoutManager->getCurrentLayout()->toggleWindowGroup(g_pCompositor->m_pLastWindow);
}

void CKeybindManager::changeGroupActive(std::string args) {
    g_pLayoutManager->getCurrentLayout()->switchGroupWindow(g_pCompositor->m_pLastWindow);
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