#include "InputManager.hpp"
#include "../../Compositor.hpp"
#include "wlr/types/wlr_switch.h"
#include <ranges>
#include "../../config/ConfigValue.hpp"
#include "../../desktop/Window.hpp"
#include "../../protocols/CursorShape.hpp"
#include "../../protocols/IdleInhibit.hpp"
#include "../../protocols/RelativePointer.hpp"
#include "../../protocols/PointerConstraints.hpp"
#include "../../protocols/IdleNotify.hpp"
#include "../../protocols/SessionLock.hpp"
#include "../../protocols/InputMethodV2.hpp"
#include "../../protocols/VirtualKeyboard.hpp"
#include "../../protocols/VirtualPointer.hpp"

#include "../../devices/Mouse.hpp"
#include "../../devices/VirtualPointer.hpp"
#include "../../devices/Keyboard.hpp"
#include "../../devices/VirtualKeyboard.hpp"
#include "../../devices/TouchDevice.hpp"

CInputManager::CInputManager() {
    m_sListeners.setCursorShape = PROTO::cursorShape->events.setShape.registerListener([this](std::any data) {
        if (!cursorImageUnlocked())
            return;

        auto event = std::any_cast<CCursorShapeProtocol::SSetShapeEvent>(data);

        if (!g_pCompositor->m_sSeat.seat->pointer_state.focused_client)
            return;

        if (wl_resource_get_client(event.pMgr->resource()) != g_pCompositor->m_sSeat.seat->pointer_state.focused_client->client)
            return;

        Debug::log(LOG, "cursorImage request: shape {} -> {}", (uint32_t)event.shape, event.shapeName);

        m_sCursorSurfaceInfo.wlSurface.unassign();
        m_sCursorSurfaceInfo.vHotspot = {};
        m_sCursorSurfaceInfo.name     = event.shapeName;
        m_sCursorSurfaceInfo.hidden   = false;

        m_sCursorSurfaceInfo.inUse = true;
        g_pHyprRenderer->setCursorFromName(m_sCursorSurfaceInfo.name);
    });

    m_sListeners.newIdleInhibitor = PROTO::idleInhibit->events.newIdleInhibitor.registerListener([this](std::any data) { this->newIdleInhibitor(data); });
    m_sListeners.newVirtualKeyboard =
        PROTO::virtualKeyboard->events.newKeyboard.registerListener([this](std::any data) { this->newVirtualKeyboard(std::any_cast<SP<CVirtualKeyboardV1Resource>>(data)); });
    m_sListeners.newVirtualMouse =
        PROTO::virtualPointer->events.newPointer.registerListener([this](std::any data) { this->newVirtualMouse(std::any_cast<SP<CVirtualPointerV1Resource>>(data)); });
}

CInputManager::~CInputManager() {
    m_vConstraints.clear();
    m_vKeyboards.clear();
    m_vPointers.clear();
    m_vTouches.clear();
    m_lTablets.clear();
    m_lTabletTools.clear();
    m_lTabletPads.clear();
    m_vIdleInhibitors.clear();
    m_lSwitches.clear();
}

void CInputManager::onMouseMoved(wlr_pointer_motion_event* e) {
    static auto PSENS      = CConfigValue<Hyprlang::FLOAT>("general:sensitivity");
    static auto PNOACCEL   = CConfigValue<Hyprlang::INT>("input:force_no_accel");
    static auto PSENSTORAW = CConfigValue<Hyprlang::INT>("general:apply_sens_to_raw");

    const auto  DELTA = *PNOACCEL == 1 ? Vector2D(e->unaccel_dx, e->unaccel_dy) : Vector2D(e->delta_x, e->delta_y);

    if (*PSENSTORAW == 1)
        PROTO::relativePointer->sendRelativeMotion((uint64_t)e->time_msec * 1000, DELTA * *PSENS, Vector2D{e->unaccel_dx, e->unaccel_dy} * *PSENS);
    else
        PROTO::relativePointer->sendRelativeMotion((uint64_t)e->time_msec * 1000, DELTA, Vector2D{e->unaccel_dx, e->unaccel_dy});

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
    if (!g_pCompositor->m_pLastFocus || isConstrained())
        return;

    // todo: this sucks ass
    const auto PWINDOW = g_pCompositor->getWindowFromSurface(g_pCompositor->m_pLastFocus);
    const auto PLS     = g_pCompositor->getLayerSurfaceFromSurface(g_pCompositor->m_pLastFocus);

    timespec   now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    const auto LOCAL = getMouseCoordsInternal() - (PWINDOW ? PWINDOW->m_vRealPosition.goal() : (PLS ? Vector2D{PLS->geometry.x, PLS->geometry.y} : Vector2D{}));

    m_bEmptyFocusCursorSet = false;

    wlr_seat_pointer_notify_enter(g_pCompositor->m_sSeat.seat, g_pCompositor->m_pLastFocus, LOCAL.x, LOCAL.y);
    wlr_seat_pointer_notify_motion(g_pCompositor->m_sSeat.seat, now.tv_sec * 1000 + now.tv_nsec / 10000000, LOCAL.x, LOCAL.y);
}

void CInputManager::mouseMoveUnified(uint32_t time, bool refocus) {
    static auto PFOLLOWMOUSE      = CConfigValue<Hyprlang::INT>("input:follow_mouse");
    static auto PMOUSEREFOCUS     = CConfigValue<Hyprlang::INT>("input:mouse_refocus");
    static auto PMOUSEDPMS        = CConfigValue<Hyprlang::INT>("misc:mouse_move_enables_dpms");
    static auto PFOLLOWONDND      = CConfigValue<Hyprlang::INT>("misc:always_follow_on_dnd");
    static auto PFLOATBEHAVIOR    = CConfigValue<Hyprlang::INT>("input:float_switch_override_focus");
    static auto PMOUSEFOCUSMON    = CConfigValue<Hyprlang::INT>("misc:mouse_move_focuses_monitor");
    static auto PRESIZEONBORDER   = CConfigValue<Hyprlang::INT>("general:resize_on_border");
    static auto PRESIZECURSORICON = CConfigValue<Hyprlang::INT>("general:hover_icon_on_border");
    static auto PZOOMFACTOR       = CConfigValue<Hyprlang::FLOAT>("misc:cursor_zoom_factor");

    const auto  FOLLOWMOUSE = *PFOLLOWONDND && m_sDrag.drag ? 1 : *PFOLLOWMOUSE;

    m_pFoundSurfaceToFocus = nullptr;
    m_pFoundLSToFocus.reset();
    m_pFoundWindowToFocus.reset();
    wlr_surface* foundSurface = nullptr;
    Vector2D     surfaceCoords;
    Vector2D     surfacePos = Vector2D(-1337, -1337);
    PHLWINDOW    pFoundWindow;
    PHLLS        pFoundLayerSurface;

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
        PROTO::idle->onActivity();

    m_vLastCursorPosFloored = MOUSECOORDSFLOORED;

    const auto PMONITOR = g_pCompositor->getMonitorFromCursor();

    // this can happen if there are no displays hooked up to Hyprland
    if (PMONITOR == nullptr)
        return;

    if (*PZOOMFACTOR != 1.f)
        g_pHyprRenderer->damageMonitor(PMONITOR);

    if (!PMONITOR->solitaryClient.lock() && g_pHyprRenderer->shouldRenderCursor() && PMONITOR->output->software_cursor_locks > 0)
        g_pCompositor->scheduleFrameForMonitor(PMONITOR);

    PHLWINDOW forcedFocus = m_pForcedFocus.lock();

    if (!forcedFocus)
        forcedFocus = g_pCompositor->getForceFocus();

    if (forcedFocus) {
        pFoundWindow = forcedFocus;
        surfacePos   = pFoundWindow->m_vRealPosition.value();
        foundSurface = pFoundWindow->m_pWLSurface.wlr();
    }

    // constraints
    if (!g_pCompositor->m_sSeat.mouse.expired() && isConstrained()) {
        const auto SURF       = CWLSurface::surfaceFromWlr(g_pCompositor->m_pLastFocus);
        const auto CONSTRAINT = SURF->constraint();

        if (SURF && CONSTRAINT) {
            if (CONSTRAINT->isLocked()) {
                const auto HINT = CONSTRAINT->logicPositionHint();
                wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, HINT.x, HINT.y);
            } else {
                const auto RG           = CONSTRAINT->logicConstraintRegion();
                const auto CLOSEST      = RG.closestPoint(mouseCoords);
                const auto BOX          = SURF->getSurfaceBoxGlobal();
                const auto CLOSESTLOCAL = (CLOSEST - (BOX.has_value() ? BOX->pos() : Vector2D{})) * (SURF->getWindow() ? SURF->getWindow()->m_fX11SurfaceScaledBy : 1.0);

                wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, CLOSEST.x, CLOSEST.y);
                wlr_seat_pointer_send_motion(g_pCompositor->m_sSeat.seat, time, CLOSESTLOCAL.x, CLOSESTLOCAL.y);
                PROTO::relativePointer->sendRelativeMotion((uint64_t)time * 1000, {}, {});
            }

            return;

        } else
            Debug::log(ERR, "BUG THIS: Null SURF/CONSTRAINT in mouse refocus. Ignoring constraints. {:x} {:x}", (uintptr_t)SURF, (uintptr_t)CONSTRAINT.get());
    }

    // update stuff
    updateDragIcon();

    if (!m_sDrag.drag && !m_lCurrentlyHeldButtons.empty() && g_pCompositor->m_pLastFocus && m_pLastMouseSurface) {
        foundSurface       = m_pLastMouseSurface;
        pFoundLayerSurface = g_pCompositor->getLayerSurfaceFromSurface(foundSurface);
        if (pFoundLayerSurface) {
            surfacePos              = pFoundLayerSurface->position;
            m_bFocusHeldByButtons   = true;
            m_bRefocusHeldByButtons = refocus;
        } else {
            CInputPopup* foundPopup = m_sIMERelay.popupFromSurface(foundSurface);
            if (foundPopup) {
                surfacePos              = foundPopup->globalBox().pos();
                m_bFocusHeldByButtons   = true;
                m_bRefocusHeldByButtons = refocus;
            } else if (!g_pCompositor->m_pLastWindow.expired()) {
                foundSurface = m_pLastMouseSurface;
                pFoundWindow = g_pCompositor->m_pLastWindow.lock();

                surfaceCoords           = g_pCompositor->vectorToSurfaceLocal(mouseCoords, pFoundWindow, foundSurface);
                m_bFocusHeldByButtons   = true;
                m_bRefocusHeldByButtons = refocus;
            }
        }
    }

    g_pLayoutManager->getCurrentLayout()->onMouseMove(getMouseCoordsInternal());

    if (PMONITOR && PMONITOR != g_pCompositor->m_pLastMonitor && (*PMOUSEFOCUSMON || refocus) && m_pForcedFocus.expired())
        g_pCompositor->setActiveMonitor(PMONITOR);

    if (g_pSessionLockManager->isSessionLocked()) {
        const auto PSLS = PMONITOR ? g_pSessionLockManager->getSessionLockSurfaceForMonitor(PMONITOR->ID) : nullptr;

        if (!PSLS)
            return;

        foundSurface = PSLS->surface->surface();
        surfacePos   = PMONITOR->vecPosition;
    }

    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerPopupSurface(mouseCoords, PMONITOR, &surfaceCoords, &pFoundLayerSurface);

    // overlays are above fullscreen
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], &surfaceCoords, &pFoundLayerSurface);

    // also IME popups
    if (!foundSurface) {
        auto popup = g_pInputManager->m_sIMERelay.popupFromCoords(mouseCoords);
        if (popup) {
            foundSurface = popup->getWlrSurface();
            surfacePos   = popup->globalBox().pos();
        }
    }

    // also top layers
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], &surfaceCoords, &pFoundLayerSurface);

    // then, we check if the workspace doesnt have a fullscreen window
    const auto PWORKSPACE = PMONITOR->activeWorkspace;
    if (PWORKSPACE->m_bHasFullscreenWindow && !foundSurface && PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL) {
        pFoundWindow = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);

        if (!pFoundWindow) {
            // what the fuck, somehow happens occasionally??
            PWORKSPACE->m_bHasFullscreenWindow = false;
            return;
        }

        const auto PWINDOWIDEAL = g_pCompositor->vectorToWindowUnified(mouseCoords, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

        if (PWINDOWIDEAL &&
            ((PWINDOWIDEAL->m_bIsFloating && PWINDOWIDEAL->m_bCreatedOverFullscreen) /* floating over fullscreen */
             || (PMONITOR->activeSpecialWorkspace == PWINDOWIDEAL->m_pWorkspace) /* on an open special workspace */))
            pFoundWindow = PWINDOWIDEAL;

        if (!pFoundWindow->m_bIsX11) {
            foundSurface = g_pCompositor->vectorWindowToSurface(mouseCoords, pFoundWindow, surfaceCoords);
            surfacePos   = Vector2D(-1337, -1337);
        } else {
            foundSurface = pFoundWindow->m_pWLSurface.wlr();
            surfacePos   = pFoundWindow->m_vRealPosition.value();
        }
    }

    // then windows
    if (!foundSurface) {
        if (PWORKSPACE->m_bHasFullscreenWindow && PWORKSPACE->m_efFullscreenMode == FULLSCREEN_MAXIMIZED) {
            if (!foundSurface) {
                if (PMONITOR->activeSpecialWorkspace) {
                    pFoundWindow = g_pCompositor->vectorToWindowUnified(mouseCoords, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

                    if (pFoundWindow && !pFoundWindow->onSpecialWorkspace()) {
                        pFoundWindow = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);
                    }
                } else {
                    // if we have a maximized window, allow focusing on a bar or something if in reserved area.
                    if (g_pCompositor->isPointOnReservedArea(mouseCoords, PMONITOR)) {
                        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM], &surfaceCoords,
                                                                           &pFoundLayerSurface);
                    }

                    if (!foundSurface) {
                        pFoundWindow = g_pCompositor->vectorToWindowUnified(mouseCoords, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

                        if (!(pFoundWindow && pFoundWindow->m_bIsFloating && pFoundWindow->m_bCreatedOverFullscreen))
                            pFoundWindow = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);
                    }
                }
            }

        } else {
            pFoundWindow = g_pCompositor->vectorToWindowUnified(mouseCoords, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);
        }

        if (pFoundWindow) {
            if (!pFoundWindow->m_bIsX11) {
                foundSurface = g_pCompositor->vectorWindowToSurface(mouseCoords, pFoundWindow, surfaceCoords);
            } else {
                foundSurface = pFoundWindow->m_pWLSurface.wlr();
                surfacePos   = pFoundWindow->m_vRealPosition.value();
            }
        }
    }

    // then surfaces below
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM], &surfaceCoords, &pFoundLayerSurface);

    if (!foundSurface)
        foundSurface =
            g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND], &surfaceCoords, &pFoundLayerSurface);

    if (g_pCompositor->m_pLastMonitor->output->software_cursor_locks > 0)
        g_pCompositor->scheduleFrameForMonitor(g_pCompositor->m_pLastMonitor);

    if (!foundSurface) {
        if (!m_bEmptyFocusCursorSet) {
            if (*PRESIZEONBORDER && *PRESIZECURSORICON && m_eBorderIconDirection != BORDERICON_NONE) {
                m_eBorderIconDirection = BORDERICON_NONE;
                unsetCursorImage();
            }

            // TODO: maybe wrap?
            if (m_ecbClickBehavior == CLICKMODE_KILL)
                setCursorImageOverride("crosshair");
            else
                setCursorImageOverride("left_ptr");

            m_bEmptyFocusCursorSet = true;
        }

        wlr_seat_pointer_clear_focus(g_pCompositor->m_sSeat.seat);
        m_pLastMouseSurface = nullptr;

        if (refocus || g_pCompositor->m_pLastWindow.expired()) // if we are forcing a refocus, and we don't find a surface, clear the kb focus too!
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

    if (currentlyDraggedWindow.lock() && pFoundWindow != currentlyDraggedWindow) {
        wlr_seat_pointer_notify_enter(g_pCompositor->m_sSeat.seat, foundSurface, surfaceLocal.x, surfaceLocal.y);
        wlr_seat_pointer_notify_motion(g_pCompositor->m_sSeat.seat, time, surfaceLocal.x, surfaceLocal.y);
        return;
    }

    if (pFoundWindow && foundSurface == pFoundWindow->m_pWLSurface.wlr() && !m_bCursorImageOverridden) {
        const auto BOX = pFoundWindow->getWindowMainSurfaceBox();
        if (!VECINRECT(mouseCoords, BOX.x, BOX.y, BOX.x + BOX.width, BOX.y + BOX.height))
            setCursorImageOverride("left_ptr");
        else
            restoreCursorIconToApp();
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
            if (pFoundWindow != g_pCompositor->m_pLastWindow.lock() && g_pCompositor->m_pLastWindow.lock() &&
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
                m_pLastMouseSurface = foundSurface;
                wlr_seat_pointer_notify_enter(g_pCompositor->m_sSeat.seat, foundSurface, surfaceLocal.x, surfaceLocal.y);
            }

            if (FOLLOWMOUSE != 0 || pFoundWindow == g_pCompositor->m_pLastWindow)
                wlr_seat_pointer_notify_motion(g_pCompositor->m_sSeat.seat, time, surfaceLocal.x, surfaceLocal.y);

            m_bLastFocusOnLS = false;
            return; // don't enter any new surfaces
        } else {
            if (allowKeyboardRefocus && ((FOLLOWMOUSE != 3 && (*PMOUSEREFOCUS || m_pLastMouseFocus.lock() != pFoundWindow)) || refocus)) {
                if (m_pLastMouseFocus.lock() != pFoundWindow || g_pCompositor->m_pLastWindow.lock() != pFoundWindow || g_pCompositor->m_pLastFocus != foundSurface || refocus) {
                    m_pLastMouseFocus = pFoundWindow;

                    // TODO: this looks wrong. When over a popup, it constantly is switching.
                    // Temp fix until that's figured out. Otherwise spams windowrule lookups and other shit.
                    if (m_pLastMouseFocus.lock() != pFoundWindow || g_pCompositor->m_pLastWindow.lock() != pFoundWindow)
                        g_pCompositor->focusWindow(pFoundWindow, foundSurface);
                    else
                        g_pCompositor->focusSurface(foundSurface, pFoundWindow);
                }
            }
        }

        m_bLastFocusOnLS = false;
    } else {
        if (*PRESIZEONBORDER && *PRESIZECURSORICON && m_eBorderIconDirection != BORDERICON_NONE) {
            m_eBorderIconDirection = BORDERICON_NONE;
            unsetCursorImage();
        }

        if (pFoundLayerSurface && (pFoundLayerSurface->layerSurface->current.keyboard_interactive != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE) && FOLLOWMOUSE != 3 &&
            allowKeyboardRefocus) {
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

    PROTO::idle->onActivity();

    m_tmrLastCursorMovement.reset();

    if (e->state == WL_POINTER_BUTTON_STATE_PRESSED) {
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

    if (m_bFocusHeldByButtons && m_lCurrentlyHeldButtons.empty() && e->state == WL_POINTER_BUTTON_STATE_RELEASED) {
        if (m_bRefocusHeldByButtons)
            refocus();
        else
            simulateMouseMovement();

        m_bFocusHeldByButtons   = false;
        m_bRefocusHeldByButtons = false;
    }
}

void CInputManager::processMouseRequest(wlr_seat_pointer_request_set_cursor_event* e) {
    if (!cursorImageUnlocked())
        return;

    Debug::log(LOG, "cursorImage request: surface {:x}", (uintptr_t)e->surface);

    if (e->seat_client == g_pCompositor->m_sSeat.seat->pointer_state.focused_client) {

        if (e->surface != m_sCursorSurfaceInfo.wlSurface.wlr()) {
            m_sCursorSurfaceInfo.wlSurface.unassign();

            if (e->surface)
                m_sCursorSurfaceInfo.wlSurface.assign(e->surface);
        }

        if (e->surface) {
            m_sCursorSurfaceInfo.vHotspot = {e->hotspot_x, e->hotspot_y};
            m_sCursorSurfaceInfo.hidden   = false;
        } else {
            m_sCursorSurfaceInfo.vHotspot = {};
            m_sCursorSurfaceInfo.hidden   = true;
        }

        m_sCursorSurfaceInfo.name = "";

        m_sCursorSurfaceInfo.inUse = true;
        g_pHyprRenderer->setCursorSurface(e->surface, e->hotspot_x, e->hotspot_y);
    }
}

void CInputManager::restoreCursorIconToApp() {
    if (m_sCursorSurfaceInfo.inUse)
        return;

    if (m_sCursorSurfaceInfo.hidden) {
        g_pHyprRenderer->setCursorSurface(nullptr, 0, 0);
        return;
    }

    if (m_sCursorSurfaceInfo.name.empty()) {
        if (m_sCursorSurfaceInfo.wlSurface.exists())
            g_pHyprRenderer->setCursorSurface(m_sCursorSurfaceInfo.wlSurface.wlr(), m_sCursorSurfaceInfo.vHotspot.x, m_sCursorSurfaceInfo.vHotspot.y);
    } else {
        g_pHyprRenderer->setCursorFromName(m_sCursorSurfaceInfo.name);
    }

    m_sCursorSurfaceInfo.inUse = true;
}

void CInputManager::setCursorImageOverride(const std::string& name) {
    if (m_bCursorImageOverridden)
        return;

    m_sCursorSurfaceInfo.inUse = false;
    g_pHyprRenderer->setCursorFromName(name);
}

bool CInputManager::cursorImageUnlocked() {
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
    static auto PPASSMOUSE        = CConfigValue<Hyprlang::INT>("binds:pass_mouse_when_bound");
    const auto  PASS              = g_pKeybindManager->onMouseEvent(e);
    static auto PFOLLOWMOUSE      = CConfigValue<Hyprlang::INT>("input:follow_mouse");
    static auto PRESIZEONBORDER   = CConfigValue<Hyprlang::INT>("general:resize_on_border");
    static auto PBORDERSIZE       = CConfigValue<Hyprlang::INT>("general:border_size");
    static auto PBORDERGRABEXTEND = CConfigValue<Hyprlang::INT>("general:extend_border_grab_area");
    const auto  BORDER_GRAB_AREA  = *PRESIZEONBORDER ? *PBORDERSIZE + *PBORDERGRABEXTEND : 0;

    if (!PASS && !*PPASSMOUSE)
        return;

    const auto mouseCoords = g_pInputManager->getMouseCoordsInternal();
    const auto w           = g_pCompositor->vectorToWindowUnified(mouseCoords, ALLOW_FLOATING | RESERVED_EXTENTS | INPUT_EXTENTS);

    if (w && !m_bLastFocusOnLS && w->checkInputOnDecos(INPUT_TYPE_BUTTON, mouseCoords, e))
        return;

    // clicking on border triggers resize
    // TODO detect click on LS properly
    if (*PRESIZEONBORDER && !m_bLastFocusOnLS && e->state == WL_POINTER_BUTTON_STATE_PRESSED) {
        if (w && !w->m_bIsFullscreen) {
            const CBox real = {w->m_vRealPosition.value().x, w->m_vRealPosition.value().y, w->m_vRealSize.value().x, w->m_vRealSize.value().y};
            const CBox grab = {real.x - BORDER_GRAB_AREA, real.y - BORDER_GRAB_AREA, real.width + 2 * BORDER_GRAB_AREA, real.height + 2 * BORDER_GRAB_AREA};

            if ((grab.containsPoint(mouseCoords) && (!real.containsPoint(mouseCoords) || w->isInCurvedCorner(mouseCoords.x, mouseCoords.y))) && !w->hasPopupAt(mouseCoords)) {
                g_pKeybindManager->resizeWithBorder(e);
                return;
            }
        }
    }

    switch (e->state) {
        case WL_POINTER_BUTTON_STATE_PRESSED:
            if (*PFOLLOWMOUSE == 3) // don't refocus on full loose
                break;

            if ((g_pCompositor->m_sSeat.mouse.expired() || !isConstrained()) /* No constraints */
                && (w && g_pCompositor->m_pLastWindow.lock() != w) /* window should change */) {
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
            if (g_pCompositor->m_pLastWindow.lock() && g_pCompositor->m_pLastWindow->m_bIsFloating)
                g_pCompositor->changeWindowZOrder(g_pCompositor->m_pLastWindow.lock(), true);

            break;
        case WL_POINTER_BUTTON_STATE_RELEASED: break;
    }

    // notify app if we didnt handle it
    if (g_pCompositor->doesSeatAcceptInput(g_pCompositor->m_pLastFocus))
        wlr_seat_pointer_notify_button(g_pCompositor->m_sSeat.seat, e->time_msec, e->button, e->state);

    if (const auto PMON = g_pCompositor->getMonitorFromVector(mouseCoords); PMON != g_pCompositor->m_pLastMonitor && PMON)
        g_pCompositor->setActiveMonitor(PMON);
}

void CInputManager::processMouseDownKill(wlr_pointer_button_event* e) {
    switch (e->state) {
        case WL_POINTER_BUTTON_STATE_PRESSED: {
            const auto PWINDOW = g_pCompositor->vectorToWindowUnified(getMouseCoordsInternal(), RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

            if (!PWINDOW) {
                Debug::log(ERR, "Cannot kill invalid window!");
                break;
            }

            // kill the mf
            kill(PWINDOW->getPID(), SIGKILL);
            break;
        }
        case WL_POINTER_BUTTON_STATE_RELEASED: break;
        default: break;
    }

    // reset click behavior mode
    m_ecbClickBehavior = CLICKMODE_DEFAULT;
}

void CInputManager::onMouseWheel(wlr_pointer_axis_event* e) {
    static auto POFFWINDOWAXIS        = CConfigValue<Hyprlang::INT>("input:off_window_axis_events");
    static auto PINPUTSCROLLFACTOR    = CConfigValue<Hyprlang::FLOAT>("input:scroll_factor");
    static auto PTOUCHPADSCROLLFACTOR = CConfigValue<Hyprlang::FLOAT>("input:touchpad:scroll_factor");

    auto        factor = (*PTOUCHPADSCROLLFACTOR <= 0.f || e->source == WL_POINTER_AXIS_SOURCE_FINGER ? *PTOUCHPADSCROLLFACTOR : *PINPUTSCROLLFACTOR);

    const auto  EMAP = std::unordered_map<std::string, std::any>{{"event", e}};
    EMIT_HOOK_EVENT_CANCELLABLE("mouseAxis", EMAP);

    bool passEvent = g_pKeybindManager->onAxisEvent(e);

    PROTO::idle->onActivity();

    if (!passEvent)
        return;

    if (!m_bLastFocusOnLS) {
        const auto MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();
        const auto PWINDOW     = g_pCompositor->vectorToWindowUnified(MOUSECOORDS, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

        if (PWINDOW && PWINDOW->checkInputOnDecos(INPUT_TYPE_AXIS, MOUSECOORDS, e))
            return;

        if (PWINDOW && *POFFWINDOWAXIS != 1) {
            const auto BOX = PWINDOW->getWindowMainSurfaceBox();

            if (!BOX.containsPoint(MOUSECOORDS) && !PWINDOW->hasPopupAt(MOUSECOORDS)) {
                if (*POFFWINDOWAXIS == 0)
                    return;

                const auto TEMPCURX = std::clamp(MOUSECOORDS.x, BOX.x, BOX.x + BOX.w - 1);
                const auto TEMPCURY = std::clamp(MOUSECOORDS.y, BOX.y, BOX.y + BOX.h - 1);

                if (*POFFWINDOWAXIS == 3)
                    wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, TEMPCURX, TEMPCURY);

                wlr_seat_pointer_notify_motion(g_pCompositor->m_sSeat.seat, e->time_msec, TEMPCURX - BOX.x, TEMPCURY - BOX.y);
                wlr_seat_pointer_notify_frame(g_pCompositor->m_sSeat.seat);
            }
        }
    }

    wlr_seat_pointer_notify_axis(g_pCompositor->m_sSeat.seat, e->time_msec, e->orientation, factor * e->delta, std::round(factor * e->delta_discrete), e->source,
                                 WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL);
}

Vector2D CInputManager::getMouseCoordsInternal() {
    return Vector2D(g_pCompositor->m_sWLRCursor->x, g_pCompositor->m_sWLRCursor->y);
}

void CInputManager::newKeyboard(wlr_input_device* keyboard) {
    const auto PNEWKEYBOARD = m_vKeyboards.emplace_back(CKeyboard::create(wlr_keyboard_from_input_device(keyboard)));

    setupKeyboard(PNEWKEYBOARD);

    Debug::log(LOG, "New keyboard created, pointers Hypr: {:x} and WLR: {:x}", (uintptr_t)PNEWKEYBOARD.get(), (uintptr_t)keyboard);
}

void CInputManager::newVirtualKeyboard(SP<CVirtualKeyboardV1Resource> keyboard) {
    const auto PNEWKEYBOARD = m_vKeyboards.emplace_back(CVirtualKeyboard::create(keyboard));

    setupKeyboard(PNEWKEYBOARD);

    Debug::log(LOG, "New virtual keyboard created, pointers Hypr: {:x} and WLR: {:x}", (uintptr_t)PNEWKEYBOARD.get(), (uintptr_t)keyboard->wlr());
}

void CInputManager::setupKeyboard(SP<IKeyboard> keeb) {
    try {
        keeb->hlName = getNameForNewDevice(keeb->wlr()->base.name);
    } catch (std::exception& e) {
        Debug::log(ERR, "Keyboard had no name???"); // logic error
    }

    keeb->events.destroy.registerStaticListener(
        [this](void* owner, std::any data) {
            auto PKEEB = ((IKeyboard*)owner)->self.lock();

            if (!PKEEB)
                return;

            destroyKeyboard(PKEEB);
            Debug::log(LOG, "Destroyed keyboard {:x}", (uintptr_t)owner);
        },
        keeb.get());

    keeb->keyboardEvents.key.registerStaticListener(
        [this](void* owner, std::any data) {
            auto PKEEB = ((IKeyboard*)owner)->self.lock();
            onKeyboardKey(data, PKEEB);
        },
        keeb.get());

    keeb->keyboardEvents.modifiers.registerStaticListener(
        [this](void* owner, std::any data) {
            auto PKEEB = ((IKeyboard*)owner)->self.lock();
            onKeyboardMod(PKEEB);
        },
        keeb.get());

    keeb->keyboardEvents.keymap.registerStaticListener(
        [](void* owner, std::any data) {
            auto       PKEEB  = ((IKeyboard*)owner)->self.lock();
            const auto LAYOUT = PKEEB->getActiveLayout();

            g_pEventManager->postEvent(SHyprIPCEvent{"activelayout", PKEEB->hlName + "," + LAYOUT});
            EMIT_HOOK_EVENT("activeLayout", (std::vector<std::any>{PKEEB, LAYOUT}));
        },
        keeb.get());

    disableAllKeyboards(false);

    g_pCompositor->m_sSeat.keyboard = keeb;

    keeb->active = true;

    applyConfigToKeyboard(keeb);

    wlr_seat_set_keyboard(g_pCompositor->m_sSeat.seat, keeb->wlr());
}

void CInputManager::setKeyboardLayout() {
    for (auto& k : m_vKeyboards)
        applyConfigToKeyboard(k);

    g_pKeybindManager->updateXKBTranslationState();
}

void CInputManager::applyConfigToKeyboard(SP<IKeyboard> pKeyboard) {
    auto       devname = pKeyboard->hlName;

    const auto HASCONFIG = g_pConfigManager->deviceConfigExists(devname);

    Debug::log(LOG, "ApplyConfigToKeyboard for \"{}\", hasconfig: {}", devname, (int)HASCONFIG);

    const auto REPEATRATE  = g_pConfigManager->getDeviceInt(devname, "repeat_rate", "input:repeat_rate");
    const auto REPEATDELAY = g_pConfigManager->getDeviceInt(devname, "repeat_delay", "input:repeat_delay");

    const auto NUMLOCKON         = g_pConfigManager->getDeviceInt(devname, "numlock_by_default", "input:numlock_by_default");
    const auto RESOLVEBINDSBYSYM = g_pConfigManager->getDeviceInt(devname, "resolve_binds_by_sym", "input:resolve_binds_by_sym");

    const auto FILEPATH = g_pConfigManager->getDeviceString(devname, "kb_file", "input:kb_file");
    const auto RULES    = g_pConfigManager->getDeviceString(devname, "kb_rules", "input:kb_rules");
    const auto MODEL    = g_pConfigManager->getDeviceString(devname, "kb_model", "input:kb_model");
    const auto LAYOUT   = g_pConfigManager->getDeviceString(devname, "kb_layout", "input:kb_layout");
    const auto VARIANT  = g_pConfigManager->getDeviceString(devname, "kb_variant", "input:kb_variant");
    const auto OPTIONS  = g_pConfigManager->getDeviceString(devname, "kb_options", "input:kb_options");

    const auto ENABLED = HASCONFIG ? g_pConfigManager->getDeviceInt(devname, "enabled") : true;

    pKeyboard->enabled           = ENABLED;
    pKeyboard->resolveBindsBySym = RESOLVEBINDSBYSYM;

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

    wlr_keyboard_set_repeat_info(pKeyboard->wlr(), std::max(0, REPEATRATE), std::max(0, REPEATDELAY));

    pKeyboard->repeatDelay = REPEATDELAY;
    pKeyboard->repeatRate  = REPEATRATE;
    pKeyboard->numlockOn   = NUMLOCKON;
    pKeyboard->xkbFilePath = FILEPATH;

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
        pKeyboard->currentRules.layout  = "us";

        KEYMAP = xkb_keymap_new_from_names(CONTEXT, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
    }

    wlr_keyboard_set_keymap(pKeyboard->wlr(), KEYMAP);

    pKeyboard->updateXKBTranslationState();

    wlr_keyboard_modifiers wlrMods = {0};

    if (NUMLOCKON == 1) {
        // lock numlock
        const auto IDX = xkb_map_mod_get_index(KEYMAP, XKB_MOD_NAME_NUM);

        if (IDX != XKB_MOD_INVALID)
            wlrMods.locked |= (uint32_t)1 << IDX;
    }

    if (wlrMods.locked != 0)
        wlr_keyboard_notify_modifiers(pKeyboard->wlr(), 0, 0, wlrMods.locked, 0);

    xkb_keymap_unref(KEYMAP);
    xkb_context_unref(CONTEXT);

    const auto LAYOUTSTR = pKeyboard->getActiveLayout();

    g_pEventManager->postEvent(SHyprIPCEvent{"activelayout", pKeyboard->hlName + "," + LAYOUTSTR});
    EMIT_HOOK_EVENT("activeLayout", (std::vector<std::any>{pKeyboard, LAYOUTSTR}));

    Debug::log(LOG, "Set the keyboard layout to {} and variant to {} for keyboard \"{}\"", pKeyboard->currentRules.layout, pKeyboard->currentRules.variant, pKeyboard->hlName);
}

void CInputManager::newVirtualMouse(SP<CVirtualPointerV1Resource> mouse) {
    const auto PMOUSE = m_vPointers.emplace_back(CVirtualPointer::create(mouse));

    setupMouse(PMOUSE);

    Debug::log(LOG, "New virtual mouse created, pointer WLR: {:x}", (uintptr_t)mouse->wlr());
}

void CInputManager::newMouse(wlr_input_device* mouse) {
    const auto PMOUSE = m_vPointers.emplace_back(CMouse::create(wlr_pointer_from_input_device(mouse)));

    setupMouse(PMOUSE);

    Debug::log(LOG, "New mouse created, pointer WLR: {:x}", (uintptr_t)mouse);
}

void CInputManager::setupMouse(SP<IPointer> mauz) {
    try {
        mauz->hlName = getNameForNewDevice(mauz->wlr()->base.name);
    } catch (std::exception& e) {
        Debug::log(ERR, "Mouse had no name???"); // logic error
    }

    if (wlr_input_device_is_libinput(&mauz->wlr()->base)) {
        const auto LIBINPUTDEV = (libinput_device*)wlr_libinput_get_device_handle(&mauz->wlr()->base);

        Debug::log(LOG, "New mouse has libinput sens {:.2f} ({:.2f}) with accel profile {} ({})", libinput_device_config_accel_get_speed(LIBINPUTDEV),
                   libinput_device_config_accel_get_default_speed(LIBINPUTDEV), (int)libinput_device_config_accel_get_profile(LIBINPUTDEV),
                   (int)libinput_device_config_accel_get_default_profile(LIBINPUTDEV));
    }

    wlr_cursor_attach_input_device(g_pCompositor->m_sWLRCursor, &mauz->wlr()->base);

    mauz->connected = true;

    setPointerConfigs();

    mauz->events.destroy.registerStaticListener(
        [this](void* mouse, std::any data) {
            const auto PMOUSE = (IPointer*)mouse;

            if (!PMOUSE)
                return;

            destroyPointer(PMOUSE->self.lock());
        },
        mauz.get());

    g_pCompositor->m_sSeat.mouse = mauz;

    m_tmrLastCursorMovement.reset();
}

void CInputManager::setPointerConfigs() {
    for (auto& m : m_vPointers) {
        auto       devname = m->hlName;

        const auto HASCONFIG = g_pConfigManager->deviceConfigExists(devname);

        if (HASCONFIG) {
            const auto ENABLED = g_pConfigManager->getDeviceInt(devname, "enabled");
            if (ENABLED && !m->connected) {
                wlr_cursor_attach_input_device(g_pCompositor->m_sWLRCursor, &m->wlr()->base);
                m->connected = true;
            } else if (!ENABLED && m->connected) {
                wlr_cursor_detach_input_device(g_pCompositor->m_sWLRCursor, &m->wlr()->base);
                m->connected = false;
            }
        }

        if (wlr_input_device_is_libinput(&m->wlr()->base)) {
            const auto LIBINPUTDEV = (libinput_device*)wlr_libinput_get_device_handle(&m->wlr()->base);

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
            const auto SCROLLPOINTS = g_pConfigManager->getDeviceString(devname, "scroll_points", "input:scroll_points");

            if (ACCELPROFILE.empty()) {
                libinput_device_config_accel_set_profile(LIBINPUTDEV, libinput_device_config_accel_get_default_profile(LIBINPUTDEV));
            } else if (ACCELPROFILE == "adaptive") {
                libinput_device_config_accel_set_profile(LIBINPUTDEV, LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);
            } else if (ACCELPROFILE == "flat") {
                libinput_device_config_accel_set_profile(LIBINPUTDEV, LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT);
            } else if (ACCELPROFILE.starts_with("custom")) {
                CVarList accelValues = {ACCELPROFILE, 0, ' '};

                try {
                    double              accelStep = std::stod(accelValues[1]);
                    std::vector<double> accelPoints;
                    for (size_t i = 2; i < accelValues.size(); ++i) {
                        accelPoints.push_back(std::stod(accelValues[i]));
                    }

                    const auto CONFIG = libinput_config_accel_create(LIBINPUT_CONFIG_ACCEL_PROFILE_CUSTOM);

                    if (!SCROLLPOINTS.empty()) {
                        CVarList scrollValues = {SCROLLPOINTS, 0, ' '};
                        try {
                            double              scrollStep = std::stod(scrollValues[0]);
                            std::vector<double> scrollPoints;
                            for (size_t i = 1; i < scrollValues.size(); ++i) {
                                scrollPoints.push_back(std::stod(scrollValues[i]));
                            }

                            libinput_config_accel_set_points(CONFIG, LIBINPUT_ACCEL_TYPE_SCROLL, scrollStep, scrollPoints.size(), scrollPoints.data());
                        } catch (std::exception& e) { Debug::log(ERR, "Invalid values in scroll_points"); }
                    }

                    libinput_config_accel_set_points(CONFIG, LIBINPUT_ACCEL_TYPE_MOTION, accelStep, accelPoints.size(), accelPoints.data());
                    libinput_device_config_accel_apply(LIBINPUTDEV, CONFIG);
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

            Debug::log(LOG, "Applied config to mouse {}, sens {:.2f}", m->hlName, LIBINPUTSENS);
        }
    }
}

void CInputManager::destroyKeyboard(SP<IKeyboard> pKeyboard) {
    if (pKeyboard->xkbTranslationState)
        xkb_state_unref(pKeyboard->xkbTranslationState);
    pKeyboard->xkbTranslationState = nullptr;

    std::erase_if(m_vKeyboards, [pKeyboard](const auto& other) { return other == pKeyboard; });

    if (m_vKeyboards.size() > 0) {
        g_pCompositor->m_sSeat.keyboard         = m_vKeyboards.back();
        g_pCompositor->m_sSeat.keyboard->active = true;
        wlr_seat_set_keyboard(g_pCompositor->m_sSeat.seat, g_pCompositor->m_sSeat.keyboard->wlr());
    } else {
        g_pCompositor->m_sSeat.keyboard.reset();
        wlr_seat_set_keyboard(g_pCompositor->m_sSeat.seat, nullptr);
    }
}

void CInputManager::destroyPointer(SP<IPointer> mouse) {
    std::erase_if(m_vPointers, [mouse](const auto& other) { return other == mouse; });

    g_pCompositor->m_sSeat.mouse = m_vPointers.size() > 0 ? m_vPointers.front() : nullptr;

    if (!g_pCompositor->m_sSeat.mouse.expired())
        unconstrainMouse();
}

void CInputManager::destroyTouchDevice(SP<ITouch> touch) {
    Debug::log(LOG, "Touch device at {:x} removed", (uintptr_t)touch.get());

    std::erase_if(m_vTouches, [touch](const auto& other) { return other == touch; });
}

void CInputManager::onKeyboardKey(std::any event, SP<IKeyboard> pKeyboard) {
    if (!pKeyboard->enabled)
        return;

    const bool DISALLOWACTION = pKeyboard->isVirtual() && shouldIgnoreVirtualKeyboard(pKeyboard);

    const auto EMAP = std::unordered_map<std::string, std::any>{{"keyboard", pKeyboard}, {"event", event}};
    EMIT_HOOK_EVENT_CANCELLABLE("keyPress", EMAP);

    static auto PDPMS = CConfigValue<Hyprlang::INT>("misc:key_press_enables_dpms");
    if (*PDPMS && !g_pCompositor->m_bDPMSStateON) {
        // enable dpms
        g_pKeybindManager->dpms("on");
    }

    bool passEvent = DISALLOWACTION || g_pKeybindManager->onKeyEvent(event, pKeyboard);

    auto e = std::any_cast<IKeyboard::SKeyEvent>(event);

    PROTO::idle->onActivity();

    if (passEvent) {
        const auto IME = m_sIMERelay.m_pIME.lock();

        if (IME && IME->hasGrab() && !DISALLOWACTION) {
            IME->setKeyboard(pKeyboard->wlr());
            IME->sendKey(e.timeMs, e.keycode, e.state);
        } else {
            wlr_seat_set_keyboard(g_pCompositor->m_sSeat.seat, pKeyboard->wlr());
            wlr_seat_keyboard_notify_key(g_pCompositor->m_sSeat.seat, e.timeMs, e.keycode, e.state);
        }

        for (auto& k : m_vKeyboards) {
            k->updateLEDs();
        }
    }
}

void CInputManager::onKeyboardMod(SP<IKeyboard> pKeyboard) {
    if (!pKeyboard->enabled)
        return;

    const bool DISALLOWACTION = pKeyboard->isVirtual() && shouldIgnoreVirtualKeyboard(pKeyboard);

    const auto ALLMODS = accumulateModsFromAllKBs();
    const auto PWLRKB  = pKeyboard->wlr();

    auto       MODS = PWLRKB->modifiers;
    MODS.depressed  = ALLMODS;

    const auto IME = m_sIMERelay.m_pIME.lock();

    if (IME && IME->hasGrab() && !DISALLOWACTION) {
        IME->setKeyboard(PWLRKB);
        IME->sendMods(MODS.depressed, MODS.latched, MODS.locked, MODS.group);
    } else {
        wlr_seat_set_keyboard(g_pCompositor->m_sSeat.seat, PWLRKB);
        wlr_seat_keyboard_notify_modifiers(g_pCompositor->m_sSeat.seat, &MODS);
    }

    for (auto& k : m_vKeyboards) {
        k->updateLEDs();
    }

    if (PWLRKB->modifiers.group != pKeyboard->activeLayout) {
        pKeyboard->activeLayout = PWLRKB->modifiers.group;

        const auto LAYOUT = pKeyboard->getActiveLayout();

        pKeyboard->updateXKBTranslationState();

        g_pEventManager->postEvent(SHyprIPCEvent{"activelayout", pKeyboard->hlName + "," + LAYOUT});
        EMIT_HOOK_EVENT("activeLayout", (std::vector<std::any>{pKeyboard, LAYOUT}));
    }
}

bool CInputManager::shouldIgnoreVirtualKeyboard(SP<IKeyboard> pKeyboard) {
    if (!pKeyboard->isVirtual())
        return false;

    CVirtualKeyboard* vk = (CVirtualKeyboard*)pKeyboard.get();

    return !pKeyboard || (!m_sIMERelay.m_pIME.expired() && m_sIMERelay.m_pIME->grabClient() == vk->getClient());
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
            CBox box = {m_sDrag.pos.x - 2, m_sDrag.pos.y - 2, m_sDrag.dragIcon->surface->current.width + 4, m_sDrag.dragIcon->surface->current.height + 4};
            g_pHyprRenderer->damageBox(&box);
            m_sDrag.pos = getMouseCoordsInternal();
            break;
        }
        default: break;
    }
}

void CInputManager::unconstrainMouse() {
    if (g_pCompositor->m_sSeat.mouse.expired())
        return;

    for (auto& c : m_vConstraints) {
        const auto C = c.lock();

        if (!C)
            continue;

        if (!C->isActive())
            continue;

        C->deactivate();
    }
}

bool CInputManager::isConstrained() {
    for (auto& c : m_vConstraints) {
        const auto C = c.lock();

        if (!C)
            continue;

        if (!C->isActive() || C->owner()->wlr() != g_pCompositor->m_pLastFocus)
            continue;

        return true;
    }

    return false;
}

void CInputManager::updateCapabilities() {
    uint32_t caps = 0;

    if (!m_vKeyboards.empty())
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    if (!m_vPointers.empty())
        caps |= WL_SEAT_CAPABILITY_POINTER;
    if (!m_vTouches.empty())
        caps |= WL_SEAT_CAPABILITY_TOUCH;
    if (!m_lTabletTools.empty())
        caps |= WL_SEAT_CAPABILITY_POINTER;

    wlr_seat_set_capabilities(g_pCompositor->m_sSeat.seat, caps);
    m_uiCapabilities = caps;
}

uint32_t CInputManager::accumulateModsFromAllKBs() {

    uint32_t finalMask = 0;

    for (auto& kb : m_vKeyboards) {
        if (kb->isVirtual() && shouldIgnoreVirtualKeyboard(kb))
            continue;

        if (!kb->enabled)
            continue;

        finalMask |= wlr_keyboard_get_modifiers(kb->wlr());
    }

    return finalMask;
}

void CInputManager::disableAllKeyboards(bool virt) {

    for (auto& k : m_vKeyboards) {
        if (k->isVirtual() != virt)
            continue;

        k->active = false;
    }
}

void CInputManager::newTouchDevice(wlr_input_device* pDevice) {
    const auto PNEWDEV = m_vTouches.emplace_back(CTouchDevice::create(wlr_touch_from_input_device(pDevice)));

    try {
        PNEWDEV->hlName = getNameForNewDevice(pDevice->name);
    } catch (std::exception& e) {
        Debug::log(ERR, "Touch Device had no name???"); // logic error
    }

    setTouchDeviceConfigs(PNEWDEV);
    wlr_cursor_attach_input_device(g_pCompositor->m_sWLRCursor, pDevice);

    PNEWDEV->events.destroy.registerStaticListener(
        [this](void* owner, std::any data) {
            auto PDEV = ((ITouch*)owner)->self.lock();

            if (!PDEV)
                return;

            destroyTouchDevice(PDEV);
        },
        PNEWDEV.get());

    Debug::log(LOG, "New touch device added at {:x}", (uintptr_t)PNEWDEV.get());
}

void CInputManager::setTouchDeviceConfigs(SP<ITouch> dev) {
    auto setConfig = [&](SP<ITouch> PTOUCHDEV) -> void {
        if (wlr_input_device_is_libinput(&PTOUCHDEV->wlr()->base)) {
            const auto LIBINPUTDEV = (libinput_device*)wlr_libinput_get_device_handle(&PTOUCHDEV->wlr()->base);

            const auto ENABLED = g_pConfigManager->getDeviceInt(PTOUCHDEV->hlName, "enabled", "input:touchdevice:enabled");
            const auto mode    = ENABLED ? LIBINPUT_CONFIG_SEND_EVENTS_ENABLED : LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
            if (libinput_device_config_send_events_get_mode(LIBINPUTDEV) != mode)
                libinput_device_config_send_events_set_mode(LIBINPUTDEV, mode);

            const int ROTATION = std::clamp(g_pConfigManager->getDeviceInt(PTOUCHDEV->hlName, "transform", "input:touchdevice:transform"), 0, 7);
            Debug::log(LOG, "Setting calibration matrix for device {}", PTOUCHDEV->hlName);
            if (libinput_device_config_calibration_has_matrix(LIBINPUTDEV))
                libinput_device_config_calibration_set_matrix(LIBINPUTDEV, MATRICES[ROTATION]);

            auto       output     = g_pConfigManager->getDeviceString(PTOUCHDEV->hlName, "output", "input:touchdevice:output");
            bool       bound      = !output.empty() && output != STRVAL_EMPTY;
            const bool AUTODETECT = output == "[[Auto]]";
            if (!bound && AUTODETECT) {
                const auto DEFAULTOUTPUT = PTOUCHDEV->wlr()->output_name;
                if (DEFAULTOUTPUT) {
                    output = DEFAULTOUTPUT;
                    bound  = true;
                }
            }
            PTOUCHDEV->boundOutput = bound ? output : "";
            const auto PMONITOR    = bound ? g_pCompositor->getMonitorFromName(output) : nullptr;
            if (PMONITOR) {
                Debug::log(LOG, "Binding touch device {} to output {}", PTOUCHDEV->hlName, PMONITOR->szName);
                wlr_cursor_map_input_to_output(g_pCompositor->m_sWLRCursor, &PTOUCHDEV->wlr()->base, PMONITOR->output);
            } else if (bound)
                Debug::log(ERR, "Failed to bind touch device {} to output '{}': monitor not found", PTOUCHDEV->hlName, output);
        }
    };

    if (dev) {
        setConfig(dev);
        return;
    }

    for (auto& m : m_vTouches) {
        setConfig(m);
    }
}

void CInputManager::setTabletConfigs() {
    for (auto& t : m_lTablets) {
        if (wlr_input_device_is_libinput(t.wlrDevice)) {
            const auto LIBINPUTDEV = (libinput_device*)wlr_libinput_get_device_handle(t.wlrDevice);

            const auto RELINPUT = g_pConfigManager->getDeviceInt(t.name, "relative_input", "input:tablet:relative_input");
            t.relativeInput     = RELINPUT;

            const int ROTATION = std::clamp(g_pConfigManager->getDeviceInt(t.name, "transform", "input:tablet:transform"), 0, 7);
            Debug::log(LOG, "Setting calibration matrix for device {}", t.name);
            libinput_device_config_calibration_set_matrix(LIBINPUTDEV, MATRICES[ROTATION]);

            if (g_pConfigManager->getDeviceInt(t.name, "left_handed", "input:tablet:left_handed") == 0)
                libinput_device_config_left_handed_set(LIBINPUTDEV, 0);
            else
                libinput_device_config_left_handed_set(LIBINPUTDEV, 1);

            const auto OUTPUT   = g_pConfigManager->getDeviceString(t.name, "output", "input:tablet:output");
            const auto PMONITOR = g_pCompositor->getMonitorFromString(OUTPUT);
            if (!OUTPUT.empty() && OUTPUT != STRVAL_EMPTY && PMONITOR) {
                Debug::log(LOG, "Binding tablet {} to output {}", t.name, PMONITOR->szName);
                wlr_cursor_map_input_to_output(g_pCompositor->m_sWLRCursor, t.wlrDevice, PMONITOR->output);
                wlr_cursor_map_input_to_region(g_pCompositor->m_sWLRCursor, t.wlrDevice, nullptr);
                t.boundOutput = OUTPUT;
            } else if (!PMONITOR)
                Debug::log(ERR, "Failed to bind tablet {} to output '{}': monitor not found", t.name, OUTPUT);

            const auto REGION_POS  = g_pConfigManager->getDeviceVec(t.name, "region_position", "input:tablet:region_position");
            const auto REGION_SIZE = g_pConfigManager->getDeviceVec(t.name, "region_size", "input:tablet:region_size");
            auto       regionBox   = CBox{REGION_POS.x, REGION_POS.y, REGION_SIZE.x, REGION_SIZE.y};
            if (!regionBox.empty())
                wlr_cursor_map_input_to_region(g_pCompositor->m_sWLRCursor, t.wlrDevice, regionBox.pWlr());

            const auto ACTIVE_AREA_SIZE = g_pConfigManager->getDeviceVec(t.name, "active_area_size", "input:tablet:active_area_size");
            const auto ACTIVE_AREA_POS  = g_pConfigManager->getDeviceVec(t.name, "active_area_position", "input:tablet:active_area_position");
            if (ACTIVE_AREA_SIZE.x != 0 || ACTIVE_AREA_SIZE.y != 0) {
                t.activeArea = CBox{ACTIVE_AREA_POS.x / t.wlrTablet->width_mm, ACTIVE_AREA_POS.y / t.wlrTablet->height_mm,
                                    (ACTIVE_AREA_POS.x + ACTIVE_AREA_SIZE.x) / t.wlrTablet->width_mm, (ACTIVE_AREA_POS.y + ACTIVE_AREA_SIZE.y) / t.wlrTablet->height_mm};
            }
        }
    }
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
    g_pHyprRenderer->setCursorFromName(name);
    m_bCursorImageOverridden   = true;
    m_sCursorSurfaceInfo.inUse = false;
}

void CInputManager::unsetCursorImage() {
    if (!m_bCursorImageOverridden)
        return;

    m_bCursorImageOverridden = false;
    restoreCursorIconToApp();
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

    while (std::find_if(m_vKeyboards.begin(), m_vKeyboards.end(),
                        [&](const auto& other) { return other->hlName == proposedNewName + (dupeno == 0 ? "" : ("-" + std::to_string(dupeno))); }) != m_vKeyboards.end())
        dupeno++;

    while (std::find_if(m_vPointers.begin(), m_vPointers.end(),
                        [&](const auto& other) { return other->hlName == proposedNewName + (dupeno == 0 ? "" : ("-" + std::to_string(dupeno))); }) != m_vPointers.end())
        dupeno++;

    while (std::find_if(m_vTouches.begin(), m_vTouches.end(),
                        [&](const auto& other) { return other->hlName == proposedNewName + (dupeno == 0 ? "" : ("-" + std::to_string(dupeno))); }) != m_vTouches.end())
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

void CInputManager::releaseAllMouseButtons() {
    const auto buttonsCopy = m_lCurrentlyHeldButtons;

    if (g_pInputManager->m_sDrag.drag)
        return;

    for (auto& mb : buttonsCopy) {
        wlr_seat_pointer_notify_button(g_pCompositor->m_sSeat.seat, 0, mb, WL_POINTER_BUTTON_STATE_RELEASED);
    }

    m_lCurrentlyHeldButtons.clear();
}

void CInputManager::setCursorIconOnBorder(PHLWINDOW w) {
    // do not override cursor icons set by mouse binds
    if (g_pKeybindManager->m_bIsMouseBindActive) {
        m_eBorderIconDirection = BORDERICON_NONE;
        return;
    }

    static auto PEXTENDBORDERGRAB = CConfigValue<Hyprlang::INT>("general:extend_border_grab_area");
    const int   BORDERSIZE        = w->getRealBorderSize();
    const int   ROUNDING          = w->rounding();

    // give a small leeway (10 px) for corner icon
    const auto           CORNER           = ROUNDING + BORDERSIZE + 10;
    const auto           mouseCoords      = getMouseCoordsInternal();
    CBox                 box              = w->getWindowMainSurfaceBox();
    eBorderIconDirection direction        = BORDERICON_NONE;
    CBox                 boxFullGrabInput = {box.x - *PEXTENDBORDERGRAB - BORDERSIZE, box.y - *PEXTENDBORDERGRAB - BORDERSIZE, box.width + 2 * (*PEXTENDBORDERGRAB + BORDERSIZE),
                                             box.height + 2 * (*PEXTENDBORDERGRAB + BORDERSIZE)};

    if (w->hasPopupAt(mouseCoords))
        direction = BORDERICON_NONE;
    else if (!boxFullGrabInput.containsPoint(mouseCoords) || (!m_lCurrentlyHeldButtons.empty() && currentlyDraggedWindow.expired()))
        direction = BORDERICON_NONE;
    else {

        bool onDeco = false;

        for (auto& wd : w->m_dWindowDecorations) {
            if (!(wd->getDecorationFlags() & DECORATION_ALLOWS_MOUSE_INPUT))
                continue;

            if (g_pDecorationPositioner->getWindowDecorationBox(wd.get()).containsPoint(mouseCoords)) {
                onDeco = true;
                break;
            }
        }

        if (onDeco)
            direction = BORDERICON_NONE;
        else {
            if (box.containsPoint(mouseCoords)) {
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
