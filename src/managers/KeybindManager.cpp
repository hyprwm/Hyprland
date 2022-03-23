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
    for (auto& k : m_dKeybinds) {
        if (modmask != k.modmask) 
            continue;

        // oMg such performance hit!!11!
        // this little maneouver is gonna cost us 4µs
        const auto KBKEY = xkb_keysym_from_name(k.key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
        const auto KBKEYUPPER = xkb_keysym_to_upper(KBKEY);
        
        if (key != KBKEY && key != KBKEYUPPER)
            continue;

        // yes.
        if (k.handler == "exec") { spawn(k.arg); }
        else if (k.handler == "killactive") { killActive(k.arg); }
        else if (k.handler == "togglefloating") { toggleActiveFloating(k.arg); }
        else if (k.handler == "workspace") { changeworkspace(k.arg); }
        else if (k.handler == "fullscreen") { fullscreenActive(k.arg); }
        else if (k.handler == "movetoworkspace") { moveActiveToWorkspace(k.arg); }

        found = true;
    }

    return found;
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
    if (g_pCompositor->m_pLastFocus && g_pCompositor->windowValidMapped(g_pCompositor->getWindowFromSurface(g_pCompositor->m_pLastFocus)))
        g_pXWaylandManager->sendCloseWindow(g_pCompositor->getWindowFromSurface(g_pCompositor->m_pLastFocus));

    g_pCompositor->focusWindow(g_pCompositor->windowFromCursor());
}

void CKeybindManager::clearKeybinds() {
    m_dKeybinds.clear();
}

void CKeybindManager::toggleActiveFloating(std::string args) {
    const auto ACTIVEWINDOW = g_pCompositor->getWindowFromSurface(g_pCompositor->m_pLastFocus);

    if (g_pCompositor->windowValidMapped(ACTIVEWINDOW)) {
        ACTIVEWINDOW->m_bIsFloating = !ACTIVEWINDOW->m_bIsFloating;

        ACTIVEWINDOW->m_vRealPosition = ACTIVEWINDOW->m_vRealPosition + Vector2D(5, 5);
        ACTIVEWINDOW->m_vSize = ACTIVEWINDOW->m_vRealPosition - Vector2D(10, 10);

        g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(ACTIVEWINDOW);
    }
}

void CKeybindManager::changeworkspace(std::string args) {
    int workspaceToChangeTo = 0;
    try {
        workspaceToChangeTo = stoi(args);
    } catch (...) {
        Debug::log(ERR, "Invalid arg \"%s\" in changeWorkspace!", args.c_str());
    }

    // if it exists, we warp to it
    if (g_pCompositor->getWorkspaceByID(workspaceToChangeTo)) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(g_pCompositor->getWorkspaceByID(workspaceToChangeTo)->monitorID);

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

        return;
    }

    // Workspace doesn't exist, create and switch
    const auto PMONITOR = g_pCompositor->getMonitorFromCursor();

    const auto OLDWORKSPACE = PMONITOR->activeWorkspace;

    g_pCompositor->m_lWorkspaces.push_back(SWorkspace());
    const auto PWORKSPACE = &g_pCompositor->m_lWorkspaces.back();

    PWORKSPACE->ID = workspaceToChangeTo;
    PWORKSPACE->monitorID = PMONITOR->ID;
    
    PMONITOR->activeWorkspace = workspaceToChangeTo;

    // we need to move XWayland windows to narnia or otherwise they will still process our cursor and shit
    // and that'd be annoying as hell
    g_pCompositor->fixXWaylandWindowsOnWorkspace(OLDWORKSPACE);
}

void CKeybindManager::fullscreenActive(std::string args) {
    const auto PWINDOW = g_pCompositor->getWindowFromSurface(g_pCompositor->m_pLastFocus);

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    g_pLayoutManager->getCurrentLayout()->fullscreenRequestForWindow(PWINDOW);
}

void CKeybindManager::moveActiveToWorkspace(std::string args) {
    const auto PWINDOW = g_pCompositor->getWindowFromSurface(g_pCompositor->m_pLastFocus);

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

    OLDWORKSPACE->hasFullscreenWindow = false;

    PWINDOW->m_iWorkspaceID = workspaceID;
    PWINDOW->m_iMonitorID = NEWWORKSPACE->monitorID;
    PWINDOW->m_bIsFullscreen = false;

    if (NEWWORKSPACE->hasFullscreenWindow) {
        g_pCompositor->getFullscreenWindowOnWorkspace(workspaceID)->m_bIsFullscreen = false;
        NEWWORKSPACE->hasFullscreenWindow = false;
    }

    // Hack: So that the layout doesnt find our window at the cursor
    PWINDOW->m_vPosition = Vector2D(-42069, -42069);
    g_pLayoutManager->getCurrentLayout()->onWindowCreated(PWINDOW);

    if (PWINDOW->m_bIsFloating) {
        PWINDOW->m_vRealPosition = PWINDOW->m_vRealPosition - g_pCompositor->getMonitorFromID(OLDWORKSPACE->monitorID)->vecPosition;
        PWINDOW->m_vRealPosition = PWINDOW->m_vRealPosition + g_pCompositor->getMonitorFromID(NEWWORKSPACE->monitorID)->vecPosition;
    }
}