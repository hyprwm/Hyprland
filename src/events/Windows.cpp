#include "Events.hpp"

#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/InputManager.hpp"
#include "../render/Renderer.hpp"

// ------------------------------------------------------------ //
//  __          _______ _   _ _____   ______          _______   //
//  \ \        / /_   _| \ | |  __ \ / __ \ \        / / ____|  //
//   \ \  /\  / /  | | |  \| | |  | | |  | \ \  /\  / / (___    //
//    \ \/  \/ /   | | | . ` | |  | | |  | |\ \/  \/ / \___ \   //
//     \  /\  /   _| |_| |\  | |__| | |__| | \  /\  /  ____) |  //
//      \/  \/   |_____|_| \_|_____/ \____/   \/  \/  |_____/   //
//                                                              //
// ------------------------------------------------------------ //

void addViewCoords(void* pWindow, int* x, int* y) {
    const auto PWINDOW = (CWindow*)pWindow;
    *x += PWINDOW->m_vRealPosition.goalv().x;
    *y += PWINDOW->m_vRealPosition.goalv().y;
}

void Events::listener_mapWindow(void* owner, void* data) {
    CWindow* PWINDOW = (CWindow*)owner;

    const auto PMONITOR = g_pCompositor->getMonitorFromCursor();
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);
    PWINDOW->m_iMonitorID = PMONITOR->ID;
    PWINDOW->m_bMappedX11 = true;
    PWINDOW->m_iWorkspaceID = PMONITOR->activeWorkspace;
    PWINDOW->m_bIsMapped = true;
    PWINDOW->m_bReadyToDelete = false;
    PWINDOW->m_bFadingOut = false;
    PWINDOW->m_szTitle = g_pXWaylandManager->getTitle(PWINDOW);
    PWINDOW->m_fAlpha = 255.f;

    // checks if the window wants borders and sets the appriopriate flag
    g_pXWaylandManager->checkBorders(PWINDOW);

    const auto PWINDOWSURFACE = g_pXWaylandManager->getWindowSurface(PWINDOW);

    if (!PWINDOWSURFACE) {
        g_pCompositor->m_lWindows.remove(*PWINDOW);
        return;
    }

    if (g_pXWaylandManager->shouldBeFloated(PWINDOW)) {
        PWINDOW->m_bIsFloating = true;
        PWINDOW->m_bRequestsFloat = true;
    }

    if (PWORKSPACE->m_bDefaultFloating)
        PWINDOW->m_bIsFloating = true;

    if (PWORKSPACE->m_bDefaultPseudo) {
        PWINDOW->m_bIsPseudotiled = true;
        wlr_box desiredGeometry = {0};
        g_pXWaylandManager->getGeometryForWindow(PWINDOW, &desiredGeometry);
        PWINDOW->m_vPseudoSize = Vector2D(desiredGeometry.width, desiredGeometry.height);
    }

    if (PWORKSPACE->m_bHasFullscreenWindow && !PWINDOW->m_bIsFloating) {
        const auto PFULLWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);
        g_pLayoutManager->getCurrentLayout()->fullscreenRequestForWindow(PFULLWINDOW);
        g_pXWaylandManager->setWindowFullscreen(PFULLWINDOW, PFULLWINDOW->m_bIsFullscreen);
    }

    // window rules
    const auto WINDOWRULES = g_pConfigManager->getMatchingRules(PWINDOW);

    for (auto& r : WINDOWRULES) {
        if (r.szRule.find("monitor") == 0) {
            try {
                const long int MONITOR = std::stoi(r.szRule.substr(r.szRule.find(" ")));

                if (MONITOR >= (long int)g_pCompositor->m_lMonitors.size() || MONITOR < (long int)0)
                    PWINDOW->m_iMonitorID = 0;
                else
                    PWINDOW->m_iMonitorID = MONITOR;

                PWINDOW->m_iWorkspaceID = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID)->activeWorkspace;

                Debug::log(ERR, "Rule monitor, applying to window %x -> mon: %i, workspace: %i", PWINDOW, PWINDOW->m_iMonitorID, PWINDOW->m_iWorkspaceID);
            } catch (std::exception& e) {
                Debug::log(ERR, "Rule monitor failed, rule: %s -> %s | err: %s", r.szRule.c_str(), r.szValue.c_str(), e.what());
            }
        } else if (r.szRule.find("workspace") == 0) {
            // switch to workspace
            g_pKeybindManager->m_mDispatchers["workspace"](r.szRule.substr(r.szRule.find_first_of(' ') + 1));

            PWINDOW->m_iMonitorID = g_pCompositor->m_pLastMonitor->ID;
            PWINDOW->m_iWorkspaceID = g_pCompositor->m_pLastMonitor->activeWorkspace;

            Debug::log(LOG, "Rule workspace matched by window %x, %s applied.", PWINDOW, r.szValue.c_str());
        } else if (r.szRule.find("float") == 0) {
            PWINDOW->m_bIsFloating = true;
        } else if (r.szRule.find("tile") == 0) {
            PWINDOW->m_bIsFloating = false;
        } else if (r.szRule.find("pseudo") == 0) {
            PWINDOW->m_bIsPseudotiled = true;
        } else if (r.szRule.find("nofocus") == 0) {
            PWINDOW->m_bNoFocus = true;
        } else if (r.szRule.find("opacity") == 0) {
            try {
                PWINDOW->m_sSpecialRenderData.alpha = std::stof(r.szRule.substr(r.szRule.find_first_of(' ') + 1));
            } catch(std::exception& e) {
                Debug::log(ERR, "Opacity rule \"%s\" failed with: %s", r.szRule.c_str(), e.what());
            }
        } else if (r.szRule.find("animation") == 0) {
            auto STYLE = r.szRule.substr(r.szRule.find_first_of(' ') + 1);
            PWINDOW->m_sAdditionalConfigData.animationStyle = STYLE;
        }
    }

    if (PWINDOW->m_bIsFloating) {
        g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(PWINDOW);
        PWINDOW->m_bCreatedOverFullscreen = true;

        // size and move rules
        for (auto& r : WINDOWRULES) {
            if (r.szRule.find("size") == 0) {
                try {
                    const auto VALUE = r.szRule.substr(r.szRule.find(" ") + 1);
                    const auto SIZEXSTR = VALUE.substr(0, VALUE.find(" "));
                    const auto SIZEYSTR = VALUE.substr(VALUE.find(" ") + 1);

                    const auto SIZEX = SIZEXSTR.find('%') == std::string::npos ? std::stoi(SIZEXSTR) : std::stoi(SIZEXSTR.substr(0, SIZEXSTR.length() - 1)) * 0.01f * PMONITOR->vecSize.x;
                    const auto SIZEY = SIZEYSTR.find('%') == std::string::npos ? std::stoi(SIZEYSTR) : std::stoi(SIZEYSTR.substr(0, SIZEYSTR.length() - 1)) * 0.01f * PMONITOR->vecSize.y;

                    Debug::log(LOG, "Rule size, applying to window %x", PWINDOW);

                    PWINDOW->m_vRealSize = Vector2D(SIZEX, SIZEY);
                    g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vRealSize.goalv());
                } catch (...) {
                    Debug::log(LOG, "Rule size failed, rule: %s -> %s", r.szRule.c_str(), r.szValue.c_str());
                }
            } else if (r.szRule.find("move") == 0) {
                try {
                    const auto VALUE = r.szRule.substr(r.szRule.find(" ") + 1);
                    const auto POSXSTR = VALUE.substr(0, VALUE.find(" "));
                    const auto POSYSTR = VALUE.substr(VALUE.find(" ") + 1);

                    const auto POSX = POSXSTR.find('%') == std::string::npos ? std::stoi(POSXSTR) : std::stoi(POSXSTR.substr(0, POSXSTR.length() - 1)) * 0.01f * PMONITOR->vecSize.x;
                    const auto POSY = POSYSTR.find('%') == std::string::npos ? std::stoi(POSYSTR) : std::stoi(POSYSTR.substr(0, POSYSTR.length() - 1)) * 0.01f * PMONITOR->vecSize.y;

                    Debug::log(LOG, "Rule move, applying to window %x", PWINDOW);

                    PWINDOW->m_vRealPosition = Vector2D(POSX, POSY) + PMONITOR->vecPosition;
                } catch (...) {
                    Debug::log(LOG, "Rule move failed, rule: %s -> %s", r.szRule.c_str(), r.szValue.c_str());
                }
            }
        }

        // set the pseudo size to the GOAL of our current size
        // because the windows are animated on RealSize
        PWINDOW->m_vPseudoSize = PWINDOW->m_vRealSize.goalv();
    }
    else {
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(PWINDOW);

        // Set the pseudo size here too so that it doesnt end up being 0x0
        PWINDOW->m_vPseudoSize = PWINDOW->m_vRealSize.goalv() - Vector2D(10,10);
    }

    if (!PWINDOW->m_bNoFocus)
        g_pCompositor->focusWindow(PWINDOW);

    PWINDOW->m_pSurfaceTree = SubsurfaceTree::createTreeRoot(g_pXWaylandManager->getWindowSurface(PWINDOW), addViewCoords, PWINDOW);

    Debug::log(LOG, "Window got assigned a surfaceTreeNode %x", PWINDOW->m_pSurfaceTree);

    if (!PWINDOW->m_bIsX11) {
        PWINDOW->hyprListener_commitWindow.initCallback(&PWINDOW->m_uSurface.xdg->surface->events.commit, &Events::listener_commitWindow, PWINDOW, "XDG Window Late");
        PWINDOW->hyprListener_setTitleWindow.initCallback(&PWINDOW->m_uSurface.xdg->toplevel->events.set_title, &Events::listener_setTitleWindow, PWINDOW, "XDG Window Late");
        PWINDOW->hyprListener_fullscreenWindow.initCallback(&PWINDOW->m_uSurface.xdg->toplevel->events.request_fullscreen, &Events::listener_fullscreenWindow, PWINDOW, "XDG Window Late");
        PWINDOW->hyprListener_newPopupXDG.initCallback(&PWINDOW->m_uSurface.xdg->events.new_popup, &Events::listener_newPopupXDG, PWINDOW, "XDG Window Late");
    } else {
        PWINDOW->hyprListener_fullscreenWindow.initCallback(&PWINDOW->m_uSurface.xwayland->events.request_fullscreen, &Events::listener_fullscreenWindow, PWINDOW, "XWayland Window Late");
        PWINDOW->hyprListener_activateX11.initCallback(&PWINDOW->m_uSurface.xwayland->events.request_activate, &Events::listener_activateX11, PWINDOW, "XWayland Window Late");
        PWINDOW->hyprListener_configureX11.initCallback(&PWINDOW->m_uSurface.xwayland->events.request_configure, &Events::listener_configureX11, PWINDOW, "XWayland Window Late");
        PWINDOW->hyprListener_setTitleWindow.initCallback(&PWINDOW->m_uSurface.xwayland->events.set_title, &Events::listener_setTitleWindow, PWINDOW, "XWayland Window Late");
    }

    // do the animation thing
    g_pAnimationManager->onWindowPostCreate(PWINDOW);

    Debug::log(LOG, "Map request dispatched, monitor %s, xywh: %f %f %f %f", PMONITOR->szName.c_str(), PWINDOW->m_vRealPosition.goalv().x, PWINDOW->m_vRealPosition.goalv().y, PWINDOW->m_vRealSize.goalv().x, PWINDOW->m_vRealSize.goalv().y);
}

void Events::listener_unmapWindow(void* owner, void* data) {
    CWindow* PWINDOW = (CWindow*)owner;

    Debug::log(LOG, "Window %x unmapped", PWINDOW);

    if (!PWINDOW->m_bIsX11) {
        Debug::log(LOG, "Unregistered late callbacks XDG: %x %x %x %x", &PWINDOW->hyprListener_commitWindow.m_sListener.link, &PWINDOW->hyprListener_setTitleWindow.m_sListener.link, &PWINDOW->hyprListener_fullscreenWindow.m_sListener.link, &PWINDOW->hyprListener_newPopupXDG.m_sListener.link);
        PWINDOW->hyprListener_commitWindow.removeCallback();
        PWINDOW->hyprListener_setTitleWindow.removeCallback();
        PWINDOW->hyprListener_fullscreenWindow.removeCallback();
        PWINDOW->hyprListener_newPopupXDG.removeCallback();
    } else {
        Debug::log(LOG, "Unregistered late callbacks XWL: %x %x %x %x", &PWINDOW->hyprListener_fullscreenWindow.m_sListener.link, &PWINDOW->hyprListener_activateX11.m_sListener.link, &PWINDOW->hyprListener_configureX11.m_sListener.link, &PWINDOW->hyprListener_setTitleWindow.m_sListener.link);
        PWINDOW->hyprListener_fullscreenWindow.removeCallback();
        PWINDOW->hyprListener_activateX11.removeCallback();
        PWINDOW->hyprListener_configureX11.removeCallback();
        PWINDOW->hyprListener_setTitleWindow.removeCallback();
    }

    // Allow the renderer to catch the last frame.
    g_pHyprOpenGL->makeWindowSnapshot(PWINDOW);

    if (PWINDOW == g_pCompositor->m_pLastWindow) {
        g_pCompositor->m_pLastWindow = nullptr;
        g_pCompositor->m_pLastFocus = nullptr;
    }

    PWINDOW->m_fAlpha = 0.f;

    PWINDOW->m_bMappedX11 = false;

    // remove the fullscreen window status from workspace if we closed it
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow && PWINDOW->m_bIsFullscreen)
        PWORKSPACE->m_bHasFullscreenWindow = false;

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(PWINDOW);

    // do this after onWindowRemoved because otherwise it'll think the window is invalid
    PWINDOW->m_bIsMapped = false;

    // refocus on a new window
    g_pInputManager->refocus();

    Debug::log(LOG, "Destroying the SubSurface tree of unmapped window %x", PWINDOW);
    SubsurfaceTree::destroySurfaceTree(PWINDOW->m_pSurfaceTree);
    
    PWINDOW->m_pSurfaceTree = nullptr;

    PWINDOW->m_bFadingOut = true;

    g_pCompositor->m_lWindowsFadingOut.push_back(PWINDOW);

    g_pHyprRenderer->damageMonitor(g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID));
}

void Events::listener_commitWindow(void* owner, void* data) {
    CWindow* PWINDOW = (CWindow*)owner;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    // Debug::log(LOG, "Window %x committed", PWINDOW); // SPAM!
}

void Events::listener_destroyWindow(void* owner, void* data) {
    CWindow* PWINDOW = (CWindow*)owner;

    Debug::log(LOG, "Window %x destroyed, queueing.", PWINDOW);

    if (PWINDOW == g_pCompositor->m_pLastWindow) {
        g_pCompositor->m_pLastWindow = nullptr;
        g_pCompositor->m_pLastFocus = nullptr;
    }

    PWINDOW->hyprListener_mapWindow.removeCallback();
    PWINDOW->hyprListener_unmapWindow.removeCallback();
    PWINDOW->hyprListener_destroyWindow.removeCallback();

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(PWINDOW);

    if (PWINDOW->m_pSurfaceTree) {
        Debug::log(LOG, "Destroying Subsurface tree of %x in destroyWindow", PWINDOW);
        SubsurfaceTree::destroySurfaceTree(PWINDOW->m_pSurfaceTree);
        PWINDOW->m_pSurfaceTree = nullptr;
    }

    PWINDOW->m_bReadyToDelete = true;
}

void Events::listener_setTitleWindow(void* owner, void* data) {
    CWindow* PWINDOW = (CWindow*)owner;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
	    return;

    PWINDOW->m_szTitle = g_pXWaylandManager->getTitle(PWINDOW);

    if (PWINDOW == g_pCompositor->m_pLastWindow) // if it's the active, let's post an event to update others
        g_pEventManager->postEvent(SHyprIPCEvent("activewindow", PWINDOW->m_szTitle));

    Debug::log(LOG, "Window %x set title to %s", PWINDOW, PWINDOW->m_szTitle.c_str());
}

void Events::listener_fullscreenWindow(void* owner, void* data) {
    CWindow* PWINDOW = (CWindow*)owner;

    if (!PWINDOW->m_bIsX11) {
        const auto REQUESTED = &PWINDOW->m_uSurface.xdg->toplevel->requested;

        if (REQUESTED->fullscreen != PWINDOW->m_bIsFullscreen)
            g_pLayoutManager->getCurrentLayout()->fullscreenRequestForWindow(PWINDOW);
    } else {
        g_pLayoutManager->getCurrentLayout()->fullscreenRequestForWindow(PWINDOW);
    }

    Debug::log(LOG, "Window %x fullscreen to %i", PWINDOW, PWINDOW->m_bIsFullscreen);
    
    g_pXWaylandManager->setWindowFullscreen(PWINDOW, PWINDOW->m_bIsFullscreen);
}

void Events::listener_activate(void* owner, void* data) {
    // TODO
}

void Events::listener_activateX11(void* owner, void* data) {
    CWindow* PWINDOW = (CWindow*)owner;

    if (PWINDOW->m_iX11Type == 1 /* Managed */) {
        wlr_xwayland_surface_activate(PWINDOW->m_uSurface.xwayland, 1);
    }
}

void Events::listener_configureX11(void* owner, void* data) {
    CWindow* PWINDOW = (CWindow*)owner;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    const auto E = (wlr_xwayland_surface_configure_event*)data;
    g_pHyprRenderer->damageWindow(PWINDOW);

    if (!PWINDOW->m_bIsFloating) {
        g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vRealSize.vec());
        g_pInputManager->refocus();
        g_pHyprRenderer->damageWindow(PWINDOW);
        return;
    }

    if (!PWINDOW->m_uSurface.xwayland->mapped) {
        wlr_xwayland_surface_configure(PWINDOW->m_uSurface.xwayland, E->x, E->y, E->width, E->height);
        return;
    }

    PWINDOW->m_vRealPosition.setValueAndWarp(Vector2D(E->x, E->y));
    PWINDOW->m_vRealSize.setValueAndWarp(Vector2D(E->width, E->height));
    PWINDOW->m_vPosition = PWINDOW->m_vRealPosition.vec();
    PWINDOW->m_vSize = PWINDOW->m_vRealSize.vec();

    wlr_xwayland_surface_configure(PWINDOW->m_uSurface.xwayland, E->x, E->y, E->width, E->height);

    g_pCompositor->moveWindowToTop(PWINDOW);

    PWINDOW->m_bCreatedOverFullscreen = true;

    g_pInputManager->refocus();

    g_pHyprRenderer->damageWindow(PWINDOW);
}

void Events::listener_surfaceXWayland(wl_listener* listener, void* data) {
    const auto XWSURFACE = (wlr_xwayland_surface*)data;

    Debug::log(LOG, "New XWayland Surface created.");

    g_pCompositor->m_lWindows.emplace_back();
    const auto PNEWWINDOW = &g_pCompositor->m_lWindows.back();

    PNEWWINDOW->m_uSurface.xwayland = XWSURFACE;
    PNEWWINDOW->m_iX11Type = XWSURFACE->override_redirect ? 2 : 1;
    PNEWWINDOW->m_bIsX11 = true;

    PNEWWINDOW->hyprListener_mapWindow.initCallback(&XWSURFACE->events.map, &Events::listener_mapWindow, PNEWWINDOW, "XWayland Window");
    PNEWWINDOW->hyprListener_unmapWindow.initCallback(&XWSURFACE->events.unmap, &Events::listener_unmapWindow, PNEWWINDOW, "XWayland Window");
    PNEWWINDOW->hyprListener_destroyWindow.initCallback(&XWSURFACE->events.destroy, &Events::listener_destroyWindow, PNEWWINDOW, "XWayland Window");
}

void Events::listener_newXDGSurface(wl_listener* listener, void* data) {
    // A window got opened
    const auto XDGSURFACE = (wlr_xdg_surface*)data;

    Debug::log(LOG, "New XDG Surface created. (%ix%i at %i %i)", XDGSURFACE->current.geometry.width, XDGSURFACE->current.geometry.height, XDGSURFACE->current.geometry.x, XDGSURFACE->current.geometry.y);

    if (XDGSURFACE->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
        return;  // TODO: handle?

    g_pCompositor->m_lWindows.emplace_back();
    const auto PNEWWINDOW = &g_pCompositor->m_lWindows.back();
    PNEWWINDOW->m_uSurface.xdg = XDGSURFACE;

    PNEWWINDOW->hyprListener_mapWindow.initCallback(&XDGSURFACE->events.map, &Events::listener_mapWindow, PNEWWINDOW, "XDG Window");
    PNEWWINDOW->hyprListener_unmapWindow.initCallback(&XDGSURFACE->events.unmap, &Events::listener_unmapWindow, PNEWWINDOW, "XDG Window");
    PNEWWINDOW->hyprListener_destroyWindow.initCallback(&XDGSURFACE->events.destroy, &Events::listener_destroyWindow, PNEWWINDOW, "XDG Window");
}

void Events::listener_NewXDGDeco(wl_listener* listener, void* data) {
    const auto WLRDECO = (wlr_xdg_toplevel_decoration_v1*)data;
    wlr_xdg_toplevel_decoration_v1_set_mode(WLRDECO, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}
