#include "FullscreenController.hpp"

#include "../../managers/fullscreen/handler/FullscreenHandler.hpp"

#include "../../layout/algorithm/Algorithm.hpp"
#include "../../layout/algorithm/FloatingAlgorithm.hpp"
#include "../../layout/algorithm/TiledAlgorithm.hpp"

#include "../../output/Monitor.hpp"
#include "../../render/Renderer.hpp"
#include "../../debug/log/Logger.hpp"
#include "../../desktop/DesktopTypes.hpp"
#include "../../desktop/view/Window.hpp"
#include "event/EventBus.hpp"
#include "managers/EventManager.hpp"
#include <optional>

using namespace Fullscreen;




//  ERSTARR - IMPORTANT: Might wanna add a layoutHandled param to isFullscreen to avoid multiple redundant calls in calling both isFullscreen and isLayoutHandledFullscreen.



// Window

bool CFullscreenController::isFullscreen(const PHLWINDOW window, const std::optional<eFullscreenMode> mode, const std::optional<bool> covering) {
    if(!window || !window->m_target || mode == FSMODE_NONE)
        return false;


    const auto FS_HANDLER = getFSHandler(window);

    if (!FS_HANDLER)
        return false; // erstarr todo - error log


    // ERSTARR TEST - getFsHandler w/o layout value passed should correctly get the FS handler the FS window is using , making below code redundant
    // // if window was default handled
    // if (FS_HANDLER->IFullscreenHandler::isFullscreen(window->m_target, mode, covering))
    //     return true;


    // // If window was layout handled
    // if (FS_HANDLER->isFullscreen(window->m_target, mode, covering))
    //     return true;


    // return false;


    return FS_HANDLER->isFullscreen(window->m_target, mode, covering);

}


SFullscreenMode CFullscreenController::getFullscreenModes(const PHLWINDOW window) {
    if(!window || !window->m_workspace || !window->m_workspace->m_space || !window->m_workspace->m_space->algorithm())
        return SFullscreenMode{};

        
    const auto FS_HANDLER = getFSHandler(window);

    if (!FS_HANDLER || !FS_HANDLER->isFullscreen(window->m_target))
        return SFullscreenMode{}; // ERSTARR TODO - ERROR LOG

    return FS_HANDLER->getFullscreenModes(window->m_target);

    // ERSTARR TEST - getFsHandler w/o layout value passed should correctly get the FS handler the FS window is using , making below code redundant
    // // If layout handled
    // if (FS_HANDLER->isFullscreen(window->m_target) || FS_HANDLER->getFullscreenMode(window->m_target).client != FSMODE_NONE)
    //     return FS_HANDLER->getFullscreenMode(window->m_target);

    

    // // if window was default handled
    // return FS_HANDLER->IFullscreenHandler::getFullscreenMode(window->m_target);
}


bool CFullscreenController::layoutManagedFS(const PHLWINDOW window) {
    if(!window)
        return false;


    const auto FS_HANDLER_NAME = getFullscreenHandlerName(window);


    if (FS_HANDLER_NAME == FULLSCREEN_HANDLER_NONE)
        return false; // ERSTARR TODO - LOG AN ERROR

    // Layout Handled
    // If a window is not FS at all, we consider its handler to be layout if it is in a workspace with a layout that implements their custom FS behaviour.
    // Change this condition hyprland gets layout handled floating FS windows
    return FS_HANDLER_NAME > 1;
}


// Workspace

bool CFullscreenController::hasFullscreen(const PHLWORKSPACE workspace, const std::optional<bool> covering) {
    if(!workspace || !workspace->m_space || !workspace->m_space->algorithm() || !workspace->m_space->algorithm()->tiledAlgo() || !workspace->m_space->algorithm()->floatingAlgo())
        return false;

    // ASSUMPTION: Floating FS window layers ontop of Tiled Default Handled FS window which layers ontop of Tiled Layout Handled FS window
    // TODO: implement a way to get the topmost FS window so layouts can implement FS behaviour that may layer over Tiled Default Handled FS windows

    const auto TILED_FS_HANDLER  = workspace->m_space->algorithm()->tiledAlgo()->getFSHandler();

    const auto DEFAULT_TILED_FS_HANDLER = workspace->m_space->algorithm()->tiledAlgo()->IModeAlgorithm::getFSHandler();

    // only one floating algo in hyprland so far, which is default handled.
    const auto FLOATING_FS_HANDLER      = workspace->m_space->algorithm()->floatingAlgo()->getFSHandler();

    if (!FLOATING_FS_HANDLER || !TILED_FS_HANDLER)
        return false; // ERROR LOG!

    // check floating first
    if (FLOATING_FS_HANDLER->hasFullscreen(covering))
        return true;
    
    // check default handled tiled second
    if (DEFAULT_TILED_FS_HANDLER->IFullscreenHandler::hasFullscreen(covering))
        return true;

    // check layout handled tiled last
    return TILED_FS_HANDLER->hasFullscreen(covering);
}


PHLWINDOW CFullscreenController::getFullscreenWindow(const PHLWORKSPACE workspace, const std::optional<bool> covering) {
    if(!workspace || !workspace->m_space || !workspace->m_space->algorithm())
        return nullptr;

    // ASSUMPTION: Floating FS window layers ontop of Tiled Default Handled FS window which layers ontop of Tiled Layout Handled FS window
    // TODO: implement a way to get the topmost FS window so layouts can implement FS behaviour that may layer over Tiled Default Handled FS windows

    const auto TILED_FS_HANDLER  = workspace->m_space->algorithm()->tiledAlgo()->getFSHandler();

    const auto DEFAULT_TILED_FS_HANDLER = workspace->m_space->algorithm()->tiledAlgo()->IModeAlgorithm::getFSHandler();
    
    // only one floating algo in hyprland so far, which is default handled
    const auto FLOATING_FS_HANDLER      = workspace->m_space->algorithm()->floatingAlgo()->getFSHandler();

    if (!TILED_FS_HANDLER || !FLOATING_FS_HANDLER)
        return nullptr; // ERSTARR TODO - ERROR LOG

    // check floating first
    if (const auto FSTARGET = FLOATING_FS_HANDLER->getFullscreen(covering); FSTARGET && FLOATING_FS_HANDLER->isFullscreen(FSTARGET))
        return FSTARGET->window();

    // check default handled tiled second
    if (const auto FSTARGET =  DEFAULT_TILED_FS_HANDLER->IFullscreenHandler::getFullscreen(covering); FSTARGET && DEFAULT_TILED_FS_HANDLER->isFullscreen(FSTARGET))
        return FSTARGET->window();

    // check layout handled tiled last
    const auto FSTARGET = TILED_FS_HANDLER->getFullscreen(covering);
    return FSTARGET && TILED_FS_HANDLER->isFullscreen(FSTARGET)? FSTARGET->window() : nullptr;

}
SFullscreenMode CFullscreenController::getFullscreenModes(const PHLWORKSPACE workspace, const std::optional<bool> covering) {
    if(!workspace || !workspace->m_space || !workspace->m_space->algorithm())
        return SFullscreenMode{};

    // ASSUMPTION: Floating FS window layers ontop of Tiled Default Handled FS window which layers ontop of Tiled Layout Handled FS window
    // TODO: implement a way to get the topmost FS window so layouts can implement FS behaviour that may layer over Tiled Default Handled FS windows

    const auto TILED_FS_HANDLER  = workspace->m_space->algorithm()->tiledAlgo()->getFSHandler();

    const auto DEFAULT_TILED_FS_HANDLER = workspace->m_space->algorithm()->tiledAlgo()->IModeAlgorithm::getFSHandler();

    // only one floating algo in hyprland so far, which is default handled
    const auto FLOATING_FS_HANDLER      = workspace->m_space->algorithm()->floatingAlgo()->getFSHandler();

    if (!TILED_FS_HANDLER || !FLOATING_FS_HANDLER)
        return SFullscreenMode{}; // ERSTARR TODO - ERROR LOG

    // check floating first
    if (const auto FSTARGET = FLOATING_FS_HANDLER->getFullscreen(covering); FSTARGET && FLOATING_FS_HANDLER->isFullscreen(FSTARGET))
        return FLOATING_FS_HANDLER->getFullscreenModes(FSTARGET);

    // check default handled tiled second
    if (const auto FSTARGET =  DEFAULT_TILED_FS_HANDLER->IFullscreenHandler::getFullscreen(covering); FSTARGET && DEFAULT_TILED_FS_HANDLER->IFullscreenHandler::isFullscreen(FSTARGET))
        return DEFAULT_TILED_FS_HANDLER->IFullscreenHandler::getFullscreenModes(FSTARGET);

    // check layout handled tiled last
    const auto FSTARGET = TILED_FS_HANDLER->getFullscreen(covering);
    return FSTARGET && TILED_FS_HANDLER->isFullscreen(FSTARGET) ? TILED_FS_HANDLER->getFullscreenModes(FSTARGET) : SFullscreenMode{};
}

// Monitor

bool CFullscreenController::hasFullscreen(const PHLMONITOR monitor, const std::optional<bool> covering) {
    if (!monitor || (!monitor->m_activeSpecialWorkspace && !monitor->m_activeWorkspace))
        return false;


    PHLWORKSPACE activeWorkspace = nullptr;

    // Check special workspace first since it renders on top of regular workspaces
    if (const auto ACTIVE_SPECIAL_WORKSPACE = monitor->m_activeSpecialWorkspace; ACTIVE_SPECIAL_WORKSPACE) {
        activeWorkspace = ACTIVE_SPECIAL_WORKSPACE;

    }
    else if (const auto ACTIVE_WORKSPACE = monitor->m_activeWorkspace; ACTIVE_WORKSPACE) {
        activeWorkspace = ACTIVE_WORKSPACE;
    }

    if (!activeWorkspace)
        return false;

    return hasFullscreen(activeWorkspace);

    // ERSTARR TODO - PRE THIS PR, MONITOR FS CHECKS SEEMS TO ONLY CONSIDER FS_MODE_FULLSCREEN. MAKE SURE THAN OUTSIDE YOU ALSO USE MODE TO ENFORCE THAT
}
PHLWINDOW CFullscreenController::getFullscreenWindow(const PHLMONITOR monitor, const std::optional<bool> covering) {
    if (!monitor || (!monitor->m_activeSpecialWorkspace && !monitor->m_activeWorkspace))
        return nullptr;

    PHLWORKSPACE activeWorkspace = nullptr;

    // Check special workspace first since it renders on top of regular workspaces
    if (const auto ACTIVE_SPECIAL_WORKSPACE = monitor->m_activeSpecialWorkspace; ACTIVE_SPECIAL_WORKSPACE) {
        activeWorkspace = ACTIVE_SPECIAL_WORKSPACE;

    }
    else if (const auto ACTIVE_WORKSPACE = monitor->m_activeWorkspace; ACTIVE_WORKSPACE) {
        activeWorkspace = ACTIVE_WORKSPACE;
    }

    if (!activeWorkspace)
        return nullptr;

    return getFullscreenWindow(activeWorkspace,covering);


    // ERSTARR TODO - PRE THIS PR, MONITOR FS CHECKS SEEMS TO ONLY CONSIDER FS_MODE_FULLSCREEN. MAKE SURE THAN OUTSIDE YOU ALSO USE MODE TO ENFORCE THAT
}

SFullscreenMode CFullscreenController::getFullscreenModes(const PHLMONITOR monitor, const std::optional<bool> covering) {
    if (!monitor || (!monitor->m_activeSpecialWorkspace && !monitor->m_activeWorkspace))
        return SFullscreenMode{};

    PHLWORKSPACE activeWorkspace = nullptr;

    // Check special workspace first since it renders on top of regular workspaces
    if (const auto ACTIVE_SPECIAL_WORKSPACE = monitor->m_activeSpecialWorkspace; ACTIVE_SPECIAL_WORKSPACE) {
        activeWorkspace = ACTIVE_SPECIAL_WORKSPACE;

    }
    else if (const auto ACTIVE_WORKSPACE = monitor->m_activeWorkspace; ACTIVE_WORKSPACE) {
        activeWorkspace = ACTIVE_WORKSPACE;
    }

    if (!activeWorkspace)
        return SFullscreenMode{};

    return  getFullscreenModes(activeWorkspace);
}

// Handler

eFullscreenHandler CFullscreenController::getFullscreenHandlerName(const PHLWINDOW window) {
    if(!window || !window->m_workspace || !window->m_workspace->m_space || !window->m_workspace->m_space->algorithm())
        return FULLSCREEN_HANDLER_NONE;

    // ERSTARR TODO - here, and with `DEFAULT_TILED_FS_HANDLER` above; IModeAlgorithm::getHandler() and IFullscreenHandler::... doesn't work because the FS list is a private member


    // IMPORTANT: no layoutHandled value passed -> no recursion.
    const auto LAYOUT_FS_HANDLER = getFSHandler(window, true);

    // IMPORTANT: no layoutHandled value passed -> no recursion.
    const auto DEFAULT_FS_HANDLER = getFSHandler(window, false);

    if (!LAYOUT_FS_HANDLER || !DEFAULT_FS_HANDLER)
        return FULLSCREEN_HANDLER_NONE; // ERSTARR TODO - ERROR LOG


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







// FS Mode Setters

// ERSTARR TODO -> NEED TO HANDLE THE CASE WHERE A WINDOW IS IN A HANDLER WITH ONLY ITS CLIENT STATE LEFT -- TEST EXTENSIVELY THAT A WINDOW IS NOT STUCK IN A LIST WHERE IT SHOULD NOT BE
void CFullscreenController::setFullscreenMode(const PHLWINDOW window, const std::optional<eFullscreenMode> internal, const std::optional<eFullscreenMode> client, std::optional<bool> layoutAware) {
    if (!window)
        return;

    bool stateChanged = false;

    // if new values not provided, we need to use the old values.
    eFullscreenMode targetInternalMode = internal.value_or(FSMODE_NONE);
    eFullscreenMode targetClientMode = client.value_or(FSMODE_NONE);



    /*
    
        We need to check if a the window is already FS - therefore is already tracked by a handler - AND it is not handled using the same layoutAware mode as the current FS request.

        If the past handled mode and current handled mode is not the same; it implies that the window IS FS; we need to move it to the new handler we will use for the current FS request

        If a window is not FS at all, layoutManagedFS(window) returns true as that's the 'default' mode.

    */


    const bool WAS_LAYOUT_HANDLED = layoutManagedFS(window);


    const auto TO_BE_USED_FS_HANDLER =  getFSHandler(window,layoutAware.value_or(WAS_LAYOUT_HANDLED));
    if (!TO_BE_USED_FS_HANDLER) {
        Log::logger->log(Log::ERR, "window {} doesn't have FS handler assinged. This should never happen", window->m_title);
        return;
    }

    // If window is FS and is handled differently than before, this implies that the window is already fullscreen (as a window's non-FS state is 'layout handled')
    if (layoutAware.value_or(WAS_LAYOUT_HANDLED) != WAS_LAYOUT_HANDLED) {

        stateChanged = true;

        // we need to remove the window from its old handler
        const auto ORIGINAL_FS_HANDLER = getFSHandler(window, WAS_LAYOUT_HANDLED);
        const auto OLD_FS_MODES = ORIGINAL_FS_HANDLER->getFullscreenModes(window->m_target);

        // save the old values if new ones aren't provided
        targetInternalMode = internal.value_or(OLD_FS_MODES.internal);
        targetClientMode = client.value_or(OLD_FS_MODES.client);

        // if syncing FS, this guarantees that it will be removed from the handler as both internal and client will be removed
        if (window->m_ruleApplicator->syncFullscreen().valueOrDefault()) {
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

        // save the old values if new ones aren't provided
        targetInternalMode = internal.value_or(OLD_FS_MODES.internal);
        targetClientMode = client.value_or(OLD_FS_MODES.client);
    }
    
    if (window->m_ruleApplicator->syncFullscreen().valueOrDefault()) {

        if (targetInternalMode != targetClientMode)
            stateChanged = true;

        targetInternalMode = std::clamp(targetInternalMode, sc<eFullscreenMode>(0), FSMODE_MAX);
        targetClientMode = targetInternalMode;


    }
    else {
        
        targetInternalMode = std::clamp(targetInternalMode, sc<eFullscreenMode>(0), FSMODE_MAX);
        targetClientMode = std::clamp(targetClientMode, sc<eFullscreenMode>(0), FSMODE_MAX);


    }


    targetInternalMode = std::clamp(targetInternalMode, sc<eFullscreenMode>(0), FSMODE_MAX);
    targetClientMode = std::clamp(targetClientMode, sc<eFullscreenMode>(0), FSMODE_MAX);


    // set new FS state in the correct handler: if specified, use that handler. If not, use the handler that was used before (if not FS, it'll use layout handler)
    if (stateChanged) {
        setWindowFullscreenModeClient(window, targetClientMode, layoutAware.value_or(WAS_LAYOUT_HANDLED));    
        setWindowFullscreenModeInternal(window, targetInternalMode, layoutAware.value_or(WAS_LAYOUT_HANDLED));
    }

}


void CFullscreenController::setWindowFullscreenModeInternal(const PHLWINDOW window, const eFullscreenMode mode, bool layoutAware) {
    
    if (!window || !validMapped(window) || !window->m_monitor || !window->m_workspace || !window->m_workspace->m_space || !window->m_workspace->m_space->algorithm())
        return;

    const auto MONITOR   = window->m_monitor.lock();
    const auto WORKSPACE = window->m_workspace;

    const auto SPACE = window->m_workspace->m_space;
    const auto ALGORITHM = window->m_workspace->m_space->algorithm();


    // there's no layout managed floating algo.
    const auto             WINDOW_FS_HANDLER        = getFSHandler(window, layoutAware);
    const SFullscreenMode& WINDOW_FS_MODE           = getFullscreenModes(window);
    const bool             WINDOW_IS_INTERNAL_FS    = isFullscreen(window);
    const bool             INTERNAL_FS_MODE_CHANGED = !window->m_pinned && WINDOW_FS_MODE.internal != mode;

    if (!WINDOW_FS_HANDLER) {
        Log::logger->log(Log::ERR, "window {} doesn't have FS handler assinged. This should never happen", window->m_title);
        return;
    }

    static auto PDIRECTSCANOUT      = CConfigValue<Config::INTEGER>("render:direct_scanout");
    static auto PALLOWPINFULLSCREEN = CConfigValue<Config::INTEGER>("binds:allow_pin_fullscreen");

    if (window->m_isFloating && WINDOW_FS_MODE.internal == FSMODE_NONE && mode != FSMODE_NONE)
        g_pHyprRenderer->damageWindow(window);



    if (*PALLOWPINFULLSCREEN && !window->m_pinFullscreened && !WINDOW_IS_INTERNAL_FS && window->m_pinned) {
        window->m_pinned          = false;
        window->m_pinFullscreened = true;
    }

    // arm m_suppressNextMaximize to swallow the set_maximized echo on fullscreen exit
    if (INTERNAL_FS_MODE_CHANGED && !window->m_isFloating && (getFullscreenModes(window).internal == FSMODE_FULLSCREEN) && mode != FSMODE_FULLSCREEN)
        window->m_suppressNextMaximize = true;


    // this should be redundant now
    // // if there is a FS window that covers the monitor right now, the window that covers the screen is a default handled FS window, and you are FSing(unFS leads to the same result) a window that is not that window
    // if (const auto FSWINDOW = PWORKSPACE->getFullscreenWindow(); FSWINDOW && FSWINDOW != PWINDOW && !FSWINDOW->m_target->layoutManagedFullscreen())
    //     setWindowFullscreenInternal(PWORKSPACE->getFullscreenWindow(), FSMODE_NONE);


    

    if (*PALLOWPINFULLSCREEN && window->m_pinFullscreened && WINDOW_IS_INTERNAL_FS && !window->m_pinned && mode == FSMODE_NONE) {
        window->m_pinned          = true;
        window->m_pinFullscreened = false;
    }



    // ERSTARR - THIS CODE IS MOVED TO CLIENT - IT SHOULD HOPEFULLY WORK
    // g_pXWaylandManager->setWindowFullscreen(window, getFullscreenMode(window).client == FSMODE_FULLSCREEN);
    

    // Note for Vax: I'm not sure why we pass RULE_PROP_FULLSCREENSTATE_CLIENT here since it only checks for internal FS change (i didn't change the logic of this from upstream)?
      // Leaving this as-is for now even though it mentions client inside the setInternal function
    if (!INTERNAL_FS_MODE_CHANGED) {
        window->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_FULLSCREEN | Desktop::Rule::RULE_PROP_FULLSCREENSTATE_CLIENT |
                                                     Desktop::Rule::RULE_PROP_FULLSCREENSTATE_INTERNAL | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
        window->updateDecorationValues();
        g_layoutManager->recalculateMonitor(MONITOR);
        return;
    }


    // Internal mode must be set by the handler; not setting it here because last FS mode should be made available to handlers
    const eFullscreenRequestResult FULLSCREEN_REQUEST_RESULT = WINDOW_FS_HANDLER->requestFullscreen({.target = window->m_target, .currentMode = WINDOW_FS_MODE.internal, .mode = mode});


    if (mode == FSMODE_NONE && window->m_isFloating)
        ALGORITHM->recenter(window->m_target);

    SPACE->recalculate(FULLSCREEN_REQUEST_RESULT == FULLSCREEN_REQUEST_DEFAULT_HANDLED ? Layout::RECALCULATE_REASON_TOGGLE_DEFAULT_HANDLED_FULLSCREEN : Layout::RECALCULATE_REASON_TOGGLE_LAYOUT_HANDLED_FULLSCREEN);





    g_pEventManager->postEvent(SHyprIPCEvent{.event = "fullscreen", .data = std::to_string(sc<int>(mode) != FSMODE_NONE)});
    Event::bus()->m_events.window.fullscreen.emit(window);

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

    const auto FS_HANDLER = getFSHandler(window,layoutAware);
    if (!FS_HANDLER) {
        Log::logger->log(Log::ERR, "window {} doesn't have FS handler assinged. This should never happen", window->m_title);
        return;
    }

    FS_HANDLER->setTargetFullscreenModeClient(window->m_target, mode);

    g_pXWaylandManager->setWindowFullscreen(window, mode == FSMODE_FULLSCREEN);

}




WP<IFullscreenHandler> CFullscreenController::getFSHandler(const PHLWINDOW window, std::optional<bool> layoutHandled) {
    if (!window || !window->m_workspace || !window->m_workspace->m_space || !window->m_workspace->m_space->algorithm() || !window->m_workspace->m_space->algorithm()->floatingAlgo() || !window->m_workspace->m_space->algorithm()->tiledAlgo())
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
