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

// Window

bool CFullscreenController::isFullscreen(const PHLWINDOW window, const std::optional<eFullscreenMode> mode, const std::optional<bool> covering) {
    if (!window || !window->m_target || mode == FSMODE_NONE)
        return false;

    if (mode.value_or(FSMODE_FULLSCREEN) == FSMODE_NONE) {
        Log::logger->log(Log::ERR, "Passed mode = FSMODE_NONE into isFullscreen. This must never happpen. Negating the result instead");
        !isFullscreen(window, std::nullopt, covering);
    }

    const auto FS_HANDLER = getFSHandler(window);

    /* Error Correction - try once */
    const auto returnBoolAfterErrorCorrection = [&](const WP<Fullscreen::IFullscreenHandler> FS_HANDLER, const WP<Desktop::View::CWindow> FS_WINDOW) -> bool {

        // FS window isn't found to begin with
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
    if (!window || !window->m_workspace || !window->m_workspace->m_space || !window->m_workspace->m_space->algorithm())
        return SFullscreenMode{};

    const auto FS_HANDLER = getFSHandler(window);

    if (!FS_HANDLER)
        return SFullscreenMode{};

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

    // Layout Handled
    // If a window is not FS at all, we consider its handler to be layout if it is in a workspace with a layout that implements their custom FS behaviour.
    return FS_HANDLER_NAME & FULLSCREEN_HANDLER_LAYOUT;
}

// Workspace

bool CFullscreenController::hasFullscreen(const PHLWORKSPACE workspace, const std::optional<bool> covering) {
    if (!workspace || !workspace->m_space || !workspace->m_space->algorithm() || !workspace->m_space->algorithm()->tiledAlgo() || !workspace->m_space->algorithm()->floatingAlgo())
        return false;

    // ASSUMPTION: Floating FS window layers ontop of Tiled Default Handled FS window which layers ontop of Tiled Layout Handled FS window
    // TODO: implement a way to get the topmost FS window so layouts can implement FS behaviour that may layer over Tiled Default Handled FS windows

    const auto TILED_FS_HANDLER = workspace->m_space->algorithm()->tiledAlgo()->getFSHandler();

    const auto DEFAULT_TILED_FS_HANDLER = workspace->m_space->algorithm()->tiledAlgo()->IModeAlgorithm::getFSHandler();

    // only one floating algo in hyprland so far, which is default handled.
    const auto FLOATING_FS_HANDLER = workspace->m_space->algorithm()->floatingAlgo()->getFSHandler();

    if (!TILED_FS_HANDLER || !DEFAULT_TILED_FS_HANDLER || !FLOATING_FS_HANDLER) {
        Log::logger->log(Log::ERR, "workspace ID:{} doesn't have FS handlers assinged. This should never happen", workspace->m_id);
        return false;
    }

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

    // check floating first
    if (FLOATING_FS_HANDLER->hasFullscreen(covering)) {
        return returnBoolAfterErrorCorrection(FLOATING_FS_HANDLER);
    }

    // check default handled tiled second
    if (DEFAULT_TILED_FS_HANDLER->hasFullscreen(covering)) {
        return returnBoolAfterErrorCorrection(DEFAULT_TILED_FS_HANDLER);
    }

    // check layout handled tiled last
    if (TILED_FS_HANDLER->hasFullscreen(covering)) {
        return returnBoolAfterErrorCorrection(TILED_FS_HANDLER);
    }

    return false;
}

PHLWINDOW CFullscreenController::getFullscreenWindow(const PHLWORKSPACE workspace, const std::optional<bool> covering) {
    if (!workspace || !workspace->m_space || !workspace->m_space->algorithm())
        return nullptr;

    // ASSUMPTION: Floating FS window layers ontop of Tiled Default Handled FS window which layers ontop of Tiled Layout Handled FS window
    // TODO: implement a way to get the topmost FS window so layouts can implement FS behaviour that may layer over Tiled Default Handled FS windows

    const auto TILED_FS_HANDLER = workspace->m_space->algorithm()->tiledAlgo()->getFSHandler();

    const auto DEFAULT_TILED_FS_HANDLER = workspace->m_space->algorithm()->tiledAlgo()->IModeAlgorithm::getFSHandler();

    // only one floating algo in hyprland so far, which is default handled
    const auto FLOATING_FS_HANDLER = workspace->m_space->algorithm()->floatingAlgo()->getFSHandler();

    if (!TILED_FS_HANDLER || !DEFAULT_TILED_FS_HANDLER || !FLOATING_FS_HANDLER) {
        Log::logger->log(Log::ERR, "workspace ID:{} doesn't have FS handlers assinged. This should never happen", workspace->m_id);
        return nullptr;
    }

    /* Error Correction - try once */

    const auto returnWindowAfterErrorCorrection = [&](WP<Fullscreen::IFullscreenHandler> FS_HANDLER, SP<Layout::ITarget> FSTARGET) -> PHLWINDOW {
        
        // FS target isn't found to begin with
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

    // check floating first
    if (const auto FSTARGET = FLOATING_FS_HANDLER->getFullscreen(covering); FSTARGET) {
        return returnWindowAfterErrorCorrection(FLOATING_FS_HANDLER, FSTARGET);
    }

    // check default handled tiled second
    if (const auto FSTARGET = DEFAULT_TILED_FS_HANDLER->getFullscreen(covering); FSTARGET && DEFAULT_TILED_FS_HANDLER->isFullscreen(FSTARGET)) {
        return returnWindowAfterErrorCorrection(DEFAULT_TILED_FS_HANDLER, FSTARGET);
    }

    // check layout handled tiled last
    const auto FSTARGET = TILED_FS_HANDLER->getFullscreen(covering);
    return returnWindowAfterErrorCorrection(TILED_FS_HANDLER, FSTARGET);
}

SFullscreenMode CFullscreenController::getFullscreenModes(const PHLWORKSPACE workspace, const std::optional<bool> covering) {
    if (!workspace || !workspace->m_space || !workspace->m_space->algorithm())
        return SFullscreenMode{};

    // ASSUMPTION: Floating FS window layers ontop of Tiled Default Handled FS window which layers ontop of Tiled Layout Handled FS window
    // TODO: implement a way to get the topmost FS window so layouts can implement FS behaviour that may layer over Tiled Default Handled FS windows

    const auto TILED_FS_HANDLER = workspace->m_space->algorithm()->tiledAlgo()->getFSHandler();

    const auto DEFAULT_TILED_FS_HANDLER = workspace->m_space->algorithm()->tiledAlgo()->IModeAlgorithm::getFSHandler();

    /* Error Correction - try once */
    const auto returnModesAfterErrorCorrection = [&](WP<Fullscreen::IFullscreenHandler> FS_HANDLER, SP<Layout::ITarget> FSTARGET) {

        // FS target isn't found to begin with
        if (!FSTARGET)
            return SFullscreenMode{};

        auto fsModes = FS_HANDLER->getFullscreenModes(FSTARGET);

        if (fsModes.internal != FSMODE_NONE && !FS_HANDLER->isFullscreen(FSTARGET, std::nullopt, covering)) {
            FS_HANDLER->syncFullscreenTargets();
            return FS_HANDLER->getFullscreenModes(FSTARGET);
        } else {
            return fsModes;
        }
        return SFullscreenMode{};
    };

    // only one floating algo in hyprland so far, which is default handled
    const auto FLOATING_FS_HANDLER = workspace->m_space->algorithm()->floatingAlgo()->getFSHandler();

    if (!TILED_FS_HANDLER || !DEFAULT_TILED_FS_HANDLER || !FLOATING_FS_HANDLER) {
        Log::logger->log(Log::ERR, "workspace ID:{} doesn't have FS handlers assinged. This should never happen", workspace->m_id);
        return SFullscreenMode{};
    }

    // check floating first
    if (const auto FSTARGET = FLOATING_FS_HANDLER->getFullscreen(covering); FSTARGET) {
        return returnModesAfterErrorCorrection(FLOATING_FS_HANDLER, FSTARGET);
    }

    // check default handled tiled second
    if (const auto FSTARGET = DEFAULT_TILED_FS_HANDLER->getFullscreen(covering); FSTARGET) {
        return returnModesAfterErrorCorrection(DEFAULT_TILED_FS_HANDLER, FSTARGET);
    }

    // check layout handled tiled last
    const auto FSTARGET = TILED_FS_HANDLER->getFullscreen(covering);
    return FSTARGET ? returnModesAfterErrorCorrection(TILED_FS_HANDLER, FSTARGET) : SFullscreenMode{};
}

// Monitor

bool CFullscreenController::hasFullscreen(const PHLMONITOR monitor, const std::optional<bool> covering) {
    if (!monitor || (!monitor->m_activeSpecialWorkspace && !monitor->m_activeWorkspace))
        return false;

    PHLWORKSPACE activeWorkspace = nullptr;

    // Check special workspace first since it renders on top of regular workspaces
    if (const auto ACTIVE_SPECIAL_WORKSPACE = monitor->m_activeSpecialWorkspace; ACTIVE_SPECIAL_WORKSPACE) {
        activeWorkspace = ACTIVE_SPECIAL_WORKSPACE;

    } else if (const auto ACTIVE_WORKSPACE = monitor->m_activeWorkspace; ACTIVE_WORKSPACE) {
        activeWorkspace = ACTIVE_WORKSPACE;
    }

    if (!activeWorkspace)
        return false;

    return hasFullscreen(activeWorkspace) && getFullscreenModes(activeWorkspace).internal == FSMODE_FULLSCREEN;
}
PHLWINDOW CFullscreenController::getFullscreenWindow(const PHLMONITOR monitor, const std::optional<bool> covering) {
    if (!monitor || (!monitor->m_activeSpecialWorkspace && !monitor->m_activeWorkspace))
        return nullptr;

    PHLWORKSPACE activeWorkspace = nullptr;

    // Check special workspace first since it renders on top of regular workspaces
    if (const auto ACTIVE_SPECIAL_WORKSPACE = monitor->m_activeSpecialWorkspace; ACTIVE_SPECIAL_WORKSPACE) {
        activeWorkspace = ACTIVE_SPECIAL_WORKSPACE;

    } else if (const auto ACTIVE_WORKSPACE = monitor->m_activeWorkspace; ACTIVE_WORKSPACE) {
        activeWorkspace = ACTIVE_WORKSPACE;
    }

    if (!activeWorkspace)
        return nullptr;

    const auto FS_WINDOW = getFullscreenWindow(activeWorkspace, covering);

    return getFullscreenModes(FS_WINDOW).internal == FSMODE_FULLSCREEN ? getFullscreenWindow(activeWorkspace, covering) : nullptr;
}

SFullscreenMode CFullscreenController::getFullscreenModes(const PHLMONITOR monitor, const std::optional<bool> covering) {
    if (!monitor || (!monitor->m_activeSpecialWorkspace && !monitor->m_activeWorkspace))
        return SFullscreenMode{};

    PHLWORKSPACE activeWorkspace = nullptr;

    // Check special workspace first since it renders on top of regular workspaces
    if (const auto ACTIVE_SPECIAL_WORKSPACE = monitor->m_activeSpecialWorkspace; ACTIVE_SPECIAL_WORKSPACE) {
        activeWorkspace = ACTIVE_SPECIAL_WORKSPACE;

    } else if (const auto ACTIVE_WORKSPACE = monitor->m_activeWorkspace; ACTIVE_WORKSPACE) {
        activeWorkspace = ACTIVE_WORKSPACE;
    }

    if (!activeWorkspace)
        return SFullscreenMode{};

    return getFullscreenModes(activeWorkspace);
}

// Handler

eFullscreenHandler CFullscreenController::getFullscreenHandlerName(const PHLWINDOW window) {
    if (!window || !window->m_workspace || !window->m_workspace->m_space || !window->m_workspace->m_space->algorithm())
        return FULLSCREEN_HANDLER_NONE;

    // IMPORTANT: no layoutHandled value passed -> infinite recursion.
    const auto LAYOUT_FS_HANDLER = getFSHandler(window, true);

    // IMPORTANT: no layoutHandled value passed -> infinite recursion.
    const auto DEFAULT_FS_HANDLER = getFSHandler(window, false);

    if (!LAYOUT_FS_HANDLER || !DEFAULT_FS_HANDLER) {
        Log::logger->log(Log::ERR, "window {} doesn't have FS handler assinged. This should never happen", window->m_title);
        return FULLSCREEN_HANDLER_NONE;
    }

    eFullscreenHandler handlerName = FULLSCREEN_HANDLER_NONE;

    // if default handled - either internal or client must be set in default FS handler
    if (DEFAULT_FS_HANDLER->isFullscreen(window->m_target) || DEFAULT_FS_HANDLER->getFullscreenModes(window->m_target).client != FSMODE_NONE)
        handlerName = DEFAULT_FS_HANDLER->getFullscreenHandlerName();
    // Layout Handled
    else
        // If a window is not FS at all, we consider its handler to be layout if it is in a workspace with a layout that implements their custom FS behaviour.
        handlerName = LAYOUT_FS_HANDLER->getFullscreenHandlerName();

    if (handlerName == FULLSCREEN_HANDLER_NONE) {
        Log::logger->log(Log::ERR, "window {} doesn't have FS handler assinged. This should never happen", window->m_title);
        return FULLSCREEN_HANDLER_NONE;
    }

    return handlerName;
}

std::string CFullscreenController::getFullscreenHandlerNameAsString(const PHLWINDOW window) {
    const auto FS_HANDLER_NAME = getFullscreenHandlerName(window);

    switch (FS_HANDLER_NAME) {
        case FULLSCREEN_HANDLER_NONE: return "none";
        case FULLSCREEN_HANDLER_DEFAULT: return "default";
        case FULLSCREEN_HANDLER_SCROLLING: return "scrolling";
        default: return "unknown";
    }
}

// FS Mode Setters

void CFullscreenController::setFullscreenMode(const PHLWINDOW window, std::optional<eFullscreenMode> internal, std::optional<eFullscreenMode> client,
                                              std::optional<bool> layoutAware) {
    // A lot of things are happening here so handling of each section is as separate as is reasonable to be readable and maintainable instead of pursuing pure performance and mixing everyhting up.
    if (!window)
        return;

    const bool WANT_SYNC = window->m_ruleApplicator->syncFullscreen().valueOrDefault();

    bool       stateChanged = false;

    // Clamp values to valid range
    if (internal.has_value())
        internal = std::clamp(internal.value(), sc<eFullscreenMode>(0), FSMODE_FULLSCREEN);
    if (client.has_value())
        client = std::clamp(client.value(), sc<eFullscreenMode>(0), FSMODE_FULLSCREEN);

    // if new values not provided, we need to use the old values.
    eFullscreenMode targetInternalMode = internal.value_or(FSMODE_NONE);
    eFullscreenMode targetClientMode   = client.value_or(FSMODE_NONE);

    /*
    
        We need to check if a the window is already FS - therefore is already tracked by a handler - AND it is not handled using the same layoutAware mode as the current FS request.

        If the past handled mode and current handled mode is not the same; it implies that the window IS FS; we need to move it to the new handler we will use for the current FS request

        If a window is not FS at all, layoutManagedFS(window) returns true as that's the 'default' mode.

    */
    const bool WAS_LAYOUT_HANDLED = layoutManagedFS(window);

    const auto ORIGINAL_FS_HANDLER = getFSHandler(window, WAS_LAYOUT_HANDLED);
    const auto OLD_FS_MODES        = ORIGINAL_FS_HANDLER->getFullscreenModes(window->m_target);


    const auto TO_BE_USED_FS_HANDLER = getFSHandler(window, layoutAware.value_or(WAS_LAYOUT_HANDLED));
    if (!TO_BE_USED_FS_HANDLER) {
        Log::logger->log(Log::ERR, "window {} doesn't have FS handler assinged. This should never happen", window->m_title);
        return;
    }

    /* Helper Lambdas */

    // handles FSMODE_MAX case. If nothing to handler on that front, saves the provided old values
    const auto saveClientInternalValues = [&](const SFullscreenMode& OLD_FS_MODES) {
        // If client requests FSMODE_FULLSCREEN when the underlying FS mode is FSMODE_MAXIMIZED
        if (client.value_or(FSMODE_NONE) == FSMODE_FULLSCREEN && OLD_FS_MODES.internal == FSMODE_MAXIMIZED) {
            m_fsModeMaxWindows.emplace(window);
        }
        // If client requests FSMODE_NONE and the window is FSMODE_MAX
        else if (const auto IT = m_fsModeMaxWindows.find(window);
                 client.value_or(FSMODE_MAXIMIZED) == FSMODE_NONE && (IT != m_fsModeMaxWindows.end() && IT->valid() && !IT->expired())) {
            targetClientMode = FSMODE_MAXIMIZED;
            m_fsModeMaxWindows.erase(IT);
        } else {
            // save the old values if new ones aren't provided
            targetInternalMode = internal.value_or(OLD_FS_MODES.internal);
            targetClientMode   = client.value_or(OLD_FS_MODES.client);
        }
    };

    // Maintenence on FSMODE_MAX list
    const auto syncFsModeMaxWindows = [&]() {
        for (auto it = m_fsModeMaxWindows.begin(); it != m_fsModeMaxWindows.end();) {

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
        Handled moving the window from one handler to another if needed
        Save internal and client states
    */
    // If window is FS and is handled differently than before, this implies that the window is already fullscreen (as a window's non-FS state defaults to 'layout handled')
    if (layoutAware.value_or(WAS_LAYOUT_HANDLED) != WAS_LAYOUT_HANDLED) {
        // we need to remove the window from its old handler

        stateChanged = true;

        saveClientInternalValues(OLD_FS_MODES);

        /* Remove Window from Old handler */

        // if syncing FS, this guarantees that it will be removed from the handler as both internal and client will be removed
        if (WANT_SYNC) {
            setWindowFullscreenModeClient(window, FSMODE_NONE, WAS_LAYOUT_HANDLED);
            setWindowFullscreenModeInternal(window, FSMODE_NONE, WAS_LAYOUT_HANDLED);
        }

        // if not syncing FS, we need to move the unmodified FS value from the old one to the new one
        else {

            // remove window from the handler
            if (OLD_FS_MODES.internal != FSMODE_NONE)
                setWindowFullscreenModeInternal(window, FSMODE_NONE, WAS_LAYOUT_HANDLED);
            if (OLD_FS_MODES.client != FSMODE_NONE)
                setWindowFullscreenModeClient(window, FSMODE_NONE, WAS_LAYOUT_HANDLED);
        }

    }
    // if window is FS and it's handled the same as before OR it's not FS at all
    else {

        const auto OLD_FS_MODES = TO_BE_USED_FS_HANDLER->getFullscreenModes(window->m_target);
        // TODO if only one state changed, we can skip setting the other. It's more robust to set both but it's tecnically unnecessary
        if (OLD_FS_MODES.internal != internal.value_or(OLD_FS_MODES.internal) || OLD_FS_MODES.client != client.value_or(OLD_FS_MODES.client))
            stateChanged = true;

        saveClientInternalValues(OLD_FS_MODES);
    }






    // Handle fullscreen request SYNCing internal and client states
    if (WANT_SYNC) {

        if (targetInternalMode != targetClientMode)
            stateChanged = true;

        // If only internal has value
        if (internal.has_value() && !client.has_value()) {
            targetClientMode = targetInternalMode;
        }
        // If only client has value or both have values - internal defers to client if both have values!
        else {
            targetInternalMode = targetClientMode;
        }
    }



    /*
        Handling Pinned windows - allow_pin_fullscreen

        Pinned windows can only be floating, therefore it is guaranteed that they will use the same FS handler within the workspace

        Moving FSed pinned window to another workspace: move without minding that; it'll be Fs in the target workspace anyway and the next un-FS request will correctly handle it

        changing the FS mode of an already FS window (from fullscreen to maximise and vice versa) also doesn't need special handling: let it keep its m_pinFullscreened value intact.

    */
    const bool WINDOW_IS_ALREADY_INTERNAL_FS_HANDLER_AGNOSTIC = OLD_FS_MODES.internal != FSMODE_NONE;
    // Current window is a pinned window - in FS state or not -- must specially handle
    const bool HANDLE_PINNED_WINDOW = window->m_pinned || window->m_pinFullscreened;
    bool pinnedWinowGoingFullscreen = false;

    static auto PALLOWPINFULLSCREEN = CConfigValue<Config::INTEGER>("binds:allow_pin_fullscreen");
    if (*PALLOWPINFULLSCREEN && !window->m_pinFullscreened && !WINDOW_IS_ALREADY_INTERNAL_FS_HANDLER_AGNOSTIC && window->m_pinned) {
        pinnedWinowGoingFullscreen = true;
    }
    if (*PALLOWPINFULLSCREEN && window->m_pinFullscreened && WINDOW_IS_ALREADY_INTERNAL_FS_HANDLER_AGNOSTIC && !window->m_pinned && targetInternalMode == FSMODE_NONE) {
        pinnedWinowGoingFullscreen = false;
    }






    // maybe todo: If only one state changed, we may skip setting the other. It's more robust to set both but it's tecnically unnecessary

    // set new FS state in the correct handler: if specified, use that handler. If not, use the handler that was used before (if not FS, it'll use layout handler)
    if (stateChanged) {


        // We have a pinned window to handle
        if (HANDLE_PINNED_WINDOW) {

            if (pinnedWinowGoingFullscreen) {
                // we are FSing a window as normal, just save the fact that it's pinned
                window->m_pinned          = false;
                window->m_pinFullscreened = true;
            }

            else {
                // We are unFSing a window or only changing its client state

                // If allow_pin_fullscreen = false, setting only the client state is allowed if not dispatched via a sync_state FS request
                if (!(*PALLOWPINFULLSCREEN) && WANT_SYNC) {
                    targetClientMode = FSMODE_NONE;
                }

                if (!window->m_pinFullscreened || targetInternalMode == FSMODE_NONE) {
                    window->m_pinned          = true;
                    window->m_pinFullscreened = false;
                }
                // if pinned window is changing between FS modes leave pin state intact
            }
        }


        // set states
        setWindowFullscreenModeClient(window, targetClientMode, layoutAware.value_or(WAS_LAYOUT_HANDLED));
        setWindowFullscreenModeInternal(window, targetInternalMode, layoutAware.value_or(WAS_LAYOUT_HANDLED));
    }
    syncFsModeMaxWindows();
}

void CFullscreenController::setWindowFullscreenModeInternal(const PHLWINDOW window, const eFullscreenMode mode, bool layoutAware) {

    if (!window || !validMapped(window) || !window->m_monitor || !window->m_workspace || !window->m_workspace->m_space || !window->m_workspace->m_space->algorithm())
        return;

    const auto MONITOR   = window->m_monitor.lock();
    const auto WORKSPACE = window->m_workspace;

    const auto SPACE     = window->m_workspace->m_space;
    const auto ALGORITHM = window->m_workspace->m_space->algorithm();

    // there's no layout managed floating algo.
    const auto            WINDOW_FS_HANDLER        = getFSHandler(window, layoutAware);
    const SFullscreenMode WINDOW_FS_MODE           = getFullscreenModes(window);
    const bool            INTERNAL_FS_MODE_CHANGED = !window->m_pinned && WINDOW_FS_MODE.internal != mode;

    if (!WINDOW_FS_HANDLER) {
        Log::logger->log(Log::ERR, "window {} doesn't have FS handler assinged. This should never happen", window->m_title);
        return;
    }

    static auto PDIRECTSCANOUT      = CConfigValue<Config::INTEGER>("render:direct_scanout");

    if (window->m_isFloating && WINDOW_FS_MODE.internal == FSMODE_NONE && mode != FSMODE_NONE)
        g_pHyprRenderer->damageWindow(window);


    // if the new window is not to be laytout handled, is not fullscreen yet, and there is already a covering FS window in the workspace
    if (hasFullscreen(WORKSPACE) && !isFullscreen(window) && !layoutAware) {

        // If the covering FS window is not layout handled, replace the covering FS with the current one.
        // If it is layout handled, !!Which implies that the covering FS window is tiling!!, layer the current FS window ontop of the covering FS window
            // e.g. in scrolling, floating FS windows (which are always default handled) are allowed to layer over layout handled tiling FS windows 
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
        return;
    }

    // Internal mode must be set by the handler; not setting it here because last FS mode should be made available to handlers
    const eFullscreenRequestResult FULLSCREEN_REQUEST_RESULT =
        WINDOW_FS_HANDLER->requestFullscreen({.target = window->m_target, .currentMode = WINDOW_FS_MODE.internal, .mode = mode});

    if (mode == FSMODE_NONE && window->m_isFloating)
        // If window group, use the group target to set all member windows
        ALGORITHM->recenter(window->layoutTarget());

    SPACE->recalculate(FULLSCREEN_REQUEST_RESULT == FULLSCREEN_REQUEST_DEFAULT_HANDLED ? Layout::RECALCULATE_REASON_TOGGLE_DEFAULT_HANDLED_FULLSCREEN :
                                                                                         Layout::RECALCULATE_REASON_TOGGLE_LAYOUT_HANDLED_FULLSCREEN);

    g_pEventManager->postEvent(SHyprIPCEvent{.event = "fullscreen", .data = std::to_string(sc<int>(mode) != FSMODE_NONE)});
    Event::bus()->m_events.window.fullscreen.emit(window);

    // todo possibly redundant as it's called by the FS handler?
    window->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_FULLSCREEN | Desktop::Rule::RULE_PROP_FULLSCREENSTATE_CLIENT |
                                                Desktop::Rule::RULE_PROP_FULLSCREENSTATE_INTERNAL | Desktop::Rule::RULE_PROP_ON_WORKSPACE);

    window->updateDecorationValues();
    g_layoutManager->recalculateMonitor(MONITOR, Layout::CLayoutManager::RECALCULATE_MONITOR_REASON_TOGGLE_FULLSCREEN);

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
    if (!window || !window->m_workspace || !window->m_workspace->m_space || !window->m_workspace->m_space->algorithm())
        return;

    const auto FS_HANDLER = getFSHandler(window, layoutAware);
    if (!FS_HANDLER) {
        Log::logger->log(Log::ERR, "window {} doesn't have FS handler assinged. This should never happen", window->m_title);
        return;
    }

    FS_HANDLER->setTargetFullscreenModeClient(window->m_target, mode);

    g_pXWaylandManager->setWindowFullscreen(window, mode == FSMODE_FULLSCREEN);
}

WP<IFullscreenHandler> CFullscreenController::getFSHandler(const PHLWINDOW window, std::optional<bool> layoutHandled) {
    if (!window || !window->m_workspace || !window->m_workspace->m_space || !window->m_workspace->m_space->algorithm() ||
        !window->m_workspace->m_space->algorithm()->floatingAlgo() || !window->m_workspace->m_space->algorithm()->tiledAlgo())
        return nullptr;

    // if not value given, use the handler the window is currently using.
    if (!layoutHandled.has_value()) {
        layoutHandled = layoutManagedFS(window);
    }

    // Layout Handled
    return (layoutHandled.value() ? (window->m_isFloating ? window->m_workspace->m_space->algorithm()->floatingAlgo()->getFSHandler() :
                                                            window->m_workspace->m_space->algorithm()->tiledAlgo()->getFSHandler()) :
                                    // Default Handled
                                    (window->m_isFloating ? window->m_workspace->m_space->algorithm()->floatingAlgo()->IModeAlgorithm::getFSHandler() :
                                                            window->m_workspace->m_space->algorithm()->tiledAlgo()->IModeAlgorithm::getFSHandler()));
}
