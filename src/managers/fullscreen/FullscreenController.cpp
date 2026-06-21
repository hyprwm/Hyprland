
#include "FullscreenController.hpp"
#include "desktop/DesktopTypes.hpp"
#include "layout/algorithm/Algorithm.hpp"
#include "layout/algorithm/FloatingAlgorithm.hpp"
#include "layout/algorithm/TiledAlgorithm.hpp"
#include "managers/fullscreen/handler/FullscreenHandler.hpp"
#include "output/Monitor.hpp"
#include "render/Renderer.hpp"

using namespace Fullscreen;




//  ERSTARR - IMPORTANT: Might wanna add a layoutHandled param to isFullscreen to avoid multiple redundant calls in calling both isFullscreen and isLayoutHandledFullscreen.



// Window

bool CFullscreenController::isFullscreen( const PHLWINDOW window, const std::optional<eFullscreenMode> mode, const std::optional<bool> covering) {
    if(!window || !window->m_workspace || !window->m_workspace->m_space || !window->m_workspace->m_space->algorithm()|| mode == FSMODE_NONE)
        return false;


    

    // If window was layout handled
    const bool LAYOUT_HANDLER_RESULT = window->m_isFloating ? window->m_workspace->m_space->algorithm()->floatingAlgo()->getFSHandler()->isFullscreen(window->m_target, mode, covering) : window->m_workspace->m_space->algorithm()->tiledAlgo()->getFSHandler()->isFullscreen(window->m_target, mode, covering);
        if (LAYOUT_HANDLER_RESULT)
            return LAYOUT_HANDLER_RESULT;

    // if window was default handled explicitly
    const bool DEFAULT_HANDLER_RESULT = window->m_isFloating ? window->m_workspace->m_space->algorithm()->floatingAlgo()->getFSHandler()->IFullscreenHandler::isFullscreen(window->m_target, mode, covering) : window->m_workspace->m_space->algorithm()->tiledAlgo()->getFSHandler()->IFullscreenHandler::isFullscreen(window->m_target, mode, covering);

    return DEFAULT_HANDLER_RESULT;

}

bool CFullscreenController::isLayoutManagedFullscreen(const PHLWINDOW window) {
    if(!window || !window->m_workspace || !window->m_workspace->m_space || !window->m_workspace->m_space->algorithm())
        return false;


    // If a window is not FS at all, we consider its handler to be layout if it is in a workspace with a layout that implements their custom FS behaviour.
    if (!isFullscreen(window)) {

        // TODO change this if hyprland gets layout handled floating FS windows
        return window->m_workspace->m_space->algorithm()->tiledAlgo()->getFSHandler()->getFullscreenHandlerName() != 1;
    }


    const bool LAYOUT_HANDLER_RESULT = window->m_isFloating ? window->m_workspace->m_space->algorithm()->floatingAlgo()->getFSHandler()->isFullscreen(window->m_target) : window->m_workspace->m_space->algorithm()->tiledAlgo()->getFSHandler()->isFullscreen(window->m_target);
        if (LAYOUT_HANDLER_RESULT)
            return (window->m_isFloating ? window->m_workspace->m_space->algorithm()->floatingAlgo()->getFSHandler()->getFullscreenHandlerName() : window->m_workspace->m_space->algorithm()->tiledAlgo()->getFSHandler()->getFullscreenHandlerName()) > 1;

    // if window was default handled explicitly
    return (window->m_isFloating ? window->m_workspace->m_space->algorithm()->floatingAlgo()->getFSHandler()->IFullscreenHandler::getFullscreenHandlerName() : window->m_workspace->m_space->algorithm()->tiledAlgo()->getFSHandler()->IFullscreenHandler::getFullscreenHandlerName()) > 1;
}

SFullscreenMode CFullscreenController::getFullscreenMode(const PHLWINDOW window) {
    if(!window || !window->m_workspace || !window->m_workspace->m_space || !window->m_workspace->m_space->algorithm())
        return SFullscreenMode{};


    const bool LAYOUT_HANDLER_RESULT = window->m_isFloating ? window->m_workspace->m_space->algorithm()->floatingAlgo()->getFSHandler()->isFullscreen(window->m_target) : window->m_workspace->m_space->algorithm()->tiledAlgo()->getFSHandler()->isFullscreen(window->m_target);
        if (LAYOUT_HANDLER_RESULT)
            return window->m_isFloating ? window->m_workspace->m_space->algorithm()->floatingAlgo()->getFSHandler()->getFullscreenMode(window->m_target) : window->m_workspace->m_space->algorithm()->tiledAlgo()->getFSHandler()->getFullscreenMode(window->m_target);

    // if window was default handled explicitly
    return window->m_isFloating ? window->m_workspace->m_space->algorithm()->floatingAlgo()->getFSHandler()->IFullscreenHandler::getFullscreenMode(window->m_target) : window->m_workspace->m_space->algorithm()->tiledAlgo()->getFSHandler()->IFullscreenHandler::getFullscreenMode(window->m_target);
}


// Workspace

bool CFullscreenController::hasFullscreen(const PHLWORKSPACE workspace, const std::optional<bool> covering) {
    if(!workspace || !workspace->m_space || workspace->m_space->algorithm())
        return false;

    // ASSUMPTION: Floating FS window layers ontop of Tiled Default Handled FS window which layers ontop of Tiled Layout Handled FS window
    // TODO: implement a way to get the topmost FS window so layouts can implement FS behaviour that may layer over Tiled Default Handled FS windows

    const auto TILED_LAYOUT_FS_HANDLER  = workspace->m_space->algorithm()->tiledAlgo()->getFSHandler();
    const auto TILED_DEFAULT_FS_HANDLER = workspace->m_space->algorithm()->tiledAlgo()->IModeAlgorithm::getFSHandler();
    // only one floating algo in hyprland so far, which is default handled
    const auto FLOATING_FS_HANDLER      = workspace->m_space->algorithm()->floatingAlgo()->getFSHandler();

    // check floating first
    if (FLOATING_FS_HANDLER->hasFullscreen(covering))
        return true;

    // check default handled tiled second
    if (TILED_DEFAULT_FS_HANDLER->hasFullscreen(covering))
        return true;

    // check layout handled tiled last
    return TILED_LAYOUT_FS_HANDLER->hasFullscreen(covering);
}


// ERSTARR TODO - if covering is true; need to check if floating algo has FS first, THEN the default handler of a layout handler. ONLY after that check the layout handler.
PHLWINDOW CFullscreenController::getFullscreenWindow(const PHLWORKSPACE workspace, const std::optional<bool> covering) {
    if(!workspace || !workspace->m_space || workspace->m_space->algorithm())
        return nullptr;

    // ASSUMPTION: Floating FS window layers ontop of Tiled Default Handled FS window which layers ontop of Tiled Layout Handled FS window
    // TODO: implement a way to get the topmost FS window so layouts can implement FS behaviour that may layer over Tiled Default Handled FS windows

    const auto TILED_LAYOUT_FS_HANDLER  = workspace->m_space->algorithm()->tiledAlgo()->getFSHandler();
    const auto TILED_DEFAULT_FS_HANDLER = workspace->m_space->algorithm()->tiledAlgo()->IModeAlgorithm::getFSHandler();
    // only one floating algo in hyprland so far, which is default handled
    const auto FLOATING_FS_HANDLER      = workspace->m_space->algorithm()->floatingAlgo()->getFSHandler();

    // check floating first
    if (const auto FSTARGET = FLOATING_FS_HANDLER->getFullscreen(covering); FSTARGET)
        return FSTARGET->window();

    // check default handled tiled second
    if (const auto FSTARGET =  TILED_DEFAULT_FS_HANDLER->getFullscreen(covering); FSTARGET)
        return FSTARGET->window();

    // check layout handled tiled last
    const auto FSTARGET = TILED_LAYOUT_FS_HANDLER->getFullscreen(covering);
    return FSTARGET ? FSTARGET->window() : nullptr;

}
SFullscreenMode CFullscreenController::getFullscreenMode(const PHLWORKSPACE workspace, const std::optional<bool> covering) {
    if(!workspace || !workspace->m_space || workspace->m_space->algorithm())
        return SFullscreenMode{};

    // ASSUMPTION: Floating FS window layers ontop of Tiled Default Handled FS window which layers ontop of Tiled Layout Handled FS window
    // TODO: implement a way to get the topmost FS window so layouts can implement FS behaviour that may layer over Tiled Default Handled FS windows

    const auto TILED_LAYOUT_FS_HANDLER  = workspace->m_space->algorithm()->tiledAlgo()->getFSHandler();
    const auto TILED_DEFAULT_FS_HANDLER = workspace->m_space->algorithm()->tiledAlgo()->IModeAlgorithm::getFSHandler();
    // only one floating algo in hyprland so far, which is default handled
    const auto FLOATING_FS_HANDLER      = workspace->m_space->algorithm()->floatingAlgo()->getFSHandler();

    // check floating first
    if (const auto FSTARGET = FLOATING_FS_HANDLER->getFullscreen(covering); FSTARGET)
        return FLOATING_FS_HANDLER->getFullscreenMode(FSTARGET);

    // check default handled tiled second
    if (const auto FSTARGET =  TILED_DEFAULT_FS_HANDLER->getFullscreen(covering); FSTARGET)
        return TILED_DEFAULT_FS_HANDLER->getFullscreenMode(FSTARGET);

    // check layout handled tiled last
    const auto FSTARGET = TILED_LAYOUT_FS_HANDLER->getFullscreen(covering);
    return FSTARGET ? TILED_LAYOUT_FS_HANDLER->getFullscreenMode(FSTARGET) : SFullscreenMode{};
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

// CHECK the floating first. After that, check the default handler base class in layout handler. After that check the layout handler.
eFullscreenHandler CFullscreenController::getFullscreenHandlerName(const PHLWINDOW window) {
    if(!window || !window->m_workspace || !window->m_workspace->m_space || window->m_workspace->m_space->algorithm())
        return FULLSCREEN_HANDLER_NONE; // ERSTARR TODO - LOG AN ERROR HERE TOO. HANDLER_NONE IS MORE THE DEFAULT 'NO-VALUE'


    // If a window is not FS at all
    if (!isFullscreen(window))
        return FULLSCREEN_HANDLER_NONE; // ERSTARR TODO - LOG AN ERROR HERE TOO. HANDLER_NONE IS MORE THE DEFAULT 'NO-VALUE'


    const bool LAYOUT_HANDLER_RESULT = window->m_isFloating ? window->m_workspace->m_space->algorithm()->floatingAlgo()->getFSHandler()->isFullscreen(window->m_target) : window->m_workspace->m_space->algorithm()->tiledAlgo()->getFSHandler()->isFullscreen(window->m_target);
        if (LAYOUT_HANDLER_RESULT)
            return (window->m_isFloating ? window->m_workspace->m_space->algorithm()->floatingAlgo()->getFSHandler()->getFullscreenHandlerName() : window->m_workspace->m_space->algorithm()->tiledAlgo()->getFSHandler()->getFullscreenHandlerName());

    // if window was default handled explicitly
    return (window->m_isFloating ? window->m_workspace->m_space->algorithm()->floatingAlgo()->getFSHandler()->IFullscreenHandler::getFullscreenHandlerName() : window->m_workspace->m_space->algorithm()->tiledAlgo()->getFSHandler()->IFullscreenHandler::getFullscreenHandlerName());

} 







// FS Mode Setters















// ERSTARR TODO - Check modes. if synced, good.
// if not, set the one that's not synced.
// internal call will dispatch to the full FS pipeline



// This is done in the controller, not the handler. hanlers do as they are told (set internal/client)
// TODO ERSTARR -> this is the syncFullscreen rule. this is to be handled HERE. set client first, then internal.
// if (WINDOW->m_ruleApplicator->syncFullscreen().valueOrDefault()) {
//     setWindowFullscreenModeInternal(WINDOW, request.mode);
//     setWindowFullscreenModeClient(WINDOW, request.mode);
// }





// ERSTARR TODO - sync the internal and client -> in client dispatches to internal and internal follows the standard FS path

void CFullscreenController::setWindowFullscreenModeInternal(const PHLWINDOW window, const eFullscreenMode mode, bool layoutAware, const bool force) {
    if (!window)
        return;


    const eFullscreenHandler WINDOW_FS_HANDLER_NAME = getFullscreenHandlerName(window);

    if (WINDOW_FS_HANDLER_NAME == FULLSCREEN_HANDLER_NONE) // FULLSCREEN_HANDLER_NONE == 0
        return; // ERSTARR TODO - LOG AN ERROR HERE - FAILED FS_INTERNAL

    // We simply compare the eFullscreenHandler of the window's handler to decide if it's layout handled instead of a call to isLayoutManagedFullscreen() since isLayoutManagedFullscreen does the same thing.
    // If isLayoutManagedFullscreen() is changed, that change must be reflected here
    const bool WINDOW_WAS_LAYOUT_FS_HANDLED = WINDOW_FS_HANDLER_NAME > 1;



    const bool WINDOW_IS_FS = isFullscreen(window);

    // unFSing a window
    if ((mode == FSMODE_NONE && WINDOW_IS_FS) ||
        // non-layoutAware operations on a window that was layoutAware FSed and vice versa
        (WINDOW_IS_FS && layoutAware != WINDOW_WAS_LAYOUT_FS_HANDLED)) {

        // We must use the FS handler that was used to FS the window
        layoutAware = WINDOW_WAS_LAYOUT_FS_HANDLED;
    }

    // At this point the window must either be non-FS, or be FS with matching layoutAware status between current request and the request that FSed it

    // FSing a window using the same mode and layoutAware will execute like we are FSing an unFSed window instead of early return. Purpose is to allow users to recover from errors in FS state, size, etc...
    // by "just doing it again". This is hardly a programatic error recovery option, just allows the user to "try again to see if it works this time"


    if (window->m_ruleApplicator->syncFullscreen().valueOrDefault()) {
        setWindowFullscreenModeClient(window, mode, layoutAware);
        setWindowFullscreenState(window, {.internal = mode, .client = mode}, layoutAware , false);
    }


    setWindowFullscreenState(window, {.internal=mode, .client=getFullscreenMode(window).client}, layoutAware , false);

}



void CFullscreenController::setWindowFullscreenModeClient(const PHLWINDOW window, const eFullscreenMode mode, bool layoutAware) { // ERSTARR TODO - FORCE SHOULD BE REDUNADNT HERE
    if (!window || !window->m_workspace || window->m_workspace->m_space || !window->m_workspace->m_space->algorithm())
        return;


    const auto WINDOW_FS_HANDLER = window->m_isFloating ? window->m_workspace->m_space->algorithm()->floatingAlgo()->getFSHandler() : (layoutAware ? window->m_workspace->m_space->algorithm()->tiledAlgo()->getFSHandler() : window->m_workspace->m_space->algorithm()->tiledAlgo()->IModeAlgorithm::getFSHandler());
    
    WINDOW_FS_HANDLER->setTargetFullscreenModeClient(window->m_target, mode);
}


// ERSTARR TODO - BETTER NAME!
void CFullscreenController::setWindowFullscreenState(const PHLWINDOW window, const SFullscreenMode state, const bool layoutAware, const bool force) {

    if (!validMapped(window) || !window->m_monitor || !window->m_workspace)
        return; // ERSTARR - LOG ERROR / OR DEBUG

    const auto MONITOR   = window->m_monitor.lock();
    const auto WORKSPACE = window->m_workspace;

    // there's no layout managed floating algo.
    const auto              WINDOW_FS_HANDLER     = window->m_isFloating ? window->m_workspace->m_space->algorithm()->floatingAlgo()->getFSHandler() :
                                                                           (layoutAware ? window->m_workspace->m_space->algorithm()->tiledAlgo()->getFSHandler() :
                                                                                          window->m_workspace->m_space->algorithm()->tiledAlgo()->IModeAlgorithm::getFSHandler());
    const SFullscreenMode& WINDOW_FS_MODE        = getFullscreenMode(window);
    const bool              WINDOW_IS_FS          = isFullscreen(window);
    const bool              INTERNAL_FS_MODE_CHANGED = !window->m_pinned && WINDOW_FS_MODE.internal != state.internal;

    if (!WINDOW_FS_HANDLER)
        return; // ERSTARR TODO - LOG ERROR

    static auto PDIRECTSCANOUT      = CConfigValue<Config::INTEGER>("render:direct_scanout");
    static auto PALLOWPINFULLSCREEN = CConfigValue<Config::INTEGER>("binds:allow_pin_fullscreen");


    


    if (window->m_isFloating && WINDOW_FS_MODE.internal == FSMODE_NONE && state.internal != FSMODE_NONE)
        g_pHyprRenderer->damageWindow(window);



    if (*PALLOWPINFULLSCREEN && !window->m_pinFullscreened && !WINDOW_IS_FS && window->m_pinned) {
        window->m_pinned          = false;
        window->m_pinFullscreened = true;
    }



    // this should be redundant now
    // // if there is a FS window that covers the monitor right now, the window that covers the screen is a default handled FS window, and you are FSing(unFS leads to the same result) a window that is not that window
    // if (const auto FSWINDOW = PWORKSPACE->getFullscreenWindow(); FSWINDOW && FSWINDOW != PWINDOW && !FSWINDOW->m_target->layoutManagedFullscreen())
    //     setWindowFullscreenInternal(PWORKSPACE->getFullscreenWindow(), FSMODE_NONE);


    

    if (*PALLOWPINFULLSCREEN && window->m_pinFullscreened && WINDOW_IS_FS && !window->m_pinned && state.internal == FSMODE_NONE) {
        window->m_pinned          = true;
        window->m_pinFullscreened = false;
    }


    // this should be redundant now
    // // TODO: update the state on syncFullscreen changes
    // if (!(force || CHANGEINTERNAL) && PWINDOW->m_ruleApplicator->syncFullscreen().valueOrDefault())
    //     return;



    g_pXWaylandManager->setWindowFullscreen(window, state.client == FSMODE_FULLSCREEN);

    

    if (!(force || INTERNAL_FS_MODE_CHANGED)) {
        window->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_FULLSCREEN | Desktop::Rule::RULE_PROP_FULLSCREENSTATE_CLIENT |
                                                     Desktop::Rule::RULE_PROP_FULLSCREENSTATE_INTERNAL | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
        window->updateDecorationValues();
        g_layoutManager->recalculateMonitor(MONITOR);
        return;
    }



    const eFullscreenRequestResult FULLSCREEN_REQUEST_RESULT = WINDOW_FS_HANDLER->requestFullscreen({.target = window->m_target, .currentMode = WINDOW_FS_MODE.internal, .mode = state.internal});

    const auto ALGORITHM = window->m_workspace->m_space->algorithm();

    if (state.internal == FSMODE_NONE && ALGORITHM && window->m_isFloating)
        ALGORITHM->recenter(window->m_target);

    ALGORITHM->recalculate(FULLSCREEN_REQUEST_RESULT == FULLSCREEN_REQUEST_DEFAULT_HANDLED ? Layout::RECALCULATE_REASON_TOGGLE_DEFAULT_HANDLED_FULLSCREEN : Layout::RECALCULATE_REASON_TOGGLE_LAYOUT_HANDLED_FULLSCREEN);



}










// Misc. Operations

void CFullscreenController::moveFullscreenWindowToWorkspace(const PHLWINDOW window, const PHLWORKSPACE workspace) {

}
