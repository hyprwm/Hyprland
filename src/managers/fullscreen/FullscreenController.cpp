#include "FullscreenController.hpp"

#include "../../managers/fullscreen/handler/FullscreenHandler.hpp"
#include "../../managers/EventManager.hpp"

#include "../../layout/algorithm/Algorithm.hpp"
#include "../../layout/algorithm/FloatingAlgorithm.hpp"
#include "../../layout/algorithm/TiledAlgorithm.hpp"
#include "../../layout/LayoutManager.hpp"
#include "../../layout/target/Target.hpp"

#include "../../desktop/DesktopTypes.hpp"
#include "../../desktop/view/Window.hpp"

#include "../../event/EventBus.hpp"
#include "../../output/Monitor.hpp"
#include "../../render/Renderer.hpp"
#include "../../debug/log/Logger.hpp"
#include <optional>

using namespace Fullscreen;

UP<CFullscreenController>& Fullscreen::controller() {
    static UP<CFullscreenController> p = makeUnique<CFullscreenController>();
    return p;
}

bool CFullscreenController::isFullscreen(const PHLWINDOW window, const std::optional<eFullscreenMode> mode, const std::optional<bool> covering) {
    if (!window)
        return false;

    if (mode.value_or(FSMODE_FULLSCREEN) == FSMODE_NONE) {
        Log::logger->log(Log::ERR, "Passed mode = FSMODE_NONE into isFullscreen(). Negating the result instead");
        !isFullscreen(window, std::nullopt, covering);
    }

    /* Error Correction - try once */
    const auto returnBoolAfterErrorCorrection = [&](const WP<Fullscreen::IFullscreenHandler> FS_HANDLER, const WP<Desktop::View::CWindow> FS_WINDOW) -> bool {
        if (!FS_WINDOW || !FS_WINDOW->m_target)
            return false;

        if (FS_WINDOW == FS_HANDLER->getFullscreen(covering)->window() && FS_HANDLER->getFullscreenModes(window->m_target).internal == mode)
            return true;
        else {
            FS_HANDLER->syncFullscreenTargets();
            return FS_HANDLER->isFullscreen(window->m_target, mode, covering);
        }
        return false;
    };

    const auto FS_HANDLER = getFsHandler(window);

    if (!FS_HANDLER) {
        Log::logger->log(Log::ERR, "window {} doesn't have FS handler assinged. This should never happen", window->m_title);
        return false;
    }

    const auto IS_FS = FS_HANDLER->isFullscreen(window->m_target, mode, covering);

    if (IS_FS)
        return returnBoolAfterErrorCorrection(FS_HANDLER, window);

    return false;
}

SFullscreenMode CFullscreenController::getFullscreenModes(const PHLWINDOW window) {
    if (!window)
        return {};

    const auto FS_HANDLER = getFsHandler(window);

    if (!FS_HANDLER)
        return {};

    auto fsModes = FS_HANDLER->getFullscreenModes(window->m_target);

    /* Error correction - try once*/
    if (fsModes.internal != FSMODE_NONE && !FS_HANDLER->isFullscreen(window->m_target)) {
        FS_HANDLER->syncFullscreenTargets();
        return FS_HANDLER->getFullscreenModes(window->m_target);
    }

    return fsModes;
}

bool CFullscreenController::layoutManagedFS(const PHLWINDOW window) {
    if (!window)
        return false;

    const auto FS_HANDLER_NAME = getFullscreenHandlerName(window);

    if (FS_HANDLER_NAME == FULLSCREEN_HANDLER_NONE) {
        Log::logger->log(Log::ERR, "window {} doesn't have FS handler assinged. This should never happen", window->m_title);
        return false;
    }

    // If a window is not FS at all, we consider its handler to be layout if it is in a workspace with a layout that implements their custom FS behaviour.
    return FS_HANDLER_NAME & FULLSCREEN_HANDLER_LAYOUT;
}

bool CFullscreenController::hasFullscreen(const PHLWORKSPACE workspace, const std::optional<bool> covering) {
    if (!workspace)
        return false;

    // ASSUMPTION: Floating FS window layers ontop of Tiled Default Handled FS window which layers ontop of Tiled Layout Handled FS window

    /* Error Correction - try once */
    const auto returnBoolAfterErrorCorrection = [&](const WP<Fullscreen::IFullscreenHandler> FS_HANDLER) -> bool {
        if (FS_HANDLER->isFullscreen(FS_HANDLER->getFullscreen(covering), std::nullopt, covering))
            return true;
        else {
            FS_HANDLER->syncFullscreenTargets();
            return FS_HANDLER->hasFullscreen(covering);
        }
        return false;
    };

    const auto HANDLERS = getFsHandlersForWorkspace(workspace);

    if (!HANDLERS.TILED_FS_HANDLER || !HANDLERS.TILED_DEFAULT_FS_HANDLER || !HANDLERS.FLOATING_FS_HANDLER)
        return false;

    if (HANDLERS.FLOATING_FS_HANDLER->hasFullscreen(covering))
        return returnBoolAfterErrorCorrection(HANDLERS.FLOATING_FS_HANDLER);
    if (HANDLERS.TILED_DEFAULT_FS_HANDLER->hasFullscreen(covering))
        return returnBoolAfterErrorCorrection(HANDLERS.TILED_DEFAULT_FS_HANDLER);

    if (HANDLERS.TILED_FS_HANDLER->hasFullscreen(covering))
        return returnBoolAfterErrorCorrection(HANDLERS.TILED_FS_HANDLER);

    return false;
}

PHLWINDOW CFullscreenController::getFullscreenWindow(const PHLWORKSPACE workspace, const std::optional<bool> covering) {
    if (!workspace)
        return nullptr;

    // ASSUMPTION: Floating FS window layers ontop of Tiled Default Handled FS window which layers ontop of Tiled Layout Handled FS window

    /* Error Correction - try once */
    const auto returnWindowAfterErrorCorrection = [&](WP<Fullscreen::IFullscreenHandler> FS_HANDLER, SP<Layout::ITarget> FSTARGET) -> PHLWINDOW {
        if (!FSTARGET)
            return nullptr;

        if (FS_HANDLER->isFullscreen(FSTARGET, std::nullopt, covering))
            return FSTARGET->window();
        else {
            FS_HANDLER->syncFullscreenTargets();
            const auto FS_TARGET_POST_RECOVERY = FS_HANDLER->getFullscreen(covering);
            return FS_TARGET_POST_RECOVERY ? FS_TARGET_POST_RECOVERY->window() : nullptr;
        }
        return nullptr;
    };

    const auto HANDLERS = getFsHandlersForWorkspace(workspace);

    if (!HANDLERS.TILED_FS_HANDLER || !HANDLERS.TILED_DEFAULT_FS_HANDLER || !HANDLERS.FLOATING_FS_HANDLER)
        return nullptr;

    if (const auto FSTARGET = HANDLERS.FLOATING_FS_HANDLER->getFullscreen(covering); FSTARGET)
        return returnWindowAfterErrorCorrection(HANDLERS.FLOATING_FS_HANDLER, FSTARGET);
    if (const auto FSTARGET = HANDLERS.TILED_DEFAULT_FS_HANDLER->getFullscreen(covering); FSTARGET)
        return returnWindowAfterErrorCorrection(HANDLERS.TILED_DEFAULT_FS_HANDLER, FSTARGET);

    const auto FSTARGET = HANDLERS.TILED_FS_HANDLER->getFullscreen(covering);
    return returnWindowAfterErrorCorrection(HANDLERS.TILED_FS_HANDLER, FSTARGET);
}

SFullscreenMode CFullscreenController::getFullscreenModes(const PHLWORKSPACE workspace, const std::optional<bool> covering) {
    if (!workspace)
        return {};

    // ASSUMPTION: Floating FS window layers ontop of Tiled Default Handled FS window which layers ontop of Tiled Layout Handled FS window

    /* Error Correction - try once */
    const auto returnModesAfterErrorCorrection = [&](WP<Fullscreen::IFullscreenHandler> FS_HANDLER, SP<Layout::ITarget> FSTARGET) -> SFullscreenMode {
        if (!FSTARGET)
            return {};

        auto fsModes = FS_HANDLER->getFullscreenModes(FSTARGET);

        if (fsModes.internal != FSMODE_NONE && !FS_HANDLER->isFullscreen(FSTARGET, std::nullopt, covering)) {
            FS_HANDLER->syncFullscreenTargets();
            return FS_HANDLER->getFullscreenModes(FSTARGET);
        } else
            return fsModes;

        return {};
    };

    const auto HANDLERS = getFsHandlersForWorkspace(workspace);

    if (!HANDLERS.TILED_FS_HANDLER || !HANDLERS.TILED_DEFAULT_FS_HANDLER || !HANDLERS.FLOATING_FS_HANDLER)
        return {};

    if (const auto FSTARGET = HANDLERS.FLOATING_FS_HANDLER->getFullscreen(covering); FSTARGET)
        return returnModesAfterErrorCorrection(HANDLERS.FLOATING_FS_HANDLER, FSTARGET);

    if (const auto FSTARGET = HANDLERS.TILED_DEFAULT_FS_HANDLER->getFullscreen(covering); FSTARGET)
        return returnModesAfterErrorCorrection(HANDLERS.TILED_DEFAULT_FS_HANDLER, FSTARGET);

    const auto FSTARGET = HANDLERS.TILED_FS_HANDLER->getFullscreen(covering);
    return returnModesAfterErrorCorrection(HANDLERS.TILED_FS_HANDLER, FSTARGET);
}

bool CFullscreenController::hasFullscreen(const PHLMONITOR monitor, const std::optional<bool> covering) {
    if (!monitor)
        return false;

    PHLWORKSPACE activeWorkspace = monitor->getCurrentWorkspace();
    if (!activeWorkspace)
        return false;

    return hasFullscreen(activeWorkspace) && getFullscreenModes(activeWorkspace).internal == FSMODE_FULLSCREEN;
}
PHLWINDOW CFullscreenController::getFullscreenWindow(const PHLMONITOR monitor, const std::optional<bool> covering) {
    if (!monitor)
        return nullptr;

    PHLWORKSPACE activeWorkspace = monitor->getCurrentWorkspace();
    if (!activeWorkspace)
        return nullptr;

    const auto FS_WINDOW = getFullscreenWindow(activeWorkspace, covering);

    return getFullscreenModes(FS_WINDOW).internal == FSMODE_FULLSCREEN ? getFullscreenWindow(activeWorkspace, covering) : nullptr;
}

SFullscreenMode CFullscreenController::getFullscreenModes(const PHLMONITOR monitor, const std::optional<bool> covering) {
    if (!monitor)
        return {};

    PHLWORKSPACE activeWorkspace = monitor->getCurrentWorkspace();
    if (!activeWorkspace)
        return {};

    return getFullscreenModes(activeWorkspace);
}

eFullscreenHandler CFullscreenController::getFullscreenHandlerName(const PHLWINDOW window) {
    if (!window)
        return FULLSCREEN_HANDLER_NONE;

    // IMPORTANT: no layoutHandled value passed -> infinite recursion.
    const auto LAYOUT_FS_HANDLER = getFsHandler(window, true);
    // IMPORTANT: no layoutHandled value passed -> infinite recursion.
    const auto DEFAULT_FS_HANDLER = getFsHandler(window, false);

    if (!LAYOUT_FS_HANDLER || !DEFAULT_FS_HANDLER) {
        Log::logger->log(Log::ERR, "window {} doesn't have FS handler assinged. This should never happen", window->m_title);
        return FULLSCREEN_HANDLER_NONE;
    }

    eFullscreenHandler handlerName = FULLSCREEN_HANDLER_NONE;

    if (DEFAULT_FS_HANDLER->isFullscreen(window->m_target) || DEFAULT_FS_HANDLER->getFullscreenModes(window->m_target).client != FSMODE_NONE)
        handlerName = DEFAULT_FS_HANDLER->getFullscreenHandlerName();
    else
        handlerName = LAYOUT_FS_HANDLER->getFullscreenHandlerName();

    if (handlerName == FULLSCREEN_HANDLER_NONE) {
        Log::logger->log(Log::ERR, "window {} doesn't have FS handler assinged. This should never happen", window->m_title);
        return FULLSCREEN_HANDLER_NONE;
    }

    return handlerName;
}

std::string CFullscreenController::getFullscreenHandlerNameAsString(const PHLWINDOW window) {
    if (!window)
        return "unknown";

    const auto FS_HANDLER_NAME = getFullscreenHandlerName(window);

    switch (FS_HANDLER_NAME) {
        case FULLSCREEN_HANDLER_DEFAULT: return "default";
        case FULLSCREEN_HANDLER_SCROLLING: return "scrolling";
        default: return "unknown";
    }
}

void CFullscreenController::setFullscreenMode(const PHLWINDOW window, std::optional<eFullscreenMode> internal, std::optional<eFullscreenMode> client,
                                              std::optional<bool> layoutAware) {
    if (!window)
        return;

    const bool WANT_SYNC = window->m_ruleApplicator->syncFullscreen().valueOrDefault();

    bool       stateChanged = false;

    if (internal.has_value())
        internal = std::clamp(internal.value(), sc<eFullscreenMode>(0), FSMODE_FULLSCREEN);
    if (client.has_value())
        client = std::clamp(client.value(), sc<eFullscreenMode>(0), FSMODE_FULLSCREEN);

    eFullscreenMode targetInternalMode = internal.value_or(FSMODE_NONE);
    eFullscreenMode targetClientMode   = client.value_or(FSMODE_NONE);

    /*
        If the past handled mode and current handled mode is not the same for an already FS window, it implies that the window IS FS; we need to move it to the new handler we will use for the current FS request
    */
    const bool WAS_LAYOUT_HANDLED = layoutManagedFS(window);

    const auto ORIGINAL_FS_HANDLER = getFsHandler(window, WAS_LAYOUT_HANDLED);
    const auto OLD_FS_MODES        = ORIGINAL_FS_HANDLER->getFullscreenModes(window->m_target);

    const auto TO_BE_USED_FS_HANDLER = getFsHandler(window, layoutAware.value_or(WAS_LAYOUT_HANDLED));
    if (!TO_BE_USED_FS_HANDLER) {
        Log::logger->log(Log::ERR, "window {} doesn't have FS handler assinged. This should never happen", window->m_title);
        return;
    }

    // handles FSMODE_MAX case. If nothing to handle on that front, saves the provided old values
    const auto saveClientInternalValues = [&](const SFullscreenMode& OLD_FS_MODES) {
        if (client.value_or(FSMODE_NONE) == FSMODE_FULLSCREEN && OLD_FS_MODES.internal == FSMODE_MAXIMIZED)
            m_fsModeMaxWindows.emplace(window);
        else if (const auto IT = m_fsModeMaxWindows.find(window);
                   client.value_or(FSMODE_MAXIMIZED) == FSMODE_NONE && (IT != m_fsModeMaxWindows.end() && IT->valid() && !IT->expired())) {
            targetClientMode = FSMODE_MAXIMIZED;
            m_fsModeMaxWindows.erase(IT);
        }
        else {
            targetInternalMode = internal.value_or(OLD_FS_MODES.internal);
            targetClientMode   = client.value_or(OLD_FS_MODES.client);
        }
    };

    // Maintenence on FSMODE_MAX list
    const auto syncFsModeMaxWindows = [&]() {
        for (auto it = m_fsModeMaxWindows.begin(); it != m_fsModeMaxWindows.end();) {

            // Somehow happens sometimes and causes WP<> to segfault
            if (m_fsModeMaxWindows.empty())
                return;

            if (!it->valid() || it->expired() || getFullscreenModes(it->lock()).internal != FSMODE_FULLSCREEN) {
                const auto NEXT = std::next(it);
                m_fsModeMaxWindows.erase(it);
                it = NEXT;
                continue;
            }
            ++it;
        }
    };

    /*
        Handle moving the window from one handler to another if needed
    */
    // If window is FS and is handled differently than before, this implies that the window is already fullscreen (as a window's non-FS state defaults to 'layout handled')
    if (layoutAware.value_or(WAS_LAYOUT_HANDLED) != WAS_LAYOUT_HANDLED) {

        stateChanged = true;

        saveClientInternalValues(OLD_FS_MODES);

        /* Remove Window from Old handler */

        if (WANT_SYNC) {
            setWindowFullscreenModeClient(window, FSMODE_NONE, WAS_LAYOUT_HANDLED);
            setWindowFullscreenModeInternal(window, FSMODE_NONE, WAS_LAYOUT_HANDLED);
        } else {
            if (OLD_FS_MODES.internal != FSMODE_NONE)
                setWindowFullscreenModeInternal(window, FSMODE_NONE, WAS_LAYOUT_HANDLED);
            if (OLD_FS_MODES.client != FSMODE_NONE)
                setWindowFullscreenModeClient(window, FSMODE_NONE, WAS_LAYOUT_HANDLED);
        }

    }
    // if window is FS and it's handled the same as before OR it's not FS at all
    else {

        const auto OLD_FS_MODES = TO_BE_USED_FS_HANDLER->getFullscreenModes(window->m_target);
        if (OLD_FS_MODES.internal != internal.value_or(OLD_FS_MODES.internal) || OLD_FS_MODES.client != client.value_or(OLD_FS_MODES.client))
            stateChanged = true;

        saveClientInternalValues(OLD_FS_MODES);
    }

    if (WANT_SYNC) {

        if (targetInternalMode != targetClientMode)
            stateChanged = true;

        if (internal.has_value() && !client.has_value())
            targetClientMode = targetInternalMode;
        else
            targetInternalMode = targetClientMode;
    }

    /*
        Handling Pinned windows - allow_pin_fullscreen
        Pinned windows can only be floating, therefore it is guaranteed that they will use the same FS handler within the workspace
    */
    const bool          WINDOW_IS_ALREADY_INTERNAL_FS_HANDLER_AGNOSTIC = OLD_FS_MODES.internal != FSMODE_NONE;
    const bool          HANDLE_PINNED_WINDOW                           = window->m_pinned || window->m_pinFullscreened;
    std::optional<bool> pinnedWindowRequetsInternalFS                  = std::nullopt;

    static auto         PALLOWPINFULLSCREEN = CConfigValue<Config::INTEGER>("binds:allow_pin_fullscreen");
    if (*PALLOWPINFULLSCREEN && !window->m_pinFullscreened && window->m_pinned && !WINDOW_IS_ALREADY_INTERNAL_FS_HANDLER_AGNOSTIC)
        pinnedWindowRequetsInternalFS = true;
    if (*PALLOWPINFULLSCREEN && window->m_pinFullscreened && WINDOW_IS_ALREADY_INTERNAL_FS_HANDLER_AGNOSTIC && !window->m_pinned && targetInternalMode == FSMODE_NONE)
        pinnedWindowRequetsInternalFS = false;

    if (HANDLE_PINNED_WINDOW) {

        if (pinnedWindowRequetsInternalFS.value_or(false)) {
            window->m_pinned          = false;
            window->m_pinFullscreened = true;
        } else if (!pinnedWindowRequetsInternalFS.value_or(true)) {
            window->m_pinned          = true;
            window->m_pinFullscreened = false;
        } else if (!(*PALLOWPINFULLSCREEN)) {
            if (WANT_SYNC)
                stateChanged = false;
            else
                targetInternalMode = FSMODE_NONE;
        }
    }

    if (stateChanged) {
        setWindowFullscreenModeClient(window, targetClientMode, layoutAware.value_or(WAS_LAYOUT_HANDLED));
        setWindowFullscreenModeInternal(window, targetInternalMode, layoutAware.value_or(WAS_LAYOUT_HANDLED));
    }
    syncFsModeMaxWindows();
}

void CFullscreenController::setWindowFullscreenModeInternal(const PHLWINDOW window, const eFullscreenMode mode, bool layoutAware) {

    if (!window || !validMapped(window) || !window->m_monitor || !window->m_workspace || !window->m_workspace->m_space || !window->m_workspace->m_space->algorithm())
        return;

    const auto            MONITOR   = window->m_monitor.lock();
    const auto            WORKSPACE = window->m_workspace;

    const auto            SPACE     = window->m_workspace->m_space;
    const auto            ALGORITHM = window->m_workspace->m_space->algorithm();

    const auto            WINDOW_FS_HANDLER        = getFsHandler(window, layoutAware);
    const SFullscreenMode WINDOW_FS_MODE           = getFullscreenModes(window);
    const bool            INTERNAL_FS_MODE_CHANGED = WINDOW_FS_MODE.internal != mode;

    if (!WINDOW_FS_HANDLER) {
        Log::logger->log(Log::ERR, "window {} doesn't have FS handler assinged. This should never happen", window->m_title);
        return;
    }

    static auto PDIRECTSCANOUT = CConfigValue<Config::INTEGER>("render:direct_scanout");

    if (window->m_isFloating && WINDOW_FS_MODE.internal == FSMODE_NONE && mode != FSMODE_NONE)
        g_pHyprRenderer->damageWindow(window);

    if (hasFullscreen(WORKSPACE) && !isFullscreen(window) && !layoutAware) {

        // Layout FS handling allows for layering a floating FS winow ontop of a tiled one. Default does not.
        const auto COVERING_FS_WINDOW = Fullscreen::controller()->getFullscreenWindow(WORKSPACE, true);
        if (!Fullscreen::controller()->layoutManagedFS(COVERING_FS_WINDOW))
            setFullscreenMode(COVERING_FS_WINDOW, FSMODE_NONE);
    }

    // arm m_suppressNextMaximize to swallow the set_maximized echo on fullscreen exit
    if (INTERNAL_FS_MODE_CHANGED && !window->m_isFloating && (getFullscreenModes(window).internal == FSMODE_FULLSCREEN) && mode != FSMODE_FULLSCREEN)
        window->m_suppressNextMaximize = true;

    // Window/Workspace Rules, decorations, etc..
    if (!INTERNAL_FS_MODE_CHANGED) {
        window->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_FULLSCREEN | Desktop::Rule::RULE_PROP_FULLSCREENSTATE_CLIENT |
                                                    Desktop::Rule::RULE_PROP_FULLSCREENSTATE_INTERNAL | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
        window->updateDecorationValues();
        g_layoutManager->recalculateMonitor(MONITOR, Layout::CLayoutManager::RECALCULATE_MONITOR_REASON_TOGGLE_FULLSCREEN);
        // Need to explicitly call as workspace may not be the currently focused one on the monitor (e.g. moving FS window between workspaces)
        WORKSPACE->m_space->recalculate(layoutAware ? Layout::RECALCULATE_REASON_TOGGLE_LAYOUT_HANDLED_FULLSCREEN : Layout::RECALCULATE_REASON_TOGGLE_DEFAULT_HANDLED_FULLSCREEN);
        return;
    }

    // Internal mode must be set by the handlers, not set here because last FS mode should be made available
    const eFullscreenRequestResult FULLSCREEN_REQUEST_RESULT =
        WINDOW_FS_HANDLER->requestFullscreen({.target = window->m_target, .currentMode = WINDOW_FS_MODE.internal, .mode = mode});

    if (mode == FSMODE_NONE && window->m_isFloating)
        // If window group, use the group target to set all member windows
        ALGORITHM->recenter(window->layoutTarget());

    SPACE->recalculate(FULLSCREEN_REQUEST_RESULT == FULLSCREEN_REQUEST_DEFAULT_HANDLED ? Layout::RECALCULATE_REASON_TOGGLE_DEFAULT_HANDLED_FULLSCREEN :
                                                                                         Layout::RECALCULATE_REASON_TOGGLE_LAYOUT_HANDLED_FULLSCREEN);

    g_pEventManager->postEvent(SHyprIPCEvent{.event = "fullscreen", .data = std::to_string(sc<int>(mode) != FSMODE_NONE)});
    Event::bus()->m_events.window.fullscreen.emit(window);

    window->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_FULLSCREEN | Desktop::Rule::RULE_PROP_FULLSCREENSTATE_CLIENT |
                                                Desktop::Rule::RULE_PROP_FULLSCREENSTATE_INTERNAL | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
    window->updateDecorationValues();
    g_layoutManager->recalculateMonitor(MONITOR, Layout::CLayoutManager::RECALCULATE_MONITOR_REASON_TOGGLE_FULLSCREEN);
    WORKSPACE->m_space->recalculate(layoutAware ? Layout::RECALCULATE_REASON_TOGGLE_LAYOUT_HANDLED_FULLSCREEN : Layout::RECALCULATE_REASON_TOGGLE_DEFAULT_HANDLED_FULLSCREEN);

    window->sendWindowSize(true);

    // recheck the work area again because visibility checks report 1 window on fs / maximize as tiled + visible
    // because the windows below fs are not visible obviously but because we update fullscreen fade which sets that
    // state later, it does it wrong
    WORKSPACE->updateWindows();
    WORKSPACE->m_space->recalculate(FULLSCREEN_REQUEST_RESULT == FULLSCREEN_REQUEST_DEFAULT_HANDLED ? Layout::RECALCULATE_REASON_TOGGLE_DEFAULT_HANDLED_FULLSCREEN :
                                                                                                      Layout::RECALCULATE_REASON_TOGGLE_LAYOUT_HANDLED_FULLSCREEN);
    WORKSPACE->forceReportSizesToWindows();

    g_pInputManager->recheckIdleInhibitorStatus();
}

void CFullscreenController::setWindowFullscreenModeClient(const PHLWINDOW window, const eFullscreenMode mode, bool layoutAware) {
    if (!window)
        return;

    const auto FS_HANDLER = getFsHandler(window, layoutAware);
    if (!FS_HANDLER) {
        Log::logger->log(Log::ERR, "window {} doesn't have FS handler assinged. This should never happen", window->m_title);
        return;
    }

    FS_HANDLER->setTargetFullscreenModeClient(window->m_target, mode);

    g_pXWaylandManager->setWindowFullscreen(window, mode == FSMODE_FULLSCREEN);
}

WP<IFullscreenHandler> CFullscreenController::getFsHandler(const PHLWINDOW window, std::optional<bool> layoutHandled) {
    if (!window)
        return nullptr;

    if (!layoutHandled.has_value())
        layoutHandled = layoutManagedFS(window);

    const auto HANDLERS = getFsHandlersForWorkspace(window->m_workspace);

    return (layoutHandled.value() ? (window->m_isFloating ? HANDLERS.FLOATING_FS_HANDLER : HANDLERS.TILED_FS_HANDLER) :
                                    (window->m_isFloating ? HANDLERS.FLOATING_FS_HANDLER : HANDLERS.TILED_DEFAULT_FS_HANDLER));
}

CFullscreenController::SSFsHandlersForWindow CFullscreenController::getFsHandlersForWorkspace(const PHLWORKSPACE workspace) const {
    if (!workspace || !workspace->m_space || !workspace->m_space->algorithm() || !workspace->m_space->algorithm()->floatingAlgo() || !workspace->m_space->algorithm()->tiledAlgo())
        return {};

    const auto TILED_FS_HANDLER         = workspace->m_space->algorithm()->tiledAlgo()->getFSHandler();
    const auto TILED_DEFAULT_FS_HANDLER = workspace->m_space->algorithm()->tiledAlgo()->IModeAlgorithm::getFSHandler();

    const auto FLOATING_FS_HANDLER = workspace->m_space->algorithm()->floatingAlgo()->getFSHandler();

    if (!TILED_FS_HANDLER || !TILED_DEFAULT_FS_HANDLER || !FLOATING_FS_HANDLER) {
        Log::logger->log(Log::ERR, "workspace ID:{} doesn't have FS handlers assinged. This should never happen", workspace->m_id);
        return {};
    }

    return {
        .TILED_FS_HANDLER         = TILED_FS_HANDLER,
        .TILED_DEFAULT_FS_HANDLER = TILED_DEFAULT_FS_HANDLER,
        .FLOATING_FS_HANDLER      = FLOATING_FS_HANDLER,
    };
}
