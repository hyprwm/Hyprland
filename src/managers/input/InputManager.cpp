#include "InputManager.hpp"
#include "../../Compositor.hpp"
#include "wlr/types/wlr_switch.h"
#include <ranges>

void CInputManager::onMouseMoved(wlr_pointer_motion_event* e) {
    static auto* const PSENS      = &g_pConfigManager->getConfigValuePtr("general:sensitivity")->floatValue;
    static auto* const PNOACCEL   = &g_pConfigManager->getConfigValuePtr("input:force_no_accel")->intValue;
    static auto* const PSENSTORAW = &g_pConfigManager->getConfigValuePtr("general:apply_sens_to_raw")->intValue;

    const auto         DELTA = *PNOACCEL == 1 ? Vector2D(e->unaccel_dx, e->unaccel_dy) : Vector2D(e->delta_x, e->delta_y);

    if (*PSENSTORAW == 1)
        wlr_relative_pointer_manager_v1_send_relative_motion(g_pCompositor->m_sWLRRelPointerMgr, g_pCompositor->m_sSeat.seat, (uint64_t)e->time_msec * 1000, DELTA.x * *PSENS,
                                                             DELTA.y * *PSENS, e->unaccel_dx * *PSENS, e->unaccel_dy * *PSENS);
    else
        wlr_relative_pointer_manager_v1_send_relative_motion(g_pCompositor->m_sWLRRelPointerMgr, g_pCompositor->m_sSeat.seat, (uint64_t)e->time_msec * 1000, DELTA.x, DELTA.y,
                                                             e->unaccel_dx, e->unaccel_dy);

    wlr_cursor_move(g_pCompositor->m_sWLRCursor, &e->pointer->base, DELTA.x * *PSENS, DELTA.y * *PSENS);

    mouseMoveUnified(e->time_msec);

    m_tmrLastCursorMovement.reset();

    m_bLastInputTouch = false;
}

void CInputManager::onMouseWarp(wlr_pointer_motion_absolute_event* e) {
    wlr_cursor_warp_absolute(g_pCompositor->m_sWLRCursor, &e->pointer->base, e->x, e->y);

    mouseMoveUnified(e->time_msec);

    m_tmrLastCursorMovement.reset();

    m_bLastInputTouch = false;
}

void CInputManager::simulateMouseMovement() {
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    m_vLastCursorPosFloored = m_vLastCursorPosFloored - Vector2D(1, 1); // hack: force the mouseMoveUnified to report without making this a refocus.
    mouseMoveUnified(now.tv_sec * 1000 + now.tv_nsec / 10000000);
}

void CInputManager::sendMotionEventsToFocused() {
    if (!g_pCompositor->m_pLastFocus)
        return;

    // todo: this sucks ass
    const auto PWINDOW = g_pCompositor->getWindowFromSurface(g_pCompositor->m_pLastFocus);
    const auto PLS     = g_pCompositor->getLayerSurfaceFromSurface(g_pCompositor->m_pLastFocus);

    timespec   now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    const auto LOCAL = getMouseCoordsInternal() - (PWINDOW ? PWINDOW->m_vRealPosition.goalv() : (PLS ? Vector2D{PLS->geometry.x, PLS->geometry.y} : Vector2D{}));

    wlr_seat_pointer_notify_enter(g_pCompositor->m_sSeat.seat, g_pCompositor->m_pLastFocus, LOCAL.x, LOCAL.y);
    wlr_seat_pointer_notify_motion(g_pCompositor->m_sSeat.seat, now.tv_sec * 1000 + now.tv_nsec / 10000000, LOCAL.x, LOCAL.y);
}

void CInputManager::mouseMoveUnified(uint32_t time, bool refocus) {
    static auto* const PFOLLOWMOUSE      = &g_pConfigManager->getConfigValuePtr("input:follow_mouse")->intValue;
    static auto* const PMOUSEREFOCUS     = &g_pConfigManager->getConfigValuePtr("input:mouse_refocus")->intValue;
    static auto* const PMOUSEDPMS        = &g_pConfigManager->getConfigValuePtr("misc:mouse_move_enables_dpms")->intValue;
    static auto* const PFOLLOWONDND      = &g_pConfigManager->getConfigValuePtr("misc:always_follow_on_dnd")->intValue;
    static auto* const PHOGFOCUS         = &g_pConfigManager->getConfigValuePtr("misc:layers_hog_keyboard_focus")->intValue;
    static auto* const PFLOATBEHAVIOR    = &g_pConfigManager->getConfigValuePtr("input:float_switch_override_focus")->intValue;
    static auto* const PMOUSEFOCUSMON    = &g_pConfigManager->getConfigValuePtr("misc:mouse_move_focuses_monitor")->intValue;
    static auto* const PRESIZEONBORDER   = &g_pConfigManager->getConfigValuePtr("general:resize_on_border")->intValue;
    static auto* const PRESIZECURSORICON = &g_pConfigManager->getConfigValuePtr("general:hover_icon_on_border")->intValue;
    static auto* const PZOOMFACTOR       = &g_pConfigManager->getConfigValuePtr("misc:cursor_zoom_factor")->floatValue;

    const auto         FOLLOWMOUSE = *PFOLLOWONDND && m_sDrag.drag ? 1 : *PFOLLOWMOUSE;

    m_pFoundSurfaceToFocus      = nullptr;
    m_pFoundLSToFocus           = nullptr;
    m_pFoundWindowToFocus       = nullptr;
    wlr_surface*   foundSurface = nullptr;
    Vector2D       surfaceCoords;
    Vector2D       surfacePos         = Vector2D(-1337, -1337);
    CWindow*       pFoundWindow       = nullptr;
    SLayerSurface* pFoundLayerSurface = nullptr;

    if (!g_pCompositor->m_bReadyToProcess || g_pCompositor->m_bIsShuttingDown || g_pCompositor->m_bUnsafeState)
        return;

    if (!g_pCompositor->m_bDPMSStateON && *PMOUSEDPMS) {
        // enable dpms
        g_pKeybindManager->dpms("on");
    }

    Vector2D   mouseCoords        = getMouseCoordsInternal();
    const auto MOUSECOORDSFLOORED = mouseCoords.floor();

    if (MOUSECOORDSFLOORED == m_vLastCursorPosFloored && !refocus)
        return;

    EMIT_HOOK_EVENT_CANCELLABLE("mouseMove", MOUSECOORDSFLOORED);

    if (time)
        g_pCompositor->notifyIdleActivity();

    m_vLastCursorPosFloored = MOUSECOORDSFLOORED;

    const auto PMONITOR = g_pCompositor->getMonitorFromCursor();

    // this can happen if there are no displays hooked up to Hyprland
    if (PMONITOR == nullptr)
        return;

    if (*PZOOMFACTOR != 1.f)
        g_pHyprRenderer->damageMonitor(PMONITOR);

    if (!PMONITOR->solitaryClient && g_pHyprRenderer->shouldRenderCursor())
        g_pCompositor->scheduleFrameForMonitor(PMONITOR);

    CWindow* forcedFocus = m_pForcedFocus;

    if (!forcedFocus)
        forcedFocus = g_pCompositor->getForceFocus();

    if (forcedFocus) {
        pFoundWindow = forcedFocus;
        surfacePos   = pFoundWindow->m_vRealPosition.vec();
        foundSurface = pFoundWindow->m_pWLSurface.wlr();
    }

    // constraints
    // All constraints TODO: multiple mice?
    if (g_pCompositor->m_sSeat.mouse && g_pCompositor->m_sSeat.mouse->currentConstraint && !g_pCompositor->m_sSeat.exclusiveClient && !g_pSessionLockManager->isSessionLocked()) {
        // XWayland windows sometimes issue constraints weirdly.
        // TODO: We probably should search their parent. wlr_xwayland_surface->parent
        const auto CONSTRAINTWINDOW = g_pCompositor->getConstraintWindow(g_pCompositor->m_sSeat.mouse);
        const auto PCONSTRAINT      = constraintFromWlr(g_pCompositor->m_sSeat.mouse->currentConstraint);

        if (!CONSTRAINTWINDOW || !PCONSTRAINT) {
            unconstrainMouse();
        } else {
            // Native Wayland apps know how 2 constrain themselves.
            // XWayland, we just have to accept them. Might cause issues, but thats XWayland for ya.
            const auto CONSTRAINTPOS  = PCONSTRAINT->getLogicConstraintPos();
            const auto CONSTRAINTSIZE = PCONSTRAINT->getLogicConstraintSize();

            if (g_pCompositor->m_sSeat.mouse->currentConstraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED) {
                // we just snap the cursor to where it should be.

                if (PCONSTRAINT->hintSet)
                    wlr_cursor_warp_closest(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, CONSTRAINTPOS.x + PCONSTRAINT->positionHint.x,
                                            CONSTRAINTPOS.y + PCONSTRAINT->positionHint.y);

                return; // don't process anything else, the cursor is locked. The surface should not receive any further events.
                        // these are usually FPS games. They will use the relative motion.
            } else {
                // we restrict the cursor to the confined region
                const auto REGION = PCONSTRAINT->getLogicCoordsRegion();

                if (!REGION.containsPoint(mouseCoords)) {
                    if (g_pCompositor->m_sSeat.mouse->constraintActive) {
                        const auto CLOSEST = REGION.closestPoint(mouseCoords);
                        wlr_cursor_warp_closest(g_pCompositor->m_sWLRCursor, NULL, CLOSEST.x, CLOSEST.y);
                        mouseCoords = getMouseCoordsInternal();
                    }
                } else {
                    if ((!CONSTRAINTWINDOW->m_bIsX11 && PMONITOR && CONSTRAINTWINDOW->m_iWorkspaceID == PMONITOR->activeWorkspace) || (CONSTRAINTWINDOW->m_bIsX11)) {
                        g_pCompositor->m_sSeat.mouse->constraintActive = true;
                    }
                }
            }

            if (CONSTRAINTWINDOW->m_bIsX11) {
                foundSurface = CONSTRAINTWINDOW->m_pWLSurface.wlr();
                surfacePos   = CONSTRAINTWINDOW->m_vRealPosition.vec();
            } else {
                g_pCompositor->vectorWindowToSurface(mouseCoords, CONSTRAINTWINDOW, surfaceCoords);
            }

            pFoundWindow = CONSTRAINTWINDOW;
        }
    }

    // update stuff
    updateDragIcon();

    if (!m_sDrag.drag && !m_lCurrentlyHeldButtons.empty() && g_pCompositor->m_pLastFocus) {
        if (m_bLastFocusOnLS) {
            foundSurface       = m_pLastMouseSurface;
            pFoundLayerSurface = g_pCompositor->getLayerSurfaceFromSurface(foundSurface);
            if (pFoundLayerSurface) {
                surfacePos              = g_pCompositor->getLayerSurfaceFromSurface(foundSurface)->position;
                m_bFocusHeldByButtons   = true;
                m_bRefocusHeldByButtons = refocus;
            } else {
                // ?
                foundSurface       = nullptr;
                pFoundLayerSurface = nullptr;
            }
        } else if (g_pCompositor->m_pLastWindow) {
            foundSurface = m_pLastMouseSurface;
            pFoundWindow = g_pCompositor->m_pLastWindow;

            surfaceCoords = g_pCompositor->vectorToSurfaceLocal(mouseCoords, pFoundWindow, foundSurface);

            m_bFocusHeldByButtons   = true;
            m_bRefocusHeldByButtons = refocus;
        }
    }

    g_pLayoutManager->getCurrentLayout()->onMouseMove(getMouseCoordsInternal());

    if (PMONITOR && PMONITOR != g_pCompositor->m_pLastMonitor && (*PMOUSEFOCUSMON || refocus))
        g_pCompositor->setActiveMonitor(PMONITOR);

    if (g_pSessionLockManager->isSessionLocked()) {
        const auto PSLS = PMONITOR ? g_pSessionLockManager->getSessionLockSurfaceForMonitor(PMONITOR->ID) : nullptr;

        if (!PSLS)
            return;

        foundSurface = PSLS->pWlrLockSurface->surface;
        surfacePos   = PMONITOR->vecPosition;
    }

    // overlay is above fullscreen
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], &surfaceCoords, &pFoundLayerSurface);

    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], &surfaceCoords, &pFoundLayerSurface);

    // then, we check if the workspace doesnt have a fullscreen window
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);
    if (PWORKSPACE->m_bHasFullscreenWindow && !foundSurface && PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL) {
        pFoundWindow = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);

        if (!pFoundWindow) {
            // what the fuck, somehow happens occasionally??
            PWORKSPACE->m_bHasFullscreenWindow = false;
            return;
        }

        const auto PWINDOWIDEAL = g_pCompositor->vectorToWindowIdeal(mouseCoords);

        if (PWINDOWIDEAL &&
            ((PWINDOWIDEAL->m_bIsFloating && PWINDOWIDEAL->m_bCreatedOverFullscreen) /* floating over fullscreen */
             || (PMONITOR->specialWorkspaceID == PWINDOWIDEAL->m_iWorkspaceID) /* on an open special workspace */))
            pFoundWindow = PWINDOWIDEAL;

        if (!pFoundWindow->m_bIsX11) {
            foundSurface = g_pCompositor->vectorWindowToSurface(mouseCoords, pFoundWindow, surfaceCoords);
            surfacePos   = Vector2D(-1337, -1337);
        } else {
            foundSurface = pFoundWindow->m_pWLSurface.wlr();
            surfacePos   = pFoundWindow->m_vRealPosition.vec();
        }
    }

    // then windows
    if (!foundSurface) {
        if (PWORKSPACE->m_bHasFullscreenWindow && PWORKSPACE->m_efFullscreenMode == FULLSCREEN_MAXIMIZED) {

            if (PMONITOR->specialWorkspaceID) {
                pFoundWindow = g_pCompositor->vectorToWindowIdeal(mouseCoords);

                if (pFoundWindow && !g_pCompositor->isWorkspaceSpecial(pFoundWindow->m_iWorkspaceID)) {
                    pFoundWindow = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);
                }
            } else {
                pFoundWindow = g_pCompositor->vectorToWindowIdeal(mouseCoords);

                if (!(pFoundWindow && pFoundWindow->m_bIsFloating && pFoundWindow->m_bCreatedOverFullscreen))
                    pFoundWindow = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);
            }
        } else {
            pFoundWindow = g_pCompositor->vectorToWindowIdeal(mouseCoords);
        }

        if (pFoundWindow) {
            if (!pFoundWindow->m_bIsX11) {
                foundSurface = g_pCompositor->vectorWindowToSurface(mouseCoords, pFoundWindow, surfaceCoords);
            } else {
                foundSurface = pFoundWindow->m_pWLSurface.wlr();
                surfacePos   = pFoundWindow->m_vRealPosition.vec();
            }
        }
    }

    // then surfaces below
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM], &surfaceCoords, &pFoundLayerSurface);

    if (!foundSurface)
        foundSurface =
            g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND], &surfaceCoords, &pFoundLayerSurface);

    g_pCompositor->scheduleFrameForMonitor(g_pCompositor->m_pLastMonitor);

    if (!foundSurface) {
        if (!m_bEmptyFocusCursorSet) {
            m_eBorderIconDirection = BORDERICON_NONE;
            if (g_pHyprRenderer->m_bHasARenderedCursor) {
                // TODO: maybe wrap?
                if (m_ecbClickBehavior == CLICKMODE_KILL)
                    g_pHyprRenderer->setCursorFromName("crosshair");
                else
                    g_pHyprRenderer->setCursorFromName("left_ptr");
            }

            m_bEmptyFocusCursorSet = true;
        }

        wlr_seat_pointer_clear_focus(g_pCompositor->m_sSeat.seat);

        if (refocus || !g_pCompositor->m_pLastWindow) // if we are forcing a refocus, and we don't find a surface, clear the kb focus too!
            g_pCompositor->focusWindow(nullptr);

        return;
    }

    m_bEmptyFocusCursorSet = false;

    Vector2D surfaceLocal = surfacePos == Vector2D(-1337, -1337) ? surfaceCoords : mouseCoords - surfacePos;

    if (pFoundWindow && !pFoundWindow->m_bIsX11 && surfacePos != Vector2D(-1337, -1337)) {
        // calc for oversized windows... fucking bullshit.
        wlr_box geom;
        wlr_xdg_surface_get_geometry(pFoundWindow->m_uSurface.xdg, &geom);

        surfaceLocal = mouseCoords - surfacePos + Vector2D(geom.x, geom.y);
    }

    if (pFoundWindow && pFoundWindow->m_bIsX11) // for x11 force scale zero
        surfaceLocal = surfaceLocal * pFoundWindow->m_fX11SurfaceScaledBy;

    bool allowKeyboardRefocus = true;

    if (!refocus && g_pCompositor->m_pLastFocus) {
        const auto PLS = g_pCompositor->getLayerSurfaceFromSurface(g_pCompositor->m_pLastFocus);

        if (PLS && PLS->layerSurface->current.keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
            allowKeyboardRefocus = false;
    }

    // set the values for use
    if (refocus) {
        m_pFoundLSToFocus      = pFoundLayerSurface;
        m_pFoundWindowToFocus  = pFoundWindow;
        m_pFoundSurfaceToFocus = foundSurface;
    }

    if (currentlyDraggedWindow && pFoundWindow != currentlyDraggedWindow) {
        wlr_seat_pointer_notify_enter(g_pCompositor->m_sSeat.seat, foundSurface, surfaceLocal.x, surfaceLocal.y);
        wlr_seat_pointer_notify_motion(g_pCompositor->m_sSeat.seat, time, surfaceLocal.x, surfaceLocal.y);
        return;
    }

    if (pFoundWindow) {
        // change cursor icon if hovering over border
        if (*PRESIZEONBORDER && *PRESIZECURSORICON) {
            if (!pFoundWindow->m_bIsFullscreen && !pFoundWindow->hasPopupAt(mouseCoords)) {
                setCursorIconOnBorder(pFoundWindow);
            } else if (m_eBorderIconDirection != BORDERICON_NONE) {
                unsetCursorImage();
            }
        }

        if (FOLLOWMOUSE != 1 && !refocus) {
            if (pFoundWindow != g_pCompositor->m_pLastWindow && g_pCompositor->m_pLastWindow &&
                ((pFoundWindow->m_bIsFloating && *PFLOATBEHAVIOR == 2) || (g_pCompositor->m_pLastWindow->m_bIsFloating != pFoundWindow->m_bIsFloating && *PFLOATBEHAVIOR != 0))) {
                // enter if change floating style
                if (FOLLOWMOUSE != 3 && allowKeyboardRefocus)
                    g_pCompositor->focusWindow(pFoundWindow, foundSurface);
                m_pLastMouseSurface = foundSurface;
                wlr_seat_pointer_notify_enter(g_pCompositor->m_sSeat.seat, foundSurface, surfaceLocal.x, surfaceLocal.y);
            } else if (FOLLOWMOUSE == 2 || FOLLOWMOUSE == 3) {
                m_pLastMouseSurface = foundSurface;
                wlr_seat_pointer_notify_enter(g_pCompositor->m_sSeat.seat, foundSurface, surfaceLocal.x, surfaceLocal.y);
            }

            if (pFoundWindow == g_pCompositor->m_pLastWindow) {
                if (foundSurface != g_pCompositor->m_pLastFocus || m_bLastFocusOnLS) {
                    //      ^^^ changed the subsurface                  ^^^ came back from a LS
                    m_pLastMouseSurface = foundSurface;
                    wlr_seat_pointer_notify_enter(g_pCompositor->m_sSeat.seat, foundSurface, surfaceLocal.x, surfaceLocal.y);
                }
            }

            if (FOLLOWMOUSE != 0 || pFoundWindow == g_pCompositor->m_pLastWindow)
                wlr_seat_pointer_notify_motion(g_pCompositor->m_sSeat.seat, time, surfaceLocal.x, surfaceLocal.y);

            m_bLastFocusOnLS = false;
            return; // don't enter any new surfaces
        } else {
            if (((FOLLOWMOUSE != 3 && allowKeyboardRefocus) && (*PMOUSEREFOCUS || m_pLastMouseFocus != pFoundWindow)) || refocus) {
                m_pLastMouseFocus = pFoundWindow;
                g_pCompositor->focusWindow(pFoundWindow, foundSurface);
            }
        }

        m_bLastFocusOnLS = false;
    } else {
        if (*PRESIZEONBORDER && *PRESIZECURSORICON && m_eBorderIconDirection != BORDERICON_NONE) {
            m_eBorderIconDirection = BORDERICON_NONE;
            unsetCursorImage();
        }

        if (pFoundLayerSurface &&
            (pFoundLayerSurface->layerSurface->current.keyboard_interactive != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE ||
             (pFoundLayerSurface->layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP && !g_pCompositor->m_pLastWindow)) &&
            FOLLOWMOUSE != 3 && allowKeyboardRefocus) {
            g_pCompositor->focusSurface(foundSurface);
        }

        if (pFoundLayerSurface)
            m_bLastFocusOnLS = true;
    }

    m_pLastMouseSurface = foundSurface;
    wlr_seat_pointer_notify_enter(g_pCompositor->m_sSeat.seat, foundSurface, surfaceLocal.x, surfaceLocal.y);
    wlr_seat_pointer_notify_motion(g_pCompositor->m_sSeat.seat, time, surfaceLocal.x, surfaceLocal.y);
}

void CInputManager::onMouseButton(wlr_pointer_button_event* e) {
    EMIT_HOOK_EVENT_CANCELLABLE("mouseButton", e);

    g_pCompositor->notifyIdleActivity();

    m_tmrLastCursorMovement.reset();

    if (e->state == WLR_BUTTON_PRESSED) {
        m_lCurrentlyHeldButtons.push_back(e->button);
    } else {
        if (std::find_if(m_lCurrentlyHeldButtons.begin(), m_lCurrentlyHeldButtons.end(), [&](const auto& other) { return other == e->button; }) == m_lCurrentlyHeldButtons.end())
            return;
        std::erase_if(m_lCurrentlyHeldButtons, [&](const auto& other) { return other == e->button; });
    }

    switch (m_ecbClickBehavior) {
        case CLICKMODE_DEFAULT: processMouseDownNormal(e); break;
        case CLICKMODE_KILL: processMouseDownKill(e); break;
        default: break;
    }

    if (m_bFocusHeldByButtons && m_lCurrentlyHeldButtons.empty() && e->state == WLR_BUTTON_RELEASED) {
        if (m_bRefocusHeldByButtons)
            refocus();
        else
            simulateMouseMovement();

        m_bFocusHeldByButtons   = false;
        m_bRefocusHeldByButtons = false;
    }
}

void CInputManager::processMouseRequest(wlr_seat_pointer_request_set_cursor_event* e) {

    if (!e->surface)
        g_pHyprRenderer->m_bWindowRequestedCursorHide = true;
    else
        g_pHyprRenderer->m_bWindowRequestedCursorHide = false;

    if (!cursorImageUnlocked() || !g_pHyprRenderer->m_bHasARenderedCursor)
        return;

    // cursorSurfaceInfo.pSurface = e->surface;

    // if (e->surface) {
    //     hyprListener_CursorSurfaceDestroy.removeCallback();
    //     hyprListener_CursorSurfaceDestroy.initCallback(
    //         &e->surface->events.destroy, [&](void* owner, void* data) { cursorSurfaceInfo.pSurface = nullptr; }, this, "InputManager");
    //     cursorSurfaceInfo.vHotspot = {e->hotspot_x, e->hotspot_y};
    // }

    if (e->seat_client == g_pCompositor->m_sSeat.seat->pointer_state.focused_client)
        g_pHyprRenderer->setCursorSurface(e->surface, e->hotspot_x, e->hotspot_y);
}

void CInputManager::processMouseRequest(wlr_cursor_shape_manager_v1_request_set_shape_event* e) {
    if (!g_pHyprRenderer->m_bHasARenderedCursor || !cursorImageUnlocked())
        return;

    if (e->seat_client == g_pCompositor->m_sSeat.seat->pointer_state.focused_client)
        g_pHyprRenderer->setCursorFromName(wlr_cursor_shape_v1_name(e->shape));
}

bool CInputManager::cursorImageUnlocked() {
    if (!g_pHyprRenderer->m_bHasARenderedCursor)
        return false;

    if (m_ecbClickBehavior == CLICKMODE_KILL)
        return false;

    if (m_bCursorImageOverridden)
        return false;

    return true;
}

eClickBehaviorMode CInputManager::getClickMode() {
    return m_ecbClickBehavior;
}

void CInputManager::setClickMode(eClickBehaviorMode mode) {
    switch (mode) {
        case CLICKMODE_DEFAULT:
            Debug::log(LOG, "SetClickMode: DEFAULT");
            m_ecbClickBehavior = CLICKMODE_DEFAULT;
            g_pHyprRenderer->setCursorFromName("left_ptr");
            break;

        case CLICKMODE_KILL:
            Debug::log(LOG, "SetClickMode: KILL");
            m_ecbClickBehavior = CLICKMODE_KILL;

            // remove constraints
            g_pInputManager->unconstrainMouse();
            refocus();

            // set cursor
            g_pHyprRenderer->setCursorFromName("crosshair");
            break;
        default: break;
    }
}

void CInputManager::processMouseDownNormal(wlr_pointer_button_event* e) {

    // notify the keybind manager
    static auto* const PPASSMOUSE      = &g_pConfigManager->getConfigValuePtr("binds:pass_mouse_when_bound")->intValue;
    const auto         PASS            = g_pKeybindManager->onMouseEvent(e);
    static auto* const PFOLLOWMOUSE    = &g_pConfigManager->getConfigValuePtr("input:follow_mouse")->intValue;
    static auto* const PRESIZEONBORDER = &g_pConfigManager->getConfigValuePtr("general:resize_on_border")->intValue;

    if (!PASS && !*PPASSMOUSE)
        return;

    const auto mouseCoords = g_pInputManager->getMouseCoordsInternal();
    const auto w           = g_pCompositor->vectorToWindowIdeal(mouseCoords);

    if (w && !w->m_bIsFullscreen && !w->hasPopupAt(mouseCoords) && w->m_sGroupData.pNextWindow) {
        const wlr_box box = w->getDecorationByType(DECORATION_GROUPBAR)->getWindowDecorationRegion().getExtents();
        if (wlr_box_contains_point(&box, mouseCoords.x, mouseCoords.y)) {
            if (e->state == WLR_BUTTON_PRESSED) {
                const int SIZE    = w->getGroupSize();
                CWindow*  pWindow = w->getGroupWindowByIndex((mouseCoords.x - box.x) * SIZE / box.width);
                if (w != pWindow)
                    w->setGroupCurrent(pWindow);
            }
            return;
        }
    }

    // clicking on border triggers resize
    // TODO detect click on LS properly
    if (*PRESIZEONBORDER && !m_bLastFocusOnLS) {
        if (w && !w->m_bIsFullscreen) {
            const wlr_box real = {w->m_vRealPosition.vec().x, w->m_vRealPosition.vec().y, w->m_vRealSize.vec().x, w->m_vRealSize.vec().y};
            if ((!wlr_box_contains_point(&real, mouseCoords.x, mouseCoords.y) || w->isInCurvedCorner(mouseCoords.x, mouseCoords.y)) && !w->hasPopupAt(mouseCoords)) {
                g_pKeybindManager->resizeWithBorder(e);
                return;
            }
        }
    }

    switch (e->state) {
        case WLR_BUTTON_PRESSED:
            if (*PFOLLOWMOUSE == 3) // don't refocus on full loose
                break;

            if ((!g_pCompositor->m_sSeat.mouse || !g_pCompositor->m_sSeat.mouse->currentConstraint) /* No constraints */
                && (w && g_pCompositor->m_pLastWindow != w) /* window should change */) {
                // a bit hacky
                // if we only pressed one button, allow us to refocus. m_lCurrentlyHeldButtons.size() > 0 will stick the focus
                if (m_lCurrentlyHeldButtons.size() == 1) {
                    const auto COPY = m_lCurrentlyHeldButtons;
                    m_lCurrentlyHeldButtons.clear();
                    refocus();
                    m_lCurrentlyHeldButtons = COPY;
                } else
                    refocus();
            }

            // if clicked on a floating window make it top
            if (g_pCompositor->m_pLastWindow && g_pCompositor->m_pLastWindow->m_bIsFloating)
                g_pCompositor->changeWindowZOrder(g_pCompositor->m_pLastWindow, true);

            break;
        case WLR_BUTTON_RELEASED: break;
    }

    // notify app if we didnt handle it
    if (g_pCompositor->doesSeatAcceptInput(g_pCompositor->m_pLastFocus)) {
        wlr_seat_pointer_notify_button(g_pCompositor->m_sSeat.seat, e->time_msec, e->button, e->state);
    }
}

void CInputManager::processMouseDownKill(wlr_pointer_button_event* e) {
    switch (e->state) {
        case WLR_BUTTON_PRESSED: {
            const auto PWINDOW = g_pCompositor->vectorToWindowIdeal(getMouseCoordsInternal());

            if (!PWINDOW) {
                Debug::log(ERR, "Cannot kill invalid window!");
                break;
            }

            // kill the mf
            kill(PWINDOW->getPID(), SIGKILL);
            break;
        }
        case WLR_BUTTON_RELEASED: break;
        default: break;
    }

    // reset click behavior mode
    m_ecbClickBehavior = CLICKMODE_DEFAULT;
}

void CInputManager::onMouseWheel(wlr_pointer_axis_event* e) {
    static auto* const PSCROLLFACTOR      = &g_pConfigManager->getConfigValuePtr("input:touchpad:scroll_factor")->floatValue;
    static auto* const PGROUPBARSCROLLING = &g_pConfigManager->getConfigValuePtr("group:groupbar:scrolling")->intValue;

    auto               factor = (*PSCROLLFACTOR <= 0.f || e->source != WLR_AXIS_SOURCE_FINGER ? 1.f : *PSCROLLFACTOR);

    bool               passEvent = g_pKeybindManager->onAxisEvent(e);

    g_pCompositor->notifyIdleActivity();

    const auto MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();
    const auto pWindow     = g_pCompositor->vectorToWindowIdeal(MOUSECOORDS);

    if (*PGROUPBARSCROLLING && pWindow && !pWindow->m_bIsFullscreen && !pWindow->hasPopupAt(MOUSECOORDS) && pWindow->m_sGroupData.pNextWindow) {
        const wlr_box box = pWindow->getDecorationByType(DECORATION_GROUPBAR)->getWindowDecorationRegion().getExtents();
        if (wlr_box_contains_point(&box, MOUSECOORDS.x, MOUSECOORDS.y)) {
            if (e->delta > 0)
                pWindow->setGroupCurrent(pWindow->m_sGroupData.pNextWindow);
            else
                pWindow->setGroupCurrent(pWindow->getGroupPrevious());
            return;
        }
    }

    if (passEvent)
        wlr_seat_pointer_notify_axis(g_pCompositor->m_sSeat.seat, e->time_msec, e->orientation, factor * e->delta, std::round(factor * e->delta_discrete), e->source);
}

Vector2D CInputManager::getMouseCoordsInternal() {
    return Vector2D(g_pCompositor->m_sWLRCursor->x, g_pCompositor->m_sWLRCursor->y);
}

void CInputManager::newKeyboard(wlr_input_device* keyboard) {
    const auto PNEWKEYBOARD = &m_lKeyboards.emplace_back();

    PNEWKEYBOARD->keyboard = keyboard;

    try {
        PNEWKEYBOARD->name = getNameForNewDevice(keyboard->name);
    } catch (std::exception& e) {
        Debug::log(ERR, "Keyboard had no name???"); // logic error
    }

    PNEWKEYBOARD->hyprListener_keyboardMod.initCallback(&wlr_keyboard_from_input_device(keyboard)->events.modifiers, &Events::listener_keyboardMod, PNEWKEYBOARD, "Keyboard");
    PNEWKEYBOARD->hyprListener_keyboardKey.initCallback(&wlr_keyboard_from_input_device(keyboard)->events.key, &Events::listener_keyboardKey, PNEWKEYBOARD, "Keyboard");
    PNEWKEYBOARD->hyprListener_keyboardDestroy.initCallback(&keyboard->events.destroy, &Events::listener_keyboardDestroy, PNEWKEYBOARD, "Keyboard");

    PNEWKEYBOARD->hyprListener_keyboardKeymap.initCallback(
        &wlr_keyboard_from_input_device(keyboard)->events.keymap,
        [&](void* owner, void* data) {
            const auto PKEYBOARD = (SKeyboard*)owner;
            const auto LAYOUT    = getActiveLayoutForKeyboard(PKEYBOARD);

            g_pEventManager->postEvent(SHyprIPCEvent{"activelayout", PKEYBOARD->name + "," + LAYOUT});
            EMIT_HOOK_EVENT("activeLayout", (std::vector<void*>{PKEYBOARD, (void*)&LAYOUT}));
        },
        PNEWKEYBOARD, "Keyboard");

    disableAllKeyboards(false);

    m_pActiveKeyboard = PNEWKEYBOARD;

    PNEWKEYBOARD->active = true;

    applyConfigToKeyboard(PNEWKEYBOARD);

    wlr_seat_set_keyboard(g_pCompositor->m_sSeat.seat, wlr_keyboard_from_input_device(keyboard));

    Debug::log(LOG, "New keyboard created, pointers Hypr: {:x} and WLR: {:x}", (uintptr_t)PNEWKEYBOARD, (uintptr_t)keyboard);
}

void CInputManager::newVirtualKeyboard(wlr_input_device* keyboard) {
    const auto PNEWKEYBOARD = &m_lKeyboards.emplace_back();

    PNEWKEYBOARD->keyboard  = keyboard;
    PNEWKEYBOARD->isVirtual = true;

    try {
        PNEWKEYBOARD->name = getNameForNewDevice(keyboard->name);
    } catch (std::exception& e) {
        Debug::log(ERR, "Keyboard had no name???"); // logic error
    }

    PNEWKEYBOARD->hyprListener_keyboardMod.initCallback(&wlr_keyboard_from_input_device(keyboard)->events.modifiers, &Events::listener_keyboardMod, PNEWKEYBOARD, "Keyboard");
    PNEWKEYBOARD->hyprListener_keyboardKey.initCallback(&wlr_keyboard_from_input_device(keyboard)->events.key, &Events::listener_keyboardKey, PNEWKEYBOARD, "Keyboard");
    PNEWKEYBOARD->hyprListener_keyboardDestroy.initCallback(&keyboard->events.destroy, &Events::listener_keyboardDestroy, PNEWKEYBOARD, "Keyboard");
    PNEWKEYBOARD->hyprListener_keyboardKeymap.initCallback(
        &wlr_keyboard_from_input_device(keyboard)->events.keymap,
        [&](void* owner, void* data) {
            const auto PKEYBOARD = (SKeyboard*)owner;
            const auto LAYOUT    = getActiveLayoutForKeyboard(PKEYBOARD);

            g_pEventManager->postEvent(SHyprIPCEvent{"activelayout", PKEYBOARD->name + "," + LAYOUT});
            EMIT_HOOK_EVENT("activeLayout", (std::vector<void*>{PKEYBOARD, (void*)&LAYOUT}));
        },
        PNEWKEYBOARD, "Keyboard");

    disableAllKeyboards(true);

    m_pActiveKeyboard = PNEWKEYBOARD;

    PNEWKEYBOARD->active = true;

    applyConfigToKeyboard(PNEWKEYBOARD);

    wlr_seat_set_keyboard(g_pCompositor->m_sSeat.seat, wlr_keyboard_from_input_device(keyboard));

    Debug::log(LOG, "New virtual keyboard created, pointers Hypr: {:x} and WLR: {:x}", (uintptr_t)PNEWKEYBOARD, (uintptr_t)keyboard);
}

void CInputManager::setKeyboardLayout() {
    for (auto& k : m_lKeyboards)
        applyConfigToKeyboard(&k);

    g_pKeybindManager->updateXKBTranslationState();
}

void CInputManager::applyConfigToKeyboard(SKeyboard* pKeyboard) {
    auto       devname = pKeyboard->name;

    const auto HASCONFIG = g_pConfigManager->deviceConfigExists(devname);

    Debug::log(LOG, "ApplyConfigToKeyboard for \"{}\", hasconfig: {}", pKeyboard->name, (int)HASCONFIG);

    ASSERT(pKeyboard);

    if (!wlr_keyboard_from_input_device(pKeyboard->keyboard))
        return;

    const auto REPEATRATE  = g_pConfigManager->getDeviceInt(devname, "repeat_rate", "input:repeat_rate");
    const auto REPEATDELAY = g_pConfigManager->getDeviceInt(devname, "repeat_delay", "input:repeat_delay");

    const auto NUMLOCKON = g_pConfigManager->getDeviceInt(devname, "numlock_by_default", "input:numlock_by_default");

    const auto FILEPATH = g_pConfigManager->getDeviceString(devname, "kb_file", "input:kb_file");
    const auto RULES    = g_pConfigManager->getDeviceString(devname, "kb_rules", "input:kb_rules");
    const auto MODEL    = g_pConfigManager->getDeviceString(devname, "kb_model", "input:kb_model");
    const auto LAYOUT   = g_pConfigManager->getDeviceString(devname, "kb_layout", "input:kb_layout");
    const auto VARIANT  = g_pConfigManager->getDeviceString(devname, "kb_variant", "input:kb_variant");
    const auto OPTIONS  = g_pConfigManager->getDeviceString(devname, "kb_options", "input:kb_options");

    const auto ENABLED = HASCONFIG ? g_pConfigManager->getDeviceInt(devname, "enabled") : true;

    pKeyboard->enabled = ENABLED;

    try {
        if (NUMLOCKON == pKeyboard->numlockOn && REPEATDELAY == pKeyboard->repeatDelay && REPEATRATE == pKeyboard->repeatRate && RULES != "" &&
            RULES == pKeyboard->currentRules.rules && MODEL == pKeyboard->currentRules.model && LAYOUT == pKeyboard->currentRules.layout &&
            VARIANT == pKeyboard->currentRules.variant && OPTIONS == pKeyboard->currentRules.options && FILEPATH == pKeyboard->xkbFilePath) {
            Debug::log(LOG, "Not applying config to keyboard, it did not change.");
            return;
        }
    } catch (std::exception& e) {
        // can be libc errors for null std::string
        // we can ignore those and just apply
    }

    wlr_keyboard_set_repeat_info(wlr_keyboard_from_input_device(pKeyboard->keyboard), std::max(0, REPEATRATE), std::max(0, REPEATDELAY));

    pKeyboard->repeatDelay = REPEATDELAY;
    pKeyboard->repeatRate  = REPEATRATE;
    pKeyboard->numlockOn   = NUMLOCKON;
    pKeyboard->xkbFilePath = FILEPATH.c_str();

    xkb_rule_names rules = {.rules = RULES.c_str(), .model = MODEL.c_str(), .layout = LAYOUT.c_str(), .variant = VARIANT.c_str(), .options = OPTIONS.c_str()};

    pKeyboard->currentRules.rules   = RULES;
    pKeyboard->currentRules.model   = MODEL;
    pKeyboard->currentRules.variant = VARIANT;
    pKeyboard->currentRules.options = OPTIONS;
    pKeyboard->currentRules.layout  = LAYOUT;

    const auto CONTEXT = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    if (!CONTEXT) {
        Debug::log(ERR, "applyConfigToKeyboard: CONTEXT null??");
        return;
    }

    Debug::log(LOG, "Attempting to create a keymap for layout {} with variant {} (rules: {}, model: {}, options: {})", rules.layout, rules.variant, rules.rules, rules.model,
               rules.options);

    xkb_keymap* KEYMAP = NULL;

    if (!FILEPATH.empty()) {
        auto path = absolutePath(FILEPATH, g_pConfigManager->configCurrentPath);

        if (FILE* const KEYMAPFILE = fopen(path.c_str(), "r"); !KEYMAPFILE)
            Debug::log(ERR, "Cannot open input:kb_file= file for reading");
        else {
            KEYMAP = xkb_keymap_new_from_file(CONTEXT, KEYMAPFILE, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
            fclose(KEYMAPFILE);
        }
    }

    if (!KEYMAP)
        KEYMAP = xkb_keymap_new_from_names(CONTEXT, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

    if (!KEYMAP) {
        g_pConfigManager->addParseError("Invalid keyboard layout passed. ( rules: " + RULES + ", model: " + MODEL + ", variant: " + VARIANT + ", options: " + OPTIONS +
                                        ", layout: " + LAYOUT + " )");

        Debug::log(ERR, "Keyboard layout {} with variant {} (rules: {}, model: {}, options: {}) couldn't have been loaded.", rules.layout, rules.variant, rules.rules, rules.model,
                   rules.options);
        memset(&rules, 0, sizeof(rules));

        pKeyboard->currentRules.rules   = "";
        pKeyboard->currentRules.model   = "";
        pKeyboard->currentRules.variant = "";
        pKeyboard->currentRules.options = "";
        pKeyboard->currentRules.layout  = "";

        KEYMAP = xkb_keymap_new_from_names(CONTEXT, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
    }

    wlr_keyboard_set_keymap(wlr_keyboard_from_input_device(pKeyboard->keyboard), KEYMAP);

    wlr_keyboard_modifiers wlrMods = {0};

    if (NUMLOCKON == 1) {
        // lock numlock
        const auto IDX = xkb_map_mod_get_index(KEYMAP, XKB_MOD_NAME_NUM);

        if (IDX != XKB_MOD_INVALID)
            wlrMods.locked |= (uint32_t)1 << IDX;
    }

    if (wlrMods.locked != 0)
        wlr_keyboard_notify_modifiers(wlr_keyboard_from_input_device(pKeyboard->keyboard), 0, 0, wlrMods.locked, 0);

    xkb_keymap_unref(KEYMAP);
    xkb_context_unref(CONTEXT);

    const auto LAYOUTSTR = getActiveLayoutForKeyboard(pKeyboard);

    g_pEventManager->postEvent(SHyprIPCEvent{"activelayout", pKeyboard->name + "," + LAYOUTSTR});
    EMIT_HOOK_EVENT("activeLayout", (std::vector<void*>{pKeyboard, (void*)&LAYOUTSTR}));

    Debug::log(LOG, "Set the keyboard layout to {} and variant to {} for keyboard \"{}\"", rules.layout, rules.variant, pKeyboard->keyboard->name);
}

void CInputManager::newMouse(wlr_input_device* mouse, bool virt) {
    m_lMice.emplace_back();
    const auto PMOUSE = &m_lMice.back();

    PMOUSE->mouse = mouse;
    PMOUSE->virt  = virt;
    try {
        PMOUSE->name = getNameForNewDevice(mouse->name);
    } catch (std::exception& e) {
        Debug::log(ERR, "Mouse had no name???"); // logic error
    }

    if (wlr_input_device_is_libinput(mouse)) {
        const auto LIBINPUTDEV = (libinput_device*)wlr_libinput_get_device_handle(mouse);

        Debug::log(LOG, "New mouse has libinput sens {:.2f} ({:.2f}) with accel profile {} ({})", libinput_device_config_accel_get_speed(LIBINPUTDEV),
                   libinput_device_config_accel_get_default_speed(LIBINPUTDEV), (int)libinput_device_config_accel_get_profile(LIBINPUTDEV),
                   (int)libinput_device_config_accel_get_default_profile(LIBINPUTDEV));
    }

    wlr_cursor_attach_input_device(g_pCompositor->m_sWLRCursor, mouse);

    PMOUSE->connected = true;

    setPointerConfigs();

    PMOUSE->hyprListener_destroyMouse.initCallback(&mouse->events.destroy, &Events::listener_destroyMouse, PMOUSE, "Mouse");

    g_pCompositor->m_sSeat.mouse = PMOUSE;

    m_tmrLastCursorMovement.reset();

    Debug::log(LOG, "New mouse created, pointer WLR: {:x}", (uintptr_t)mouse);
}

void CInputManager::setPointerConfigs() {
    for (auto& m : m_lMice) {
        const auto PPOINTER = &m;

        auto       devname = PPOINTER->name;

        const auto HASCONFIG = g_pConfigManager->deviceConfigExists(devname);

        if (HASCONFIG) {
            const auto ENABLED = g_pConfigManager->getDeviceInt(devname, "enabled");
            if (ENABLED && !m.connected) {
                wlr_cursor_attach_input_device(g_pCompositor->m_sWLRCursor, m.mouse);
                m.connected = true;
            } else if (!ENABLED && m.connected) {
                wlr_cursor_detach_input_device(g_pCompositor->m_sWLRCursor, m.mouse);
                m.connected = false;
            }
        }

        if (wlr_input_device_is_libinput(m.mouse)) {
            const auto LIBINPUTDEV = (libinput_device*)wlr_libinput_get_device_handle(m.mouse);

            double     touchw = 0, touchh = 0;
            const auto ISTOUCHPAD = libinput_device_has_capability(LIBINPUTDEV, LIBINPUT_DEVICE_CAP_POINTER) &&
                libinput_device_get_size(LIBINPUTDEV, &touchw, &touchh) == 0; // pointer with size is a touchpad

            if (g_pConfigManager->getDeviceInt(devname, "clickfinger_behavior", "input:touchpad:clickfinger_behavior") == 0) // toggle software buttons or clickfinger
                libinput_device_config_click_set_method(LIBINPUTDEV, LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS);
            else
                libinput_device_config_click_set_method(LIBINPUTDEV, LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER);

            if (g_pConfigManager->getDeviceInt(devname, "left_handed", "input:left_handed") == 0)
                libinput_device_config_left_handed_set(LIBINPUTDEV, 0);
            else
                libinput_device_config_left_handed_set(LIBINPUTDEV, 1);

            if (libinput_device_config_middle_emulation_is_available(LIBINPUTDEV)) { // middleclick on r+l mouse button pressed
                if (g_pConfigManager->getDeviceInt(devname, "middle_button_emulation", "input:touchpad:middle_button_emulation") == 1)
                    libinput_device_config_middle_emulation_set_enabled(LIBINPUTDEV, LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED);
                else
                    libinput_device_config_middle_emulation_set_enabled(LIBINPUTDEV, LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED);

                const auto TAP_MAP = g_pConfigManager->getDeviceString(devname, "tap_button_map", "input:touchpad:tap_button_map");
                if (TAP_MAP == "" || TAP_MAP == "lrm")
                    libinput_device_config_tap_set_button_map(LIBINPUTDEV, LIBINPUT_CONFIG_TAP_MAP_LRM);
                else if (TAP_MAP == "lmr")
                    libinput_device_config_tap_set_button_map(LIBINPUTDEV, LIBINPUT_CONFIG_TAP_MAP_LMR);
                else
                    Debug::log(WARN, "Tap button mapping unknown");
            }

            const auto SCROLLMETHOD = g_pConfigManager->getDeviceString(devname, "scroll_method", "input:scroll_method");
            if (SCROLLMETHOD == "") {
                libinput_device_config_scroll_set_method(LIBINPUTDEV, libinput_device_config_scroll_get_default_method(LIBINPUTDEV));
            } else if (SCROLLMETHOD == "no_scroll") {
                libinput_device_config_scroll_set_method(LIBINPUTDEV, LIBINPUT_CONFIG_SCROLL_NO_SCROLL);
            } else if (SCROLLMETHOD == "2fg") {
                libinput_device_config_scroll_set_method(LIBINPUTDEV, LIBINPUT_CONFIG_SCROLL_2FG);
            } else if (SCROLLMETHOD == "edge") {
                libinput_device_config_scroll_set_method(LIBINPUTDEV, LIBINPUT_CONFIG_SCROLL_EDGE);
            } else if (SCROLLMETHOD == "on_button_down") {
                libinput_device_config_scroll_set_method(LIBINPUTDEV, LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN);
            } else {
                Debug::log(WARN, "Scroll method unknown");
            }

            if (g_pConfigManager->getDeviceInt(devname, "tap-and-drag", "input:touchpad:tap-and-drag") == 0)
                libinput_device_config_tap_set_drag_enabled(LIBINPUTDEV, LIBINPUT_CONFIG_DRAG_DISABLED);
            else
                libinput_device_config_tap_set_drag_enabled(LIBINPUTDEV, LIBINPUT_CONFIG_DRAG_ENABLED);

            if (g_pConfigManager->getDeviceInt(devname, "drag_lock", "input:touchpad:drag_lock") == 0)
                libinput_device_config_tap_set_drag_lock_enabled(LIBINPUTDEV, LIBINPUT_CONFIG_DRAG_LOCK_DISABLED);
            else
                libinput_device_config_tap_set_drag_lock_enabled(LIBINPUTDEV, LIBINPUT_CONFIG_DRAG_LOCK_ENABLED);

            if (libinput_device_config_tap_get_finger_count(LIBINPUTDEV)) // this is for tapping (like on a laptop)
                if (g_pConfigManager->getDeviceInt(devname, "tap-to-click", "input:touchpad:tap-to-click") == 1)
                    libinput_device_config_tap_set_enabled(LIBINPUTDEV, LIBINPUT_CONFIG_TAP_ENABLED);

            if (libinput_device_config_scroll_has_natural_scroll(LIBINPUTDEV)) {

                if (ISTOUCHPAD)
                    libinput_device_config_scroll_set_natural_scroll_enabled(LIBINPUTDEV,
                                                                             g_pConfigManager->getDeviceInt(devname, "natural_scroll", "input:touchpad:natural_scroll"));
                else
                    libinput_device_config_scroll_set_natural_scroll_enabled(LIBINPUTDEV, g_pConfigManager->getDeviceInt(devname, "natural_scroll", "input:natural_scroll"));
            }

            if (libinput_device_config_dwt_is_available(LIBINPUTDEV)) {
                const auto DWT =
                    static_cast<enum libinput_config_dwt_state>(g_pConfigManager->getDeviceInt(devname, "disable_while_typing", "input:touchpad:disable_while_typing") != 0);
                libinput_device_config_dwt_set_enabled(LIBINPUTDEV, DWT);
            }

            const auto LIBINPUTSENS = std::clamp(g_pConfigManager->getDeviceFloat(devname, "sensitivity", "input:sensitivity"), -1.f, 1.f);
            libinput_device_config_accel_set_speed(LIBINPUTDEV, LIBINPUTSENS);

            const auto ACCELPROFILE = g_pConfigManager->getDeviceString(devname, "accel_profile", "input:accel_profile");

            if (ACCELPROFILE == "") {
                libinput_device_config_accel_set_profile(LIBINPUTDEV, libinput_device_config_accel_get_default_profile(LIBINPUTDEV));
            } else if (ACCELPROFILE == "adaptive") {
                libinput_device_config_accel_set_profile(LIBINPUTDEV, LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);
            } else if (ACCELPROFILE == "flat") {
                libinput_device_config_accel_set_profile(LIBINPUTDEV, LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT);
            } else if (ACCELPROFILE.starts_with("custom")) {
                CVarList args = {ACCELPROFILE, 0, ' '};
                try {
                    double              step = std::stod(args[1]);
                    std::vector<double> points;
                    for (size_t i = 2; i < args.size(); ++i)
                        points.push_back(std::stod(args[i]));

                    const auto CONFIG = libinput_config_accel_create(LIBINPUT_CONFIG_ACCEL_PROFILE_CUSTOM);
                    libinput_config_accel_set_points(CONFIG, LIBINPUT_ACCEL_TYPE_MOTION, step, points.size(), points.data());
                    libinput_device_config_accel_set_profile(LIBINPUTDEV, LIBINPUT_CONFIG_ACCEL_PROFILE_CUSTOM);
                    libinput_config_accel_destroy(CONFIG);
                } catch (std::exception& e) { Debug::log(ERR, "Invalid values in custom accel profile"); }
            } else {
                Debug::log(WARN, "Unknown acceleration profile, falling back to default");
            }

            const auto SCROLLBUTTON = g_pConfigManager->getDeviceInt(devname, "scroll_button", "input:scroll_button");

            libinput_device_config_scroll_set_button(LIBINPUTDEV, SCROLLBUTTON == 0 ? libinput_device_config_scroll_get_default_button(LIBINPUTDEV) : SCROLLBUTTON);

            const auto SCROLLBUTTONLOCK = g_pConfigManager->getDeviceInt(devname, "scroll_button_lock", "input:scroll_button_lock");

            libinput_device_config_scroll_set_button_lock(LIBINPUTDEV,
                                                          SCROLLBUTTONLOCK == 0 ? LIBINPUT_CONFIG_SCROLL_BUTTON_LOCK_DISABLED : LIBINPUT_CONFIG_SCROLL_BUTTON_LOCK_ENABLED);

            Debug::log(LOG, "Applied config to mouse {}, sens {:.2f}", m.name, LIBINPUTSENS);
        }
    }
}

void CInputManager::destroyKeyboard(SKeyboard* pKeyboard) {
    pKeyboard->hyprListener_keyboardDestroy.removeCallback();
    pKeyboard->hyprListener_keyboardMod.removeCallback();
    pKeyboard->hyprListener_keyboardKey.removeCallback();

    if (pKeyboard->active) {
        m_lKeyboards.remove(*pKeyboard);

        if (m_lKeyboards.size() > 0) {
            m_pActiveKeyboard         = &m_lKeyboards.back();
            m_pActiveKeyboard->active = true;
        } else {
            m_pActiveKeyboard = nullptr;
        }
    } else
        m_lKeyboards.remove(*pKeyboard);
}

void CInputManager::destroyMouse(wlr_input_device* mouse) {
    for (auto& m : m_lMice) {
        if (m.mouse == mouse) {
            m_lMice.remove(m);
            break;
        }
    }

    g_pCompositor->m_sSeat.mouse = m_lMice.size() > 0 ? &m_lMice.front() : nullptr;

    if (g_pCompositor->m_sSeat.mouse)
        unconstrainMouse();
}

void CInputManager::updateKeyboardsLeds(wlr_input_device* pKeyboard) {
    auto keyboard = wlr_keyboard_from_input_device(pKeyboard);

    if (keyboard->xkb_state == NULL) {
        return;
    }

    uint32_t leds = 0;
    for (uint32_t i = 0; i < WLR_LED_COUNT; ++i) {
        if (xkb_state_led_index_is_active(keyboard->xkb_state, keyboard->led_indexes[i]))
            leds |= (1 << i);
    }

    for (auto& kb : m_lKeyboards) {
        if ((kb.isVirtual && shouldIgnoreVirtualKeyboard(&kb)) || kb.keyboard == pKeyboard)
            continue;

        wlr_keyboard_led_update(wlr_keyboard_from_input_device(kb.keyboard), leds);
    }
}

void CInputManager::onKeyboardKey(wlr_keyboard_key_event* e, SKeyboard* pKeyboard) {
    if (!pKeyboard->enabled)
        return;

    static auto* const PDPMS = &g_pConfigManager->getConfigValuePtr("misc:key_press_enables_dpms")->intValue;
    if (*PDPMS && !g_pCompositor->m_bDPMSStateON) {
        // enable dpms
        g_pKeybindManager->dpms("on");
    }

    bool passEvent = g_pKeybindManager->onKeyEvent(e, pKeyboard);

    g_pCompositor->notifyIdleActivity();

    if (passEvent) {

        const auto PIMEGRAB = m_sIMERelay.getIMEKeyboardGrab(pKeyboard);

        if (PIMEGRAB && PIMEGRAB->pWlrKbGrab && PIMEGRAB->pWlrKbGrab->input_method) {
            wlr_input_method_keyboard_grab_v2_set_keyboard(PIMEGRAB->pWlrKbGrab, wlr_keyboard_from_input_device(pKeyboard->keyboard));
            wlr_input_method_keyboard_grab_v2_send_key(PIMEGRAB->pWlrKbGrab, e->time_msec, e->keycode, e->state);
        } else {
            wlr_seat_set_keyboard(g_pCompositor->m_sSeat.seat, wlr_keyboard_from_input_device(pKeyboard->keyboard));
            wlr_seat_keyboard_notify_key(g_pCompositor->m_sSeat.seat, e->time_msec, e->keycode, e->state);
        }

        updateKeyboardsLeds(pKeyboard->keyboard);
    }
}

void CInputManager::onKeyboardMod(void* data, SKeyboard* pKeyboard) {
    if (!pKeyboard->enabled)
        return;

    const auto PIMEGRAB = m_sIMERelay.getIMEKeyboardGrab(pKeyboard);

    const auto ALLMODS = accumulateModsFromAllKBs();

    auto       MODS = wlr_keyboard_from_input_device(pKeyboard->keyboard)->modifiers;
    MODS.depressed  = ALLMODS;

    if (PIMEGRAB && PIMEGRAB->pWlrKbGrab && PIMEGRAB->pWlrKbGrab->input_method) {
        wlr_input_method_keyboard_grab_v2_set_keyboard(PIMEGRAB->pWlrKbGrab, wlr_keyboard_from_input_device(pKeyboard->keyboard));
        wlr_input_method_keyboard_grab_v2_send_modifiers(PIMEGRAB->pWlrKbGrab, &MODS);
    } else {
        wlr_seat_set_keyboard(g_pCompositor->m_sSeat.seat, wlr_keyboard_from_input_device(pKeyboard->keyboard));
        wlr_seat_keyboard_notify_modifiers(g_pCompositor->m_sSeat.seat, &MODS);
    }

    updateKeyboardsLeds(pKeyboard->keyboard);

    const auto PWLRKB = wlr_keyboard_from_input_device(pKeyboard->keyboard);

    if (PWLRKB->modifiers.group != pKeyboard->activeLayout) {
        pKeyboard->activeLayout = PWLRKB->modifiers.group;

        const auto LAYOUT = getActiveLayoutForKeyboard(pKeyboard);

        g_pEventManager->postEvent(SHyprIPCEvent{"activelayout", pKeyboard->name + "," + LAYOUT});
        EMIT_HOOK_EVENT("activeLayout", (std::vector<void*>{pKeyboard, (void*)&LAYOUT}));
    }
}

bool CInputManager::shouldIgnoreVirtualKeyboard(SKeyboard* pKeyboard) {
    return !pKeyboard ||
        (m_sIMERelay.m_pKeyboardGrab &&
         wl_resource_get_client(m_sIMERelay.m_pKeyboardGrab->pWlrKbGrab->resource) == wl_resource_get_client(wlr_input_device_get_virtual_keyboard(pKeyboard->keyboard)->resource));
}

void CInputManager::refocus() {
    mouseMoveUnified(0, true);
}

void CInputManager::updateDragIcon() {
    if (!m_sDrag.dragIcon)
        return;

    switch (m_sDrag.dragIcon->drag->grab_type) {
        case WLR_DRAG_GRAB_KEYBOARD: break;
        case WLR_DRAG_GRAB_KEYBOARD_POINTER: {
            wlr_box box = {m_sDrag.pos.x - 2, m_sDrag.pos.y - 2, m_sDrag.dragIcon->surface->current.width + 4, m_sDrag.dragIcon->surface->current.height + 4};
            g_pHyprRenderer->damageBox(&box);
            m_sDrag.pos = getMouseCoordsInternal();
            break;
        }
        default: break;
    }
}

void CInputManager::recheckConstraint(SMouse* pMouse) {
    if (!pMouse->currentConstraint)
        return;

    const auto PREGION = &pMouse->currentConstraint->region;

    if (pMouse->currentConstraint->type == WLR_POINTER_CONSTRAINT_V1_CONFINED)
        pMouse->confinedTo.set(PREGION);
    else
        pMouse->confinedTo.clear();
}

void CInputManager::constrainMouse(SMouse* pMouse, wlr_pointer_constraint_v1* constraint) {

    if (pMouse->currentConstraint == constraint)
        return;

    const auto MOUSECOORDS = getMouseCoordsInternal();
    const auto PCONSTRAINT = constraintFromWlr(constraint);

    pMouse->hyprListener_commitConstraint.removeCallback();

    if (pMouse->currentConstraint)
        wlr_pointer_constraint_v1_send_deactivated(pMouse->currentConstraint);

    if (const auto PWINDOW = g_pCompositor->getWindowFromSurface(constraint->surface); PWINDOW) {
        const auto RELATIVETO = PWINDOW->m_bIsX11 ?
            (PWINDOW->m_bIsMapped ? PWINDOW->m_vRealPosition.goalv() :
                                    g_pXWaylandManager->xwaylandToWaylandCoords({PWINDOW->m_uSurface.xwayland->x, PWINDOW->m_uSurface.xwayland->y})) :
            PWINDOW->m_vRealPosition.goalv();

        PCONSTRAINT->cursorPosOnActivate = MOUSECOORDS - RELATIVETO;
    }

    if (constraint->current.committed & WLR_POINTER_CONSTRAINT_V1_STATE_CURSOR_HINT) {
        PCONSTRAINT->hintSet      = true;
        PCONSTRAINT->positionHint = {constraint->current.cursor_hint.x, constraint->current.cursor_hint.y};
    }

    if (constraint->current.committed & WLR_POINTER_CONSTRAINT_V1_STATE_CURSOR_HINT && constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED)
        warpMouseToConstraintMiddle(PCONSTRAINT);

    pMouse->currentConstraint = constraint;
    pMouse->constraintActive  = true;

    if (pixman_region32_not_empty(&constraint->current.region))
        pixman_region32_intersect(&constraint->region, &constraint->surface->input_region, &constraint->current.region);
    else
        pixman_region32_copy(&constraint->region, &constraint->surface->input_region);

    // warp to the constraint
    recheckConstraint(pMouse);

    PCONSTRAINT->active = true;

    wlr_pointer_constraint_v1_send_activated(pMouse->currentConstraint);

    pMouse->hyprListener_commitConstraint.initCallback(&pMouse->currentConstraint->surface->events.commit, &Events::listener_commitConstraint, pMouse, "Mouse constraint commit");

    Debug::log(LOG, "Constrained mouse to {:x}", (uintptr_t)pMouse->currentConstraint);
}

void CInputManager::warpMouseToConstraintMiddle(SConstraint* pConstraint) {

    if (!pConstraint)
        return;

    const auto PWINDOW = g_pCompositor->getWindowFromSurface(pConstraint->constraint->surface);

    if (PWINDOW) {
        const auto RELATIVETO = pConstraint->getLogicConstraintPos();
        const auto HINTSCALE  = PWINDOW->m_fX11SurfaceScaledBy;

        auto       HINT = pConstraint->hintSet ? pConstraint->positionHint : pConstraint->cursorPosOnActivate;

        if (HINT == Vector2D{-1, -1})
            HINT = pConstraint->getLogicConstraintSize() / 2.f;

        if (HINT != Vector2D{-1, -1}) {
            wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, RELATIVETO.x + HINT.x / HINTSCALE, RELATIVETO.y + HINT.y / HINTSCALE);
            wlr_seat_pointer_warp(pConstraint->constraint->seat, pConstraint->constraint->current.cursor_hint.x, pConstraint->constraint->current.cursor_hint.y);
        }
    }
}

void CInputManager::unconstrainMouse() {
    if (!g_pCompositor->m_sSeat.mouse || !g_pCompositor->m_sSeat.mouse->currentConstraint)
        return;

    const auto CONSTRAINTWINDOW = g_pCompositor->getConstraintWindow(g_pCompositor->m_sSeat.mouse);

    if (CONSTRAINTWINDOW)
        g_pXWaylandManager->activateSurface(CONSTRAINTWINDOW->m_pWLSurface.wlr(), false);

    wlr_pointer_constraint_v1_send_deactivated(g_pCompositor->m_sSeat.mouse->currentConstraint);

    const auto PCONSTRAINT = constraintFromWlr(g_pCompositor->m_sSeat.mouse->currentConstraint);
    if (PCONSTRAINT)
        PCONSTRAINT->active = false;

    g_pCompositor->m_sSeat.mouse->constraintActive = false;

    // TODO: its better to somehow detect the workspace...
    g_pCompositor->m_sSeat.mouse->currentConstraint = nullptr;

    g_pCompositor->m_sSeat.mouse->hyprListener_commitConstraint.removeCallback();
}

void Events::listener_commitConstraint(void* owner, void* data) {
    const auto PMOUSE = (SMouse*)owner;

    if (PMOUSE->currentConstraint->current.committed & WLR_POINTER_CONSTRAINT_V1_STATE_CURSOR_HINT) {
        const auto PCONSTRAINT = g_pInputManager->constraintFromWlr(PMOUSE->currentConstraint);
        if (PCONSTRAINT) { // should never be null but who knows
            PCONSTRAINT->positionHint = Vector2D(PMOUSE->currentConstraint->current.cursor_hint.x, PMOUSE->currentConstraint->current.cursor_hint.y);
            PCONSTRAINT->hintSet      = true;
        }
    }

    if (PMOUSE->currentConstraint->current.committed & WLR_POINTER_CONSTRAINT_V1_STATE_REGION) {
        if (pixman_region32_not_empty(&PMOUSE->currentConstraint->current.region))
            pixman_region32_intersect(&PMOUSE->currentConstraint->region, &PMOUSE->currentConstraint->surface->input_region, &PMOUSE->currentConstraint->current.region);
        else
            pixman_region32_copy(&PMOUSE->currentConstraint->region, &PMOUSE->currentConstraint->surface->input_region);

        g_pInputManager->recheckConstraint(PMOUSE);
    }
}

void CInputManager::updateCapabilities() {
    uint32_t caps = 0;

    if (!m_lKeyboards.empty())
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    if (!m_lMice.empty())
        caps |= WL_SEAT_CAPABILITY_POINTER;
    if (!m_lTouchDevices.empty())
        caps |= WL_SEAT_CAPABILITY_TOUCH;
    if (!m_lTabletTools.empty())
        caps |= WL_SEAT_CAPABILITY_POINTER;

    wlr_seat_set_capabilities(g_pCompositor->m_sSeat.seat, caps);
    m_uiCapabilities = caps;
}

uint32_t CInputManager::accumulateModsFromAllKBs() {

    uint32_t finalMask = 0;

    for (auto& kb : m_lKeyboards) {
        if (kb.isVirtual && shouldIgnoreVirtualKeyboard(&kb))
            continue;

        if (!kb.enabled)
            continue;

        finalMask |= wlr_keyboard_get_modifiers(wlr_keyboard_from_input_device(kb.keyboard));
    }

    return finalMask;
}

std::string CInputManager::getActiveLayoutForKeyboard(SKeyboard* pKeyboard) {
    const auto WLRKB      = wlr_keyboard_from_input_device(pKeyboard->keyboard);
    const auto KEYMAP     = WLRKB->keymap;
    const auto STATE      = WLRKB->xkb_state;
    const auto LAYOUTSNUM = xkb_keymap_num_layouts(KEYMAP);

    for (uint32_t i = 0; i < LAYOUTSNUM; ++i) {
        if (xkb_state_layout_index_is_active(STATE, i, XKB_STATE_LAYOUT_EFFECTIVE)) {
            const auto LAYOUTNAME = xkb_keymap_layout_get_name(KEYMAP, i);

            if (LAYOUTNAME)
                return std::string(LAYOUTNAME);
            return "error";
        }
    }

    return "none";
}

void CInputManager::disableAllKeyboards(bool virt) {

    for (auto& k : m_lKeyboards) {
        if (k.isVirtual != virt)
            continue;

        k.active = false;
    }
}

void CInputManager::newTouchDevice(wlr_input_device* pDevice) {
    const auto PNEWDEV  = &m_lTouchDevices.emplace_back();
    PNEWDEV->pWlrDevice = pDevice;

    try {
        PNEWDEV->name = getNameForNewDevice(pDevice->name);
    } catch (std::exception& e) {
        Debug::log(ERR, "Touch Device had no name???"); // logic error
    }

    setTouchDeviceConfigs(PNEWDEV);
    wlr_cursor_attach_input_device(g_pCompositor->m_sWLRCursor, pDevice);

    Debug::log(LOG, "New touch device added at {:x}", (uintptr_t)PNEWDEV);

    PNEWDEV->hyprListener_destroy.initCallback(
        &pDevice->events.destroy, [&](void* owner, void* data) { destroyTouchDevice((STouchDevice*)data); }, PNEWDEV, "TouchDevice");
}

void CInputManager::setTouchDeviceConfigs(STouchDevice* dev) {

    auto setConfig = [&](STouchDevice* const PTOUCHDEV) -> void {
        if (wlr_input_device_is_libinput(PTOUCHDEV->pWlrDevice)) {
            const auto LIBINPUTDEV = (libinput_device*)wlr_libinput_get_device_handle(PTOUCHDEV->pWlrDevice);

            const int  ROTATION = std::clamp(g_pConfigManager->getDeviceInt(PTOUCHDEV->name, "transform", "input:touchdevice:transform"), 0, 7);
            if (libinput_device_config_calibration_has_matrix(LIBINPUTDEV))
                libinput_device_config_calibration_set_matrix(LIBINPUTDEV, MATRICES[ROTATION]);

            const auto OUTPUT = g_pConfigManager->getDeviceString(PTOUCHDEV->name, "output", "input:touchdevice:output");
            if (!OUTPUT.empty() && OUTPUT != STRVAL_EMPTY)
                PTOUCHDEV->boundOutput = OUTPUT;
            else
                PTOUCHDEV->boundOutput = "";
        }
    };

    if (dev) {
        setConfig(dev);
        return;
    }

    for (auto& m : m_lTouchDevices) {
        const auto PTOUCHDEV = &m;

        setConfig(PTOUCHDEV);
    }
}

void CInputManager::setTabletConfigs() {
    for (auto& t : m_lTablets) {
        if (wlr_input_device_is_libinput(t.wlrDevice)) {
            const auto LIBINPUTDEV = (libinput_device*)wlr_libinput_get_device_handle(t.wlrDevice);

            const int  ROTATION = std::clamp(g_pConfigManager->getDeviceInt(t.name, "transform", "input:tablet:transform"), 0, 7);
            Debug::log(LOG, "Setting calibration matrix for device {}", t.name);
            libinput_device_config_calibration_set_matrix(LIBINPUTDEV, MATRICES[ROTATION]);

            const auto OUTPUT   = g_pConfigManager->getDeviceString(t.name, "output", "input:tablet:output");
            const auto PMONITOR = g_pCompositor->getMonitorFromString(OUTPUT);
            if (!OUTPUT.empty() && OUTPUT != STRVAL_EMPTY && PMONITOR) {
                wlr_cursor_map_input_to_output(g_pCompositor->m_sWLRCursor, t.wlrDevice, PMONITOR->output);
                wlr_cursor_map_input_to_region(g_pCompositor->m_sWLRCursor, t.wlrDevice, nullptr);
            }

            const auto REGION_POS  = g_pConfigManager->getDeviceVec(t.name, "region_position", "input:tablet:region_position");
            const auto REGION_SIZE = g_pConfigManager->getDeviceVec(t.name, "region_size", "input:tablet:region_size");
            const auto REGION      = wlr_box{REGION_POS.x, REGION_POS.y, REGION_SIZE.x, REGION_SIZE.y};
            if (!wlr_box_empty(&REGION))
                wlr_cursor_map_input_to_region(g_pCompositor->m_sWLRCursor, t.wlrDevice, &REGION);
        }
    }
}

void CInputManager::destroyTouchDevice(STouchDevice* pDevice) {
    Debug::log(LOG, "Touch device at {:x} removed", (uintptr_t)pDevice);

    m_lTouchDevices.remove(*pDevice);
}

void CInputManager::newSwitch(wlr_input_device* pDevice) {
    const auto PNEWDEV  = &m_lSwitches.emplace_back();
    PNEWDEV->pWlrDevice = pDevice;

    Debug::log(LOG, "New switch with name \"{}\" added", pDevice->name);

    PNEWDEV->hyprListener_destroy.initCallback(
        &pDevice->events.destroy, [&](void* owner, void* data) { destroySwitch((SSwitchDevice*)owner); }, PNEWDEV, "SwitchDevice");

    const auto PSWITCH = wlr_switch_from_input_device(pDevice);

    PNEWDEV->hyprListener_toggle.initCallback(
        &PSWITCH->events.toggle,
        [&](void* owner, void* data) {
            const auto PDEVICE = (SSwitchDevice*)owner;
            const auto NAME    = std::string(PDEVICE->pWlrDevice->name);
            const auto E       = (wlr_switch_toggle_event*)data;

            if (PDEVICE->status != -1 && PDEVICE->status == E->switch_state)
                return;

            Debug::log(LOG, "Switch {} fired, triggering binds.", NAME);

            g_pKeybindManager->onSwitchEvent(NAME);

            switch (E->switch_state) {
                case WLR_SWITCH_STATE_ON:
                    Debug::log(LOG, "Switch {} turn on, triggering binds.", NAME);
                    g_pKeybindManager->onSwitchOnEvent(NAME);
                    break;
                case WLR_SWITCH_STATE_OFF:
                    Debug::log(LOG, "Switch {} turn off, triggering binds.", NAME);
                    g_pKeybindManager->onSwitchOffEvent(NAME);
                    break;
            }

            PDEVICE->status = E->switch_state;
        },
        PNEWDEV, "SwitchDevice");
}

void CInputManager::destroySwitch(SSwitchDevice* pDevice) {
    m_lSwitches.remove(*pDevice);
}

void CInputManager::setCursorImageUntilUnset(std::string name) {
    g_pHyprRenderer->setCursorFromName(name.c_str());
    m_bCursorImageOverridden = true;
}

void CInputManager::unsetCursorImage() {
    if (!m_bCursorImageOverridden)
        return;

    m_bCursorImageOverridden = false;
    if (!g_pHyprRenderer->m_bWindowRequestedCursorHide)
        g_pHyprRenderer->setCursorFromName("left_ptr");
}

std::string CInputManager::deviceNameToInternalString(std::string in) {
    std::replace(in.begin(), in.end(), ' ', '-');
    std::replace(in.begin(), in.end(), '\n', '-');
    std::transform(in.begin(), in.end(), in.begin(), ::tolower);
    return in;
}

std::string CInputManager::getNameForNewDevice(std::string internalName) {

    auto proposedNewName = deviceNameToInternalString(internalName);
    int  dupeno          = 0;

    while (std::find_if(m_lKeyboards.begin(), m_lKeyboards.end(),
                        [&](const SKeyboard& other) { return other.name == proposedNewName + (dupeno == 0 ? "" : ("-" + std::to_string(dupeno))); }) != m_lKeyboards.end())
        dupeno++;

    while (std::find_if(m_lMice.begin(), m_lMice.end(), [&](const SMouse& other) { return other.name == proposedNewName + (dupeno == 0 ? "" : ("-" + std::to_string(dupeno))); }) !=
           m_lMice.end())
        dupeno++;

    while (std::find_if(m_lTouchDevices.begin(), m_lTouchDevices.end(),
                        [&](const STouchDevice& other) { return other.name == proposedNewName + (dupeno == 0 ? "" : ("-" + std::to_string(dupeno))); }) != m_lTouchDevices.end())
        dupeno++;

    while (std::find_if(m_lTabletPads.begin(), m_lTabletPads.end(),
                        [&](const STabletPad& other) { return other.name == proposedNewName + (dupeno == 0 ? "" : ("-" + std::to_string(dupeno))); }) != m_lTabletPads.end())
        dupeno++;

    while (std::find_if(m_lTablets.begin(), m_lTablets.end(),
                        [&](const STablet& other) { return other.name == proposedNewName + (dupeno == 0 ? "" : ("-" + std::to_string(dupeno))); }) != m_lTablets.end())
        dupeno++;

    while (std::find_if(m_lTabletTools.begin(), m_lTabletTools.end(),
                        [&](const STabletTool& other) { return other.name == proposedNewName + (dupeno == 0 ? "" : ("-" + std::to_string(dupeno))); }) != m_lTabletTools.end())
        dupeno++;

    return proposedNewName + (dupeno == 0 ? "" : ("-" + std::to_string(dupeno)));
}

SConstraint* CInputManager::constraintFromWlr(wlr_pointer_constraint_v1* constraint) {
    for (auto& c : m_lConstraints) {
        if (c.constraint == constraint)
            return &c;
    }

    return nullptr;
}

void CInputManager::releaseAllMouseButtons() {
    const auto buttonsCopy = m_lCurrentlyHeldButtons;

    if (g_pInputManager->m_sDrag.drag)
        return;

    for (auto& mb : buttonsCopy) {
        wlr_seat_pointer_notify_button(g_pCompositor->m_sSeat.seat, 0, mb, WLR_BUTTON_RELEASED);
    }

    m_lCurrentlyHeldButtons.clear();
}

void CInputManager::setCursorIconOnBorder(CWindow* w) {
    // do not override cursor icons set by mouse binds
    if (g_pKeybindManager->m_bIsMouseBindActive) {
        m_eBorderIconDirection = BORDERICON_NONE;
        return;
    }

    static auto* const PROUNDING         = &g_pConfigManager->getConfigValuePtr("decoration:rounding")->intValue;
    static const auto* PEXTENDBORDERGRAB = &g_pConfigManager->getConfigValuePtr("general:extend_border_grab_area")->intValue;
    const int          BORDERSIZE        = w->getRealBorderSize();

    // give a small leeway (10 px) for corner icon
    const auto           CORNER           = *PROUNDING + BORDERSIZE + 10;
    const auto           mouseCoords      = getMouseCoordsInternal();
    wlr_box              box              = {w->m_vRealPosition.vec().x, w->m_vRealPosition.vec().y, w->m_vRealSize.vec().x, w->m_vRealSize.vec().y};
    eBorderIconDirection direction        = BORDERICON_NONE;
    wlr_box              boxFullGrabInput = {box.x - *PEXTENDBORDERGRAB - BORDERSIZE, box.y - *PEXTENDBORDERGRAB - BORDERSIZE, box.width + 2 * (*PEXTENDBORDERGRAB + BORDERSIZE),
                                box.height + 2 * (*PEXTENDBORDERGRAB + BORDERSIZE)};

    if (!wlr_box_contains_point(&boxFullGrabInput, mouseCoords.x, mouseCoords.y) || (!m_lCurrentlyHeldButtons.empty() && !currentlyDraggedWindow)) {
        direction = BORDERICON_NONE;
    } else if (wlr_box_contains_point(&box, mouseCoords.x, mouseCoords.y)) {
        if (!w->isInCurvedCorner(mouseCoords.x, mouseCoords.y)) {
            direction = BORDERICON_NONE;
        } else {
            if (mouseCoords.y < box.y + CORNER) {
                if (mouseCoords.x < box.x + CORNER)
                    direction = BORDERICON_UP_LEFT;
                else
                    direction = BORDERICON_UP_RIGHT;
            } else {
                if (mouseCoords.x < box.x + CORNER)
                    direction = BORDERICON_DOWN_LEFT;
                else
                    direction = BORDERICON_DOWN_RIGHT;
            }
        }
    } else {
        if (mouseCoords.y < box.y + CORNER) {
            if (mouseCoords.x < box.x + CORNER)
                direction = BORDERICON_UP_LEFT;
            else if (mouseCoords.x > box.x + box.width - CORNER)
                direction = BORDERICON_UP_RIGHT;
            else
                direction = BORDERICON_UP;
        } else if (mouseCoords.y > box.y + box.height - CORNER) {
            if (mouseCoords.x < box.x + CORNER)
                direction = BORDERICON_DOWN_LEFT;
            else if (mouseCoords.x > box.x + box.width - CORNER)
                direction = BORDERICON_DOWN_RIGHT;
            else
                direction = BORDERICON_DOWN;
        } else {
            if (mouseCoords.x < box.x + CORNER)
                direction = BORDERICON_LEFT;
            else if (mouseCoords.x > box.x + box.width - CORNER)
                direction = BORDERICON_RIGHT;
        }
    }

    if (direction == m_eBorderIconDirection)
        return;

    m_eBorderIconDirection = direction;

    switch (direction) {
        case BORDERICON_NONE: unsetCursorImage(); break;
        case BORDERICON_UP: setCursorImageUntilUnset("top_side"); break;
        case BORDERICON_DOWN: setCursorImageUntilUnset("bottom_side"); break;
        case BORDERICON_LEFT: setCursorImageUntilUnset("left_side"); break;
        case BORDERICON_RIGHT: setCursorImageUntilUnset("right_side"); break;
        case BORDERICON_UP_LEFT: setCursorImageUntilUnset("top_left_corner"); break;
        case BORDERICON_DOWN_LEFT: setCursorImageUntilUnset("bottom_left_corner"); break;
        case BORDERICON_UP_RIGHT: setCursorImageUntilUnset("top_right_corner"); break;
        case BORDERICON_DOWN_RIGHT: setCursorImageUntilUnset("bottom_right_corner"); break;
    }
}
