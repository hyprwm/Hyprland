
#include "FullscreenController.hpp"
#include "desktop/DesktopTypes.hpp"
#include "layout/algorithm/Algorithm.hpp"
#include "layout/algorithm/FloatingAlgorithm.hpp"
#include "layout/algorithm/TiledAlgorithm.hpp"
#include "managers/fullscreen/handler/FullscreenHandler.hpp"
#include "output/Monitor.hpp"
#include "render/Renderer.hpp"
#include <optional>

using namespace Fullscreen;




//  ERSTARR - IMPORTANT: Might wanna add a layoutHandled param to isFullscreen to avoid multiple redundant calls in calling both isFullscreen and isLayoutHandledFullscreen.



// Window

bool CFullscreenController::isFullscreen( const PHLWINDOW window, const std::optional<eFullscreenMode> mode, const std::optional<bool> covering) {
    if(!window || !window->m_workspace || !window->m_workspace->m_space || !window->m_workspace->m_space->algorithm()|| mode == FSMODE_NONE)
        return false;


    const auto FS_HANDLER = getFSHandler(window);



    // ERSTARR TEST - getFsHandler w/o layout value passed should correctly get the FS handler the FS window is using , making below code redundant
    // // if window was default handled
    // if (FS_HANDLER->IFullscreenHandler::isFullscreen(window->m_target, mode, covering))
    //     return true;


    // // If window was layout handled
    // if (FS_HANDLER->isFullscreen(window->m_target, mode, covering))
    //     return true;


    // return false;


    return FS_HANDLER->isFullscreen(window->m_target);

}


SFullscreenMode CFullscreenController::getFullscreenMode(const PHLWINDOW window) {
    if(!window || !window->m_workspace || !window->m_workspace->m_space || !window->m_workspace->m_space->algorithm())
        return SFullscreenMode{};

        
    const auto FS_HANDLER = getFSHandler(window);


    return FS_HANDLER->getFullscreenMode(window->m_target);

    // ERSTARR TEST - getFsHandler w/o layout value passed should correctly get the FS handler the FS window is using , making below code redundant
    // // If layout handled
    // if (FS_HANDLER->isFullscreen(window->m_target) || FS_HANDLER->getFullscreenMode(window->m_target).client != FSMODE_NONE)
    //     return FS_HANDLER->getFullscreenMode(window->m_target);

    

    // // if window was default handled
    // return FS_HANDLER->IFullscreenHandler::getFullscreenMode(window->m_target);
}


bool CFullscreenController::isFsManagedByLayoutHandler(const PHLWINDOW window) {
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
    if(!workspace || !workspace->m_space || workspace->m_space->algorithm())
        return false;

    // ASSUMPTION: Floating FS window layers ontop of Tiled Default Handled FS window which layers ontop of Tiled Layout Handled FS window
    // TODO: implement a way to get the topmost FS window so layouts can implement FS behaviour that may layer over Tiled Default Handled FS windows

    const auto TILED_FS_HANDLER  = workspace->m_space->algorithm()->tiledAlgo()->getFSHandler();
    // only one floating algo in hyprland so far, which is default handled.
    const auto FLOATING_FS_HANDLER      = workspace->m_space->algorithm()->floatingAlgo()->getFSHandler();

    // check floating first
    if (FLOATING_FS_HANDLER->hasFullscreen(covering))
        return true;
    
    // check default handled tiled second
    if (TILED_FS_HANDLER->IFullscreenHandler::hasFullscreen(covering))
        return true;

    // check layout handled tiled last
    return TILED_FS_HANDLER->hasFullscreen(covering);
}


PHLWINDOW CFullscreenController::getFullscreenWindow(const PHLWORKSPACE workspace, const std::optional<bool> covering) {
    if(!workspace || !workspace->m_space || workspace->m_space->algorithm())
        return nullptr;

    // ASSUMPTION: Floating FS window layers ontop of Tiled Default Handled FS window which layers ontop of Tiled Layout Handled FS window
    // TODO: implement a way to get the topmost FS window so layouts can implement FS behaviour that may layer over Tiled Default Handled FS windows

    const auto TILED_FS_HANDLER  = workspace->m_space->algorithm()->tiledAlgo()->getFSHandler();
    // only one floating algo in hyprland so far, which is default handled
    const auto FLOATING_FS_HANDLER      = workspace->m_space->algorithm()->floatingAlgo()->getFSHandler();

    // check floating first
    if (const auto FSTARGET = FLOATING_FS_HANDLER->getFullscreen(covering); FSTARGET && FLOATING_FS_HANDLER->isFullscreen(FSTARGET))
        return FSTARGET->window();

    // check default handled tiled second
    if (const auto FSTARGET =  TILED_FS_HANDLER->IFullscreenHandler::getFullscreen(covering); FSTARGET && TILED_FS_HANDLER->IFullscreenHandler::isFullscreen(FSTARGET))
        return FSTARGET->window();

    // check layout handled tiled last
    const auto FSTARGET = TILED_FS_HANDLER->getFullscreen(covering);
    return FSTARGET && TILED_FS_HANDLER->isFullscreen(FSTARGET)? FSTARGET->window() : nullptr;

}
SFullscreenMode CFullscreenController::getFullscreenMode(const PHLWORKSPACE workspace, const std::optional<bool> covering) {
    if(!workspace || !workspace->m_space || workspace->m_space->algorithm())
        return SFullscreenMode{};

    // ASSUMPTION: Floating FS window layers ontop of Tiled Default Handled FS window which layers ontop of Tiled Layout Handled FS window
    // TODO: implement a way to get the topmost FS window so layouts can implement FS behaviour that may layer over Tiled Default Handled FS windows

    const auto TILED_FS_HANDLER  = workspace->m_space->algorithm()->tiledAlgo()->getFSHandler();
    // only one floating algo in hyprland so far, which is default handled
    const auto FLOATING_FS_HANDLER      = workspace->m_space->algorithm()->floatingAlgo()->getFSHandler();

    // check floating first
    if (const auto FSTARGET = FLOATING_FS_HANDLER->getFullscreen(covering); FSTARGET && FLOATING_FS_HANDLER->isFullscreen(FSTARGET))
        return FLOATING_FS_HANDLER->getFullscreenMode(FSTARGET);

    // check default handled tiled second
    if (const auto FSTARGET =  TILED_FS_HANDLER->IFullscreenHandler::getFullscreen(covering); FSTARGET && TILED_FS_HANDLER->IFullscreenHandler::isFullscreen(FSTARGET))
        return TILED_FS_HANDLER->IFullscreenHandler::getFullscreenMode(FSTARGET);

    // check layout handled tiled last
    const auto FSTARGET = TILED_FS_HANDLER->getFullscreen(covering);
    return FSTARGET && TILED_FS_HANDLER->isFullscreen(FSTARGET) ? TILED_FS_HANDLER->getFullscreenMode(FSTARGET) : SFullscreenMode{};
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

SFullscreenMode CFullscreenController::getFullscreenMode(const PHLMONITOR monitor, const std::optional<bool> covering) {
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

    return  getFullscreenMode(activeWorkspace);
}

// Handler

eFullscreenHandler CFullscreenController::getFullscreenHandlerName(const PHLWINDOW window) {
    if(!window || !window->m_workspace || !window->m_workspace->m_space || window->m_workspace->m_space->algorithm())
        return FULLSCREEN_HANDLER_NONE; // ERSTARR TODO - LOG AN ERROR HERE TOO. HANDLER_NONE IS MORE THE DEFAULT 'NO-VALUE'


    // get the layout FS handler - we will upcast.
    // IMPORTANT: no layoutHandled value passed -> no recursion.
    const auto LAYOUT_FS_HANDLER = getFSHandler(window,true);


    eFullscreenHandler handlerName = FULLSCREEN_HANDLER_NONE;

    // if default handled
    if (LAYOUT_FS_HANDLER->IFullscreenHandler::isFullscreen(window->m_target) || LAYOUT_FS_HANDLER->IFullscreenHandler::getFullscreenMode(window->m_target).client != FSMODE_NONE)
        handlerName = LAYOUT_FS_HANDLER->IFullscreenHandler::getFullscreenHandlerName();
    // Layout Handled
    else
    // If a window is not FS at all, we consider its handler to be layout if it is in a workspace with a layout that implements their custom FS behaviour.
     handlerName = LAYOUT_FS_HANDLER->getFullscreenHandlerName();
    


    if (handlerName == FULLSCREEN_HANDLER_NONE)
        return FULLSCREEN_HANDLER_NONE; // ERSTARR TODO - LOG AN ERROR HERE TOO. HANDLER_NONE IS MORE THE DEFAULT 'NO-VALUE'

    return handlerName;


} 







// FS Mode Setters

void CFullscreenController::setFullscreenMode(const PHLWINDOW window, const std::optional<eFullscreenMode> client, const std::optional<eFullscreenMode> internal, std::optional<bool> layoutAware) {
    if (!window)
        return;


    // if new values not provided, we need to use the old values.
    eFullscreenMode targetInternalMode = internal.value_or(FSMODE_NONE);
    eFullscreenMode targetClientMode = client.value_or(FSMODE_NONE);


    // If a window is already FS, get its handler
    // This returns information on if a window is already present in its layout or default handler as internal mode set, or client mode set (or both).
        // Important as we need to detect if a window is at all present in a handler that it will not use in this transaction
    const bool IS_LAYOUT_HANDLED = isFsManagedByLayoutHandler(window);


    const auto FS_HANDLER =  getFSHandler(window,layoutAware.value_or(IS_LAYOUT_HANDLED));

    // If window is FS and is handled differently than before
    if (layoutAware.value_or(IS_LAYOUT_HANDLED) != IS_LAYOUT_HANDLED) {
        // we need to remove the window from its old handler
        const auto ORIGINAL_FS_HANDLER = getFSHandler(window, IS_LAYOUT_HANDLED);
        const auto OLD_FS_MODE = ORIGINAL_FS_HANDLER->getFullscreenMode(window->m_target);

        // save the old values if new ones aren't provided
        targetInternalMode = internal.value_or(OLD_FS_MODE.internal);
        targetClientMode = internal.value_or(OLD_FS_MODE.client);

        // if syncing FS, this guarantees that it will be removed from the handler as both internal and client will be removed
        if (window->m_ruleApplicator->syncFullscreen().valueOrDefault()) {
            setWindowFullscreenModeClient(window, FSMODE_NONE, IS_LAYOUT_HANDLED);
            // ERSTARR TODO - I'LL TRY TO MAKE INTERNAL THE STATE CHANGE FUNCTION - IF IT FAILS, NEED TO CALL STATE FUNCTION HERE (AFTER SETTING INTERNAL MODE OFC)
            setWindowFullscreenModeInternal(window, FSMODE_NONE, IS_LAYOUT_HANDLED, false);
        }
        // if not syncing FS, we need to move the unmodified FS value from the old one to the new one
        else {

            // remove window from the handler
            if (OLD_FS_MODE.internal != FSMODE_NONE)
                setWindowFullscreenModeInternal(window, FSMODE_NONE, IS_LAYOUT_HANDLED, false);
            if (OLD_FS_MODE.client != FSMODE_NONE)
                setWindowFullscreenModeClient(window, FSMODE_NONE, IS_LAYOUT_HANDLED);
            
        }

    }
    // if window is FS and it's handled the same as before or it's not FS at all
    else {
    
        const auto OLD_FS_MODE = FS_HANDLER->getFullscreenMode(window->m_target);

        // save the old values if new ones aren't provided
        targetInternalMode = internal.value_or(OLD_FS_MODE.internal);
        targetClientMode = internal.value_or(OLD_FS_MODE.client);
    }



    // set new FS state in the correct handler
    setWindowFullscreenModeInternal(window, targetInternalMode, layoutAware.value_or(IS_LAYOUT_HANDLED), false);
    setWindowFullscreenModeClient(window, targetClientMode, layoutAware.value_or(IS_LAYOUT_HANDLED));    

}


// ERSTARR TODO -> NEED TO HANDLE THE CASE WHERE A WINDOW IS IN A HANDLER WITH ONLY ITS CLIENT STATE LEFT -- TEST EXTENSIVELY THAT A WINDOW IS NOT STUCK IN A LIST WHERE IT SHOULD NOT BE









// ERSTARR TODO - Check modes. if synced, good.
// if not, set the one that's not synced.
// internal call will dispatch to the full FS pipeline





// ERSTARR TODO - sync the internal and client -> in client dispatches to internal and internal follows the standard FS path

void CFullscreenController::setWindowFullscreenModeInternal(const PHLWINDOW window, const eFullscreenMode mode, bool layoutAware, const bool force) {
    
    if (!window || !validMapped(window) || !window->m_monitor || !window->m_workspace)
        return;

    const auto FS_HANDLER = getFSHandler(window,layoutAware);
    FS_HANDLER->setTargetFullscreenModeInternal(window->layoutTarget(), mode);




    const auto MONITOR   = window->m_monitor.lock();
    const auto WORKSPACE = window->m_workspace;

    // there's no layout managed floating algo.
    const auto              WINDOW_FS_HANDLER     = getFSHandler(window);
    const SFullscreenMode& WINDOW_FS_MODE        = getFullscreenMode(window);
    const bool              WINDOW_IS_INTERNAL_FS          = isFullscreen(window);
    const bool              INTERNAL_FS_MODE_CHANGED = !window->m_pinned && WINDOW_FS_MODE.internal != mode;

    if (!WINDOW_FS_HANDLER)
        return; // ERSTARR TODO - LOG ERROR

    static auto PDIRECTSCANOUT      = CConfigValue<Config::INTEGER>("render:direct_scanout");
    static auto PALLOWPINFULLSCREEN = CConfigValue<Config::INTEGER>("binds:allow_pin_fullscreen");


    


    if (window->m_isFloating && WINDOW_FS_MODE.internal == FSMODE_NONE && mode != FSMODE_NONE)
        g_pHyprRenderer->damageWindow(window);



    if (*PALLOWPINFULLSCREEN && !window->m_pinFullscreened && !WINDOW_IS_INTERNAL_FS && window->m_pinned) {
        window->m_pinned          = false;
        window->m_pinFullscreened = true;
    }



    // this should be redundant now
    // // if there is a FS window that covers the monitor right now, the window that covers the screen is a default handled FS window, and you are FSing(unFS leads to the same result) a window that is not that window
    // if (const auto FSWINDOW = PWORKSPACE->getFullscreenWindow(); FSWINDOW && FSWINDOW != PWINDOW && !FSWINDOW->m_target->layoutManagedFullscreen())
    //     setWindowFullscreenInternal(PWORKSPACE->getFullscreenWindow(), FSMODE_NONE);


    

    if (*PALLOWPINFULLSCREEN && window->m_pinFullscreened && WINDOW_IS_INTERNAL_FS && !window->m_pinned && mode == FSMODE_NONE) {
        window->m_pinned          = true;
        window->m_pinFullscreened = false;
    }


    // this should be redundant now
    // // TODO: update the state on syncFullscreen changes
    // if (!(force || CHANGEINTERNAL) && PWINDOW->m_ruleApplicator->syncFullscreen().valueOrDefault())
    //     return;

    // ERSTARR - THIS CODE IS MOVED TO CLIENT - IT SHOULD HOPEFULLY WORK
    // g_pXWaylandManager->setWindowFullscreen(window, getFullscreenMode(window).client == FSMODE_FULLSCREEN);
    

    // Note for Vax: I'm not sure why we pass RULE_PROP_FULLSCREENSTATE_CLIENT here since it only checks for internal FS change (i didn't change the logic of this from upstream)?
      // Leaving this as-is for now even though it mentions client inside the setInternal function
    if (!(force || INTERNAL_FS_MODE_CHANGED)) {
        window->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_FULLSCREEN | Desktop::Rule::RULE_PROP_FULLSCREENSTATE_CLIENT |
                                                     Desktop::Rule::RULE_PROP_FULLSCREENSTATE_INTERNAL | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
        window->updateDecorationValues();
        g_layoutManager->recalculateMonitor(MONITOR);
        return;
    }



    const eFullscreenRequestResult FULLSCREEN_REQUEST_RESULT = WINDOW_FS_HANDLER->requestFullscreen({.target = window->m_target, .currentMode = WINDOW_FS_MODE.internal, .mode = mode});

    const auto ALGORITHM = window->m_workspace->m_space->algorithm();

    if (mode == FSMODE_NONE && ALGORITHM && window->m_isFloating)
        ALGORITHM->recenter(window->m_target);

    ALGORITHM->recalculate(FULLSCREEN_REQUEST_RESULT == FULLSCREEN_REQUEST_DEFAULT_HANDLED ? Layout::RECALCULATE_REASON_TOGGLE_DEFAULT_HANDLED_FULLSCREEN : Layout::RECALCULATE_REASON_TOGGLE_LAYOUT_HANDLED_FULLSCREEN);


}



void CFullscreenController::setWindowFullscreenModeClient(const PHLWINDOW window, const eFullscreenMode mode, bool layoutAware) { // ERSTARR TODO - FORCE SHOULD BE REDUNADNT HERE
    if (!window || !window->m_workspace || window->m_workspace->m_space || !window->m_workspace->m_space->algorithm())
        return;

    const auto FS_HANDLER = getFSHandler(window,layoutAware);

    FS_HANDLER->setTargetFullscreenModeClient(window->m_target, mode);

    g_pXWaylandManager->setWindowFullscreen(window, mode == FSMODE_FULLSCREEN);

}


// // ERSTARR TODO - BETTER NAME!
// void CFullscreenController::setWindowFullscreenState(const PHLWINDOW window, const SFullscreenMode state, const bool layoutAware, const bool force) {
// }



SP<IFullscreenHandler> CFullscreenController::getFSHandler(const PHLWINDOW window, std::optional<bool> layoutHandled) {

    if (!window || !window->m_workspace || !window->m_workspace->m_space || !window->m_workspace->m_space->algorithm() ||
        (window->m_isFloating ? !window->m_workspace->m_space->algorithm()->floatingAlgo() : !window->m_workspace->m_space->algorithm()->tiledAlgo()))
        return nullptr;
    


    // if not value given, use the handler the window is currently using.
    if (!layoutHandled.has_value()) {
        layoutHandled = isFsManagedByLayoutHandler(window);
    }

    // Layout Handled
    return (layoutHandled ? (window->m_isFloating ? window->m_workspace->m_space->algorithm()->floatingAlgo()->getFSHandler() :
                                                    window->m_workspace->m_space->algorithm()->tiledAlgo()->getFSHandler()) :
                            // Default Handled
                            (window->m_isFloating ? window->m_workspace->m_space->algorithm()->floatingAlgo()->IModeAlgorithm::getFSHandler() :
                                                    window->m_workspace->m_space->algorithm()->tiledAlgo()->IModeAlgorithm::getFSHandler()))
        .lock();
}

// Misc. Operations

// void CFullscreenController::moveFullscreenWindowToWorkspace(const PHLWINDOW window, const PHLWORKSPACE workspace) {

// }
