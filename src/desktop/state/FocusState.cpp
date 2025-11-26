#include "FocusState.hpp"
#include "../Window.hpp"
#include "../../Compositor.hpp"
#include "../../protocols/XDGShell.hpp"
#include "../../render/Renderer.hpp"
#include "../../managers/LayoutManager.hpp"
#include "../../managers/EventManager.hpp"
#include "../../managers/HookSystemManager.hpp"
#include "../../xwayland/XSurface.hpp"
#include "../../protocols/PointerConstraints.hpp"

using namespace Desktop;

SP<CFocusState> Desktop::focusState() {
    static SP<CFocusState> state = makeShared<CFocusState>();
    return state;
}

Desktop::CFocusState::CFocusState() {
    m_windowOpen = g_pHookSystem->hookDynamic("openWindowEarly", [this](void* self, SCallbackInfo& info, std::any data) {
        auto window = std::any_cast<PHLWINDOW>(data);

        addWindowToHistory(window);
    });

    m_windowClose = g_pHookSystem->hookDynamic("closeWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        auto window = std::any_cast<PHLWINDOW>(data);

        removeWindowFromHistory(window);
    });
}

struct SFullscreenWorkspaceFocusResult {
    PHLWINDOW overrideFocusWindow = nullptr;
};

static SFullscreenWorkspaceFocusResult onFullscreenWorkspaceFocusWindow(PHLWINDOW pWindow, bool forceFSCycle) {
    const auto FSWINDOW = pWindow->m_workspace->getFullscreenWindow();
    const auto FSMODE   = pWindow->m_workspace->m_fullscreenMode;

    if (pWindow == FSWINDOW)
        return {}; // no conflict

    if (pWindow->m_isFloating) {
        // if the window is floating, just bring it to the top
        pWindow->m_createdOverFullscreen = true;
        g_pHyprRenderer->damageWindow(pWindow);
        return {};
    }

    static auto PONFOCUSUNDERFS = CConfigValue<Hyprlang::INT>("misc:on_focus_under_fullscreen");

    switch (*PONFOCUSUNDERFS) {
        case 0:
            // focus the fullscreen window instead
            return {.overrideFocusWindow = FSWINDOW};
        case 2:
            // undo fs, unless we force a cycle
            if (!forceFSCycle) {
                g_pCompositor->setWindowFullscreenInternal(FSWINDOW, FSMODE_NONE);
                break;
            }
            [[fallthrough]];
        case 1:
            // replace fullscreen
            g_pCompositor->setWindowFullscreenInternal(FSWINDOW, FSMODE_NONE);
            g_pCompositor->setWindowFullscreenInternal(pWindow, FSMODE);
            break;

        default: Debug::log(ERR, "Invalid misc:on_focus_under_fullscreen mode: {}", *PONFOCUSUNDERFS); break;
    }

    return {};
}

void CFocusState::fullWindowFocus(PHLWINDOW pWindow, SP<CWLSurfaceResource> surface, bool preserveFocusHistory, bool forceFSCycle) {
    if (pWindow) {
        if (!pWindow->m_workspace)
            return;

        const auto CURRENT_FS_MODE = pWindow->m_workspace->m_hasFullscreenWindow ? pWindow->m_workspace->m_fullscreenMode : FSMODE_NONE;
        if (CURRENT_FS_MODE != FSMODE_NONE) {
            const auto RESULT = onFullscreenWorkspaceFocusWindow(pWindow, forceFSCycle);
            if (RESULT.overrideFocusWindow)
                pWindow = RESULT.overrideFocusWindow;
        }
    }

    static auto PMODALPARENTBLOCKING = CConfigValue<Hyprlang::INT>("general:modal_parent_blocking");

    if (*PMODALPARENTBLOCKING && pWindow && pWindow->m_xdgSurface && pWindow->m_xdgSurface->m_toplevel && pWindow->m_xdgSurface->m_toplevel->anyChildModal()) {
        Debug::log(LOG, "Refusing focus to window shadowed by modal dialog");
        return;
    }

    rawWindowFocus(pWindow, surface, preserveFocusHistory);
}

void CFocusState::rawWindowFocus(PHLWINDOW pWindow, SP<CWLSurfaceResource> surface, bool preserveFocusHistory) {
    static auto PFOLLOWMOUSE        = CConfigValue<Hyprlang::INT>("input:follow_mouse");
    static auto PSPECIALFALLTHROUGH = CConfigValue<Hyprlang::INT>("input:special_fallthrough");

    if (!pWindow || !pWindow->priorityFocus()) {
        if (g_pSessionLockManager->isSessionLocked()) {
            Debug::log(LOG, "Refusing a keyboard focus to a window because of a sessionlock");
            return;
        }

        if (!g_pInputManager->m_exclusiveLSes.empty()) {
            Debug::log(LOG, "Refusing a keyboard focus to a window because of an exclusive ls");
            return;
        }
    }

    if (pWindow && pWindow->m_isX11 && pWindow->isX11OverrideRedirect() && !pWindow->m_xwaylandSurface->wantsFocus())
        return;

    g_pLayoutManager->getCurrentLayout()->bringWindowToTop(pWindow);

    if (!pWindow || !validMapped(pWindow)) {

        if (m_focusWindow.expired() && !pWindow)
            return;

        const auto PLASTWINDOW = m_focusWindow.lock();
        m_focusWindow.reset();

        if (PLASTWINDOW && PLASTWINDOW->m_isMapped) {
            PLASTWINDOW->m_ruleApplicator->propertiesChanged(Rule::RULE_PROP_FOCUS);
            PLASTWINDOW->updateDecorationValues();

            g_pXWaylandManager->activateWindow(PLASTWINDOW, false);
        }

        g_pSeatManager->setKeyboardFocus(nullptr);

        g_pEventManager->postEvent(SHyprIPCEvent{"activewindow", ","});
        g_pEventManager->postEvent(SHyprIPCEvent{"activewindowv2", ""});

        EMIT_HOOK_EVENT("activeWindow", PHLWINDOW{nullptr});

        g_pLayoutManager->getCurrentLayout()->onWindowFocusChange(nullptr);

        m_focusSurface.reset();

        g_pInputManager->recheckIdleInhibitorStatus();
        return;
    }

    if (pWindow->m_ruleApplicator->noFocus().valueOrDefault()) {
        Debug::log(LOG, "Ignoring focus to nofocus window!");
        return;
    }

    if (m_focusWindow.lock() == pWindow && g_pSeatManager->m_state.keyboardFocus == surface && g_pSeatManager->m_state.keyboardFocus)
        return;

    if (pWindow->m_pinned)
        pWindow->m_workspace = m_focusMonitor->m_activeWorkspace;

    const auto PMONITOR = pWindow->m_monitor.lock();

    if (!pWindow->m_workspace || !pWindow->m_workspace->isVisible()) {
        const auto PWORKSPACE = pWindow->m_workspace;
        // This is to fix incorrect feedback on the focus history.
        PWORKSPACE->m_lastFocusedWindow = pWindow;
        if (m_focusMonitor->m_activeWorkspace)
            PWORKSPACE->rememberPrevWorkspace(m_focusMonitor->m_activeWorkspace);
        if (PWORKSPACE->m_isSpecialWorkspace)
            m_focusMonitor->changeWorkspace(PWORKSPACE, false, true); // if special ws, open on current monitor
        else if (PMONITOR)
            PMONITOR->changeWorkspace(PWORKSPACE, false, true);
        // changeworkspace already calls focusWindow
        return;
    }

    const auto PLASTWINDOW = m_focusWindow.lock();
    m_focusWindow          = pWindow;

    /* If special fallthrough is enabled, this behavior will be disabled, as I have no better idea of nicely tracking which
       window focuses are "via keybinds" and which ones aren't. */
    if (PMONITOR && PMONITOR->m_activeSpecialWorkspace && PMONITOR->m_activeSpecialWorkspace != pWindow->m_workspace && !pWindow->m_pinned && !*PSPECIALFALLTHROUGH)
        PMONITOR->setSpecialWorkspace(nullptr);

    // we need to make the PLASTWINDOW not equal to m_pLastWindow so that RENDERDATA is correct for an unfocused window
    if (PLASTWINDOW && PLASTWINDOW->m_isMapped) {
        PLASTWINDOW->m_ruleApplicator->propertiesChanged(Rule::RULE_PROP_FOCUS);
        PLASTWINDOW->updateDecorationValues();

        if (!pWindow->m_isX11 || !pWindow->isX11OverrideRedirect())
            g_pXWaylandManager->activateWindow(PLASTWINDOW, false);
    }

    const auto PWINDOWSURFACE = surface ? surface : pWindow->m_wlSurface->resource();

    rawSurfaceFocus(PWINDOWSURFACE, pWindow);

    g_pXWaylandManager->activateWindow(pWindow, true); // sets the m_pLastWindow

    pWindow->m_ruleApplicator->propertiesChanged(Rule::RULE_PROP_FOCUS);
    pWindow->onFocusAnimUpdate();
    pWindow->updateDecorationValues();

    if (pWindow->m_isUrgent)
        pWindow->m_isUrgent = false;

    // Send an event
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "activewindow", .data = pWindow->m_class + "," + pWindow->m_title});
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "activewindowv2", .data = std::format("{:x}", rc<uintptr_t>(pWindow.get()))});

    EMIT_HOOK_EVENT("activeWindow", pWindow);

    g_pLayoutManager->getCurrentLayout()->onWindowFocusChange(pWindow);

    g_pInputManager->recheckIdleInhibitorStatus();

    if (!preserveFocusHistory) {
        // move to front of the window history
        moveWindowToLatestInHistory(pWindow);
    }

    if (*PFOLLOWMOUSE == 0)
        g_pInputManager->sendMotionEventsToFocused();

    if (pWindow->m_groupData.pNextWindow)
        pWindow->deactivateGroupMembers();
}

void CFocusState::rawSurfaceFocus(SP<CWLSurfaceResource> pSurface, PHLWINDOW pWindowOwner) {
    if (g_pSeatManager->m_state.keyboardFocus == pSurface || (pWindowOwner && g_pSeatManager->m_state.keyboardFocus == pWindowOwner->m_wlSurface->resource()))
        return; // Don't focus when already focused on this.

    if (g_pSessionLockManager->isSessionLocked() && pSurface && !g_pSessionLockManager->isSurfaceSessionLock(pSurface))
        return;

    if (g_pSeatManager->m_seatGrab && !g_pSeatManager->m_seatGrab->accepts(pSurface)) {
        Debug::log(LOG, "surface {:x} won't receive kb focus because grab rejected it", rc<uintptr_t>(pSurface.get()));
        return;
    }

    const auto PLASTSURF = m_focusSurface.lock();

    // Unfocus last surface if should
    if (m_focusSurface && !pWindowOwner)
        g_pXWaylandManager->activateSurface(m_focusSurface.lock(), false);

    if (!pSurface) {
        g_pSeatManager->setKeyboardFocus(nullptr);
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "activewindow", .data = ","});
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "activewindowv2", .data = ""});
        EMIT_HOOK_EVENT("keyboardFocus", SP<CWLSurfaceResource>{nullptr});
        m_focusSurface.reset();
        return;
    }

    if (g_pSeatManager->m_keyboard)
        g_pSeatManager->setKeyboardFocus(pSurface);

    if (pWindowOwner)
        Debug::log(LOG, "Set keyboard focus to surface {:x}, with {}", rc<uintptr_t>(pSurface.get()), pWindowOwner);
    else
        Debug::log(LOG, "Set keyboard focus to surface {:x}", rc<uintptr_t>(pSurface.get()));

    g_pXWaylandManager->activateSurface(pSurface, true);
    m_focusSurface = pSurface;

    EMIT_HOOK_EVENT("keyboardFocus", pSurface);

    const auto SURF    = CWLSurface::fromResource(pSurface);
    const auto OLDSURF = CWLSurface::fromResource(PLASTSURF);

    if (OLDSURF && OLDSURF->constraint())
        OLDSURF->constraint()->deactivate();

    if (SURF && SURF->constraint())
        SURF->constraint()->activate();
}

void CFocusState::rawMonitorFocus(PHLMONITOR pMonitor) {
    if (m_focusMonitor == pMonitor)
        return;

    if (!pMonitor) {
        m_focusMonitor.reset();
        return;
    }

    const auto PWORKSPACE = pMonitor->m_activeWorkspace;

    const auto WORKSPACE_ID   = PWORKSPACE ? std::to_string(PWORKSPACE->m_id) : std::to_string(WORKSPACE_INVALID);
    const auto WORKSPACE_NAME = PWORKSPACE ? PWORKSPACE->m_name : "?";

    g_pEventManager->postEvent(SHyprIPCEvent{.event = "focusedmon", .data = pMonitor->m_name + "," + WORKSPACE_NAME});
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "focusedmonv2", .data = pMonitor->m_name + "," + WORKSPACE_ID});

    EMIT_HOOK_EVENT("focusedMon", pMonitor);
    m_focusMonitor = pMonitor;
}

SP<CWLSurfaceResource> CFocusState::surface() {
    return m_focusSurface.lock();
}

PHLWINDOW CFocusState::window() {
    return m_focusWindow.lock();
}

PHLMONITOR CFocusState::monitor() {
    return m_focusMonitor.lock();
}

const std::vector<PHLWINDOWREF>& CFocusState::windowHistory() {
    return m_windowFocusHistory;
}

void CFocusState::removeWindowFromHistory(PHLWINDOW w) {
    std::erase_if(m_windowFocusHistory, [&w](const auto& e) { return !e || e == w; });
}

void CFocusState::addWindowToHistory(PHLWINDOW w) {
    m_windowFocusHistory.emplace_back(w);
}

void CFocusState::moveWindowToLatestInHistory(PHLWINDOW w) {
    const auto HISTORYPIVOT = std::ranges::find_if(m_windowFocusHistory, [&w](const auto& other) { return other.lock() == w; });
    if (HISTORYPIVOT == m_windowFocusHistory.end())
        Debug::log(TRACE, "CFocusState: {} has no pivot in history, ignoring request to move to latest", w);
    else
        std::rotate(m_windowFocusHistory.begin(), HISTORYPIVOT, HISTORYPIVOT + 1);
}
