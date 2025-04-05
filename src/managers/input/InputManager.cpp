#include "InputManager.hpp"
#include "../../Compositor.hpp"
#include <aquamarine/output/Output.hpp>
#include <cstdint>
#include <hyprutils/math/Vector2D.hpp>
#include <ranges>
#include "../../config/ConfigValue.hpp"
#include "../../desktop/Window.hpp"
#include "../../desktop/LayerSurface.hpp"
#include "../../protocols/CursorShape.hpp"
#include "../../protocols/IdleInhibit.hpp"
#include "../../protocols/RelativePointer.hpp"
#include "../../protocols/PointerConstraints.hpp"
#include "../../protocols/IdleNotify.hpp"
#include "../../protocols/SessionLock.hpp"
#include "../../protocols/InputMethodV2.hpp"
#include "../../protocols/VirtualKeyboard.hpp"
#include "../../protocols/VirtualPointer.hpp"
#include "../../protocols/LayerShell.hpp"
#include "../../protocols/core/Seat.hpp"
#include "../../protocols/core/DataDevice.hpp"
#include "../../protocols/core/Compositor.hpp"
#include "../../protocols/XDGShell.hpp"

#include "../../devices/Mouse.hpp"
#include "../../devices/VirtualPointer.hpp"
#include "../../devices/Keyboard.hpp"
#include "../../devices/VirtualKeyboard.hpp"
#include "../../devices/TouchDevice.hpp"

#include "../../managers/PointerManager.hpp"
#include "../../managers/SeatManager.hpp"
#include "../../managers/KeybindManager.hpp"
#include "../../render/Renderer.hpp"
#include "../../managers/HookSystemManager.hpp"
#include "../../managers/EventManager.hpp"
#include "../../managers/LayoutManager.hpp"

#include <aquamarine/input/Input.hpp>

CInputManager::CInputManager() {
    m_sListeners.setCursorShape = PROTO::cursorShape->events.setShape.registerListener([this](std::any data) {
        if (!cursorImageUnlocked())
            return;

        auto event = std::any_cast<CCursorShapeProtocol::SSetShapeEvent>(data);

        if (!g_pSeatManager->state.pointerFocusResource)
            return;

        if (wl_resource_get_client(event.pMgr->resource()) != g_pSeatManager->state.pointerFocusResource->client())
            return;

        Debug::log(LOG, "cursorImage request: shape {} -> {}", (uint32_t)event.shape, event.shapeName);

        m_sCursorSurfaceInfo.wlSurface->unassign();
        m_sCursorSurfaceInfo.vHotspot = {};
        m_sCursorSurfaceInfo.name     = event.shapeName;
        m_sCursorSurfaceInfo.hidden   = false;

        m_sCursorSurfaceInfo.inUse = true;
        g_pHyprRenderer->setCursorFromName(m_sCursorSurfaceInfo.name);
    });

    m_sListeners.newIdleInhibitor   = PROTO::idleInhibit->events.newIdleInhibitor.registerListener([this](std::any data) { this->newIdleInhibitor(data); });
    m_sListeners.newVirtualKeyboard = PROTO::virtualKeyboard->events.newKeyboard.registerListener([this](std::any data) {
        this->newVirtualKeyboard(std::any_cast<SP<CVirtualKeyboardV1Resource>>(data));
        updateCapabilities();
    });
    m_sListeners.newVirtualMouse    = PROTO::virtualPointer->events.newPointer.registerListener([this](std::any data) {
        this->newVirtualMouse(std::any_cast<SP<CVirtualPointerV1Resource>>(data));
        updateCapabilities();
    });
    m_sListeners.setCursor          = g_pSeatManager->events.setCursor.registerListener([this](std::any d) { this->processMouseRequest(d); });

    m_sCursorSurfaceInfo.wlSurface = CWLSurface::create();
}

CInputManager::~CInputManager() {
    m_vConstraints.clear();
    m_vKeyboards.clear();
    m_vPointers.clear();
    m_vTouches.clear();
    m_vTablets.clear();
    m_vTabletTools.clear();
    m_vTabletPads.clear();
    m_vIdleInhibitors.clear();
    m_lSwitches.clear();
}

void CInputManager::onMouseMoved(IPointer::SMotionEvent e) {
    static auto PNOACCEL = CConfigValue<Hyprlang::INT>("input:force_no_accel");

    Vector2D    delta   = e.delta;
    Vector2D    unaccel = e.unaccel;

    if (e.device) {
        if (e.device->isTouchpad) {
            if (e.device->flipX) {
                delta.x   = -delta.x;
                unaccel.x = -unaccel.x;
            }
            if (e.device->flipY) {
                delta.y   = -delta.y;
                unaccel.y = -unaccel.y;
            }
        }
    }

    const auto DELTA = *PNOACCEL == 1 ? unaccel : delta;

    if (g_pSeatManager->isPointerFrameSkipped)
        g_pPointerManager->storeMovement((uint64_t)e.timeMs, DELTA, unaccel);
    else
        g_pPointerManager->setStoredMovement((uint64_t)e.timeMs, DELTA, unaccel);

    PROTO::relativePointer->sendRelativeMotion((uint64_t)e.timeMs * 1000, DELTA, unaccel);

    if (e.mouse)
        recheckMouseWarpOnMouseInput();

    g_pPointerManager->move(DELTA);

    mouseMoveUnified(e.timeMs, false, e.mouse);

    m_tmrLastCursorMovement.reset();

    m_bLastInputTouch = false;

    if (e.mouse)
        m_vLastMousePos = getMouseCoordsInternal();
}

void CInputManager::onMouseWarp(IPointer::SMotionAbsoluteEvent e) {
    g_pPointerManager->warpAbsolute(e.absolute, e.device);

    mouseMoveUnified(e.timeMs);

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
    const auto PWINDOW = g_pCompositor->getWindowFromSurface(g_pCompositor->m_pLastFocus.lock());
    const auto PLS     = g_pCompositor->getLayerSurfaceFromSurface(g_pCompositor->m_pLastFocus.lock());

    timespec   now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    const auto LOCAL = getMouseCoordsInternal() - (PWINDOW ? PWINDOW->m_vRealPosition->goal() : (PLS ? Vector2D{PLS->geometry.x, PLS->geometry.y} : Vector2D{}));

    m_bEmptyFocusCursorSet = false;

    g_pSeatManager->setPointerFocus(g_pCompositor->m_pLastFocus.lock(), LOCAL);
}

void CInputManager::mouseMoveUnified(uint32_t time, bool refocus, bool mouse) {
    m_bLastInputMouse = mouse;

    if (!g_pCompositor->m_bReadyToProcess || g_pCompositor->m_bIsShuttingDown || g_pCompositor->m_bUnsafeState)
        return;

    Vector2D const mouseCoords        = getMouseCoordsInternal();
    auto const     MOUSECOORDSFLOORED = mouseCoords.floor();

    if (MOUSECOORDSFLOORED == m_vLastCursorPosFloored && !refocus)
        return;

    static auto PFOLLOWMOUSE          = CConfigValue<Hyprlang::INT>("input:follow_mouse");
    static auto PFOLLOWMOUSETHRESHOLD = CConfigValue<Hyprlang::FLOAT>("input:follow_mouse_threshold");
    static auto PMOUSEREFOCUS         = CConfigValue<Hyprlang::INT>("input:mouse_refocus");
    static auto PFOLLOWONDND          = CConfigValue<Hyprlang::INT>("misc:always_follow_on_dnd");
    static auto PFLOATBEHAVIOR        = CConfigValue<Hyprlang::INT>("input:float_switch_override_focus");
    static auto PMOUSEFOCUSMON        = CConfigValue<Hyprlang::INT>("misc:mouse_move_focuses_monitor");
    static auto PRESIZEONBORDER       = CConfigValue<Hyprlang::INT>("general:resize_on_border");
    static auto PRESIZECURSORICON     = CConfigValue<Hyprlang::INT>("general:hover_icon_on_border");
    static auto PZOOMFACTOR           = CConfigValue<Hyprlang::FLOAT>("cursor:zoom_factor");

    const auto  FOLLOWMOUSE = *PFOLLOWONDND && PROTO::data->dndActive() ? 1 : *PFOLLOWMOUSE;

    if (FOLLOWMOUSE == 1 && m_tmrLastCursorMovement.getSeconds() < 0.5)
        m_fMousePosDelta += MOUSECOORDSFLOORED.distance(m_vLastCursorPosFloored);
    else
        m_fMousePosDelta = 0;

    m_pFoundSurfaceToFocus.reset();
    m_pFoundLSToFocus.reset();
    m_pFoundWindowToFocus.reset();
    SP<CWLSurfaceResource> foundSurface;
    Vector2D               surfaceCoords;
    Vector2D               surfacePos = Vector2D(-1337, -1337);
    PHLWINDOW              pFoundWindow;
    PHLLS                  pFoundLayerSurface;

    EMIT_HOOK_EVENT_CANCELLABLE("mouseMove", MOUSECOORDSFLOORED);

    m_vLastCursorPosFloored = MOUSECOORDSFLOORED;

    const auto PMONITOR = isLocked() && g_pCompositor->m_pLastMonitor ? g_pCompositor->m_pLastMonitor.lock() : g_pCompositor->getMonitorFromCursor();

    // this can happen if there are no displays hooked up to Hyprland
    if (PMONITOR == nullptr)
        return;

    if (*PZOOMFACTOR != 1.f)
        g_pHyprRenderer->damageMonitor(PMONITOR);

    bool skipFrameSchedule = PMONITOR->shouldSkipScheduleFrameOnMouseEvent();

    if (!PMONITOR->solitaryClient.lock() && g_pHyprRenderer->shouldRenderCursor() && g_pPointerManager->softwareLockedFor(PMONITOR->self.lock()) && !skipFrameSchedule)
        g_pCompositor->scheduleFrameForMonitor(PMONITOR, Aquamarine::IOutput::AQ_SCHEDULE_CURSOR_MOVE);

    // constraints
    if (!g_pSeatManager->mouse.expired() && isConstrained()) {
        const auto SURF       = CWLSurface::fromResource(g_pCompositor->m_pLastFocus.lock());
        const auto CONSTRAINT = SURF ? SURF->constraint() : nullptr;

        if (CONSTRAINT) {
            if (CONSTRAINT->isLocked()) {
                const auto HINT = CONSTRAINT->logicPositionHint();
                g_pCompositor->warpCursorTo(HINT, true);
            } else {
                const auto RG           = CONSTRAINT->logicConstraintRegion();
                const auto CLOSEST      = RG.closestPoint(mouseCoords);
                const auto BOX          = SURF->getSurfaceBoxGlobal();
                const auto CLOSESTLOCAL = (CLOSEST - (BOX.has_value() ? BOX->pos() : Vector2D{})) * (SURF->getWindow() ? SURF->getWindow()->m_fX11SurfaceScaledBy : 1.0);

                g_pCompositor->warpCursorTo(CLOSEST, true);
                g_pSeatManager->sendPointerMotion(time, CLOSESTLOCAL);
                PROTO::relativePointer->sendRelativeMotion((uint64_t)time * 1000, {}, {});
            }

            return;

        } else
            Debug::log(ERR, "BUG THIS: Null SURF/CONSTRAINT in mouse refocus. Ignoring constraints. {:x} {:x}", (uintptr_t)SURF.get(), (uintptr_t)CONSTRAINT.get());
    }

    if (PMONITOR != g_pCompositor->m_pLastMonitor && (*PMOUSEFOCUSMON || refocus) && m_pForcedFocus.expired())
        g_pCompositor->setActiveMonitor(PMONITOR);

    if (g_pSessionLockManager->isSessionLocked()) {

        // search for interactable abovelock surfaces
        for (auto& lsl : PMONITOR->m_aLayerSurfaceLayers | std::views::reverse) {
            foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &lsl, &surfaceCoords, &pFoundLayerSurface, true);

            if (foundSurface) break;
        }

        Vector2D surfacelocal = surfaceCoords;

        // focus sessionlock surface if no abovelock layer found
        if (!foundSurface) {
            const auto PSESSIONLOCKSURFACE = g_pSessionLockManager->getSessionLockSurfaceForMonitor(PMONITOR->ID);

            surfacelocal = mouseCoords - PMONITOR->vecPosition;
            foundSurface = PSESSIONLOCKSURFACE ? PSESSIONLOCKSURFACE->surface->surface() : nullptr;
        }

        g_pCompositor->focusSurface(foundSurface);
        g_pSeatManager->setPointerFocus(foundSurface, surfacelocal);
        g_pSeatManager->sendPointerMotion(time, surfacelocal);

        return;
    }

    PHLWINDOW forcedFocus = m_pForcedFocus.lock();

    if (!forcedFocus)
        forcedFocus = g_pCompositor->getForceFocus();

    if (forcedFocus) {
        pFoundWindow = forcedFocus;
        surfacePos   = pFoundWindow->m_vRealPosition->value();
        foundSurface = pFoundWindow->m_pWLSurface->resource();
    }

    // if we are holding a pointer button,
    // and we're not dnd-ing, don't refocus. Keep focus on last surface.
    if (!PROTO::data->dndActive() && !m_lCurrentlyHeldButtons.empty() && g_pCompositor->m_pLastFocus && g_pSeatManager->state.pointerFocus && !m_bHardInput) {
        foundSurface = g_pSeatManager->state.pointerFocus.lock();

        // IME popups aren't desktop-like elements
        // TODO: make them.
        CInputPopup* foundPopup = m_sIMERelay.popupFromSurface(foundSurface);
        if (foundPopup) {
            surfacePos              = foundPopup->globalBox().pos();
            m_bFocusHeldByButtons   = true;
            m_bRefocusHeldByButtons = refocus;
        } else {
            auto HLSurface = CWLSurface::fromResource(foundSurface);

            if (HLSurface) {
                const auto BOX = HLSurface->getSurfaceBoxGlobal();

                if (BOX) {
                    const auto PWINDOW = HLSurface->getWindow();
                    surfacePos         = BOX->pos();
                    pFoundLayerSurface = HLSurface->getLayer();
                    pFoundWindow       = !PWINDOW || PWINDOW->isHidden() ? g_pCompositor->m_pLastWindow.lock() : PWINDOW;
                } else // reset foundSurface, find one normally
                    foundSurface = nullptr;
            } else // reset foundSurface, find one normally
                foundSurface = nullptr;
        }
    }

    g_pLayoutManager->getCurrentLayout()->onMouseMove(getMouseCoordsInternal());

    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerPopupSurface(mouseCoords, PMONITOR, &surfaceCoords, &pFoundLayerSurface);

    // overlays are above fullscreen
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], &surfaceCoords, &pFoundLayerSurface);

    // also IME popups
    if (!foundSurface) {
        auto popup = g_pInputManager->m_sIMERelay.popupFromCoords(mouseCoords);
        if (popup) {
            foundSurface = popup->getSurface();
            surfacePos   = popup->globalBox().pos();
        }
    }

    // also top layers
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], &surfaceCoords, &pFoundLayerSurface);

    // then, we check if the workspace doesnt have a fullscreen window
    const auto PWORKSPACE   = PMONITOR->activeWorkspace;
    const auto PWINDOWIDEAL = g_pCompositor->vectorToWindowUnified(mouseCoords, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);
    if (PWORKSPACE->m_bHasFullscreenWindow && !foundSurface && PWORKSPACE->m_efFullscreenMode == FSMODE_FULLSCREEN) {
        pFoundWindow = PWORKSPACE->getFullscreenWindow();

        if (!pFoundWindow) {
            // what the fuck, somehow happens occasionally??
            PWORKSPACE->m_bHasFullscreenWindow = false;
            return;
        }

        if (PWINDOWIDEAL &&
            ((PWINDOWIDEAL->m_bIsFloating && PWINDOWIDEAL->m_bCreatedOverFullscreen) /* floating over fullscreen */
             || (PMONITOR->activeSpecialWorkspace == PWINDOWIDEAL->m_pWorkspace) /* on an open special workspace */))
            pFoundWindow = PWINDOWIDEAL;

        if (!pFoundWindow->m_bIsX11) {
            foundSurface = g_pCompositor->vectorWindowToSurface(mouseCoords, pFoundWindow, surfaceCoords);
            surfacePos   = Vector2D(-1337, -1337);
        } else {
            foundSurface = pFoundWindow->m_pWLSurface->resource();
            surfacePos   = pFoundWindow->m_vRealPosition->value();
        }
    }

    // then windows
    if (!foundSurface) {
        if (PWORKSPACE->m_bHasFullscreenWindow && PWORKSPACE->m_efFullscreenMode == FSMODE_MAXIMIZED) {
            if (!foundSurface) {
                if (PMONITOR->activeSpecialWorkspace) {
                    if (pFoundWindow != PWINDOWIDEAL)
                        pFoundWindow = g_pCompositor->vectorToWindowUnified(mouseCoords, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

                    if (pFoundWindow && !pFoundWindow->onSpecialWorkspace()) {
                        pFoundWindow = PWORKSPACE->getFullscreenWindow();
                    }
                } else {
                    // if we have a maximized window, allow focusing on a bar or something if in reserved area.
                    if (g_pCompositor->isPointOnReservedArea(mouseCoords, PMONITOR)) {
                        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM], &surfaceCoords,
                                                                           &pFoundLayerSurface);
                    }

                    if (!foundSurface) {
                        if (pFoundWindow != PWINDOWIDEAL)
                            pFoundWindow = g_pCompositor->vectorToWindowUnified(mouseCoords, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

                        if (!(pFoundWindow && pFoundWindow->m_bIsFloating && pFoundWindow->m_bCreatedOverFullscreen))
                            pFoundWindow = PWORKSPACE->getFullscreenWindow();
                    }
                }
            }

        } else {
            if (pFoundWindow != PWINDOWIDEAL)
                pFoundWindow = g_pCompositor->vectorToWindowUnified(mouseCoords, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);
        }

        if (pFoundWindow) {
            if (!pFoundWindow->m_bIsX11) {
                foundSurface = g_pCompositor->vectorWindowToSurface(mouseCoords, pFoundWindow, surfaceCoords);
                if (!foundSurface) {
                    foundSurface = pFoundWindow->m_pWLSurface->resource();
                    surfacePos   = pFoundWindow->m_vRealPosition->value();
                }
            } else {
                foundSurface = pFoundWindow->m_pWLSurface->resource();
                surfacePos   = pFoundWindow->m_vRealPosition->value();
            }
        }
    }

    // then surfaces below
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM], &surfaceCoords, &pFoundLayerSurface);

    if (!foundSurface)
        foundSurface =
            g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND], &surfaceCoords, &pFoundLayerSurface);

    if (g_pPointerManager->softwareLockedFor(PMONITOR->self.lock()) > 0 && !skipFrameSchedule)
        g_pCompositor->scheduleFrameForMonitor(g_pCompositor->m_pLastMonitor.lock(), Aquamarine::IOutput::AQ_SCHEDULE_CURSOR_MOVE);

    // FIXME: This will be disabled during DnD operations because we do not exactly follow the spec
    // xdg-popup grabs should be keyboard-only, while they are absolute in our case...
    if (g_pSeatManager->seatGrab && !g_pSeatManager->seatGrab->accepts(foundSurface) && !PROTO::data->dndActive()) {
        if (m_bHardInput || refocus) {
            g_pSeatManager->setGrab(nullptr);
            return; // setGrab will refocus
        } else {
            // we need to grab the last surface.
            foundSurface = g_pSeatManager->state.pointerFocus.lock();

            auto HLSurface = CWLSurface::fromResource(foundSurface);

            if (HLSurface) {
                const auto BOX = HLSurface->getSurfaceBoxGlobal();

                if (BOX.has_value())
                    surfacePos = BOX->pos();
            }
        }
    }

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

        g_pSeatManager->setPointerFocus(nullptr, {});

        if (refocus || g_pCompositor->m_pLastWindow.expired()) // if we are forcing a refocus, and we don't find a surface, clear the kb focus too!
            g_pCompositor->focusWindow(nullptr);

        return;
    }

    m_bEmptyFocusCursorSet = false;

    Vector2D surfaceLocal = surfacePos == Vector2D(-1337, -1337) ? surfaceCoords : mouseCoords - surfacePos;

    if (pFoundWindow && !pFoundWindow->m_bIsX11 && surfacePos != Vector2D(-1337, -1337)) {
        // calc for oversized windows... fucking bullshit.
        CBox geom = pFoundWindow->m_pXDGSurface->current.geometry;

        surfaceLocal = mouseCoords - surfacePos + geom.pos();
    }

    if (pFoundWindow && pFoundWindow->m_bIsX11) // for x11 force scale zero
        surfaceLocal = surfaceLocal * pFoundWindow->m_fX11SurfaceScaledBy;

    bool allowKeyboardRefocus = true;

    if (!refocus && g_pCompositor->m_pLastFocus) {
        const auto PLS = g_pCompositor->getLayerSurfaceFromSurface(g_pCompositor->m_pLastFocus.lock());

        if (PLS && PLS->layerSurface->current.interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
            allowKeyboardRefocus = false;
    }

    // set the values for use
    if (refocus) {
        m_pFoundLSToFocus      = pFoundLayerSurface;
        m_pFoundWindowToFocus  = pFoundWindow;
        m_pFoundSurfaceToFocus = foundSurface;
    }

    if (currentlyDraggedWindow.lock() && pFoundWindow != currentlyDraggedWindow) {
        g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);
        return;
    }

    if (pFoundWindow && foundSurface == pFoundWindow->m_pWLSurface->resource() && !m_bCursorImageOverridden) {
        const auto BOX = pFoundWindow->getWindowMainSurfaceBox();
        if (!VECINRECT(mouseCoords, BOX.x, BOX.y, BOX.x + BOX.width, BOX.y + BOX.height))
            setCursorImageOverride("left_ptr");
        else
            restoreCursorIconToApp();
    }

    if (pFoundWindow) {
        // change cursor icon if hovering over border
        if (*PRESIZEONBORDER && *PRESIZECURSORICON) {
            if (!pFoundWindow->isFullscreen() && !pFoundWindow->hasPopupAt(mouseCoords)) {
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
                g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);
            } else if (FOLLOWMOUSE == 2 || FOLLOWMOUSE == 3)
                g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);

            if (pFoundWindow == g_pCompositor->m_pLastWindow)
                g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);

            if (FOLLOWMOUSE != 0 || pFoundWindow == g_pCompositor->m_pLastWindow)
                g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);

            if (g_pSeatManager->state.pointerFocus == foundSurface)
                g_pSeatManager->sendPointerMotion(time, surfaceLocal);

            m_bLastFocusOnLS = false;
            return; // don't enter any new surfaces
        } else {
            if (allowKeyboardRefocus && ((FOLLOWMOUSE != 3 && (*PMOUSEREFOCUS || m_pLastMouseFocus.lock() != pFoundWindow)) || refocus)) {
                if (m_pLastMouseFocus.lock() != pFoundWindow || g_pCompositor->m_pLastWindow.lock() != pFoundWindow || g_pCompositor->m_pLastFocus != foundSurface || refocus) {
                    m_pLastMouseFocus = pFoundWindow;

                    // TODO: this looks wrong. When over a popup, it constantly is switching.
                    // Temp fix until that's figured out. Otherwise spams windowrule lookups and other shit.
                    if (m_pLastMouseFocus.lock() != pFoundWindow || g_pCompositor->m_pLastWindow.lock() != pFoundWindow) {
                        if (m_fMousePosDelta > *PFOLLOWMOUSETHRESHOLD || refocus)
                            g_pCompositor->focusWindow(pFoundWindow, foundSurface);
                    } else
                        g_pCompositor->focusSurface(foundSurface, pFoundWindow);
                }
            }
        }

        if (g_pSeatManager->state.keyboardFocus == nullptr)
            g_pCompositor->focusWindow(pFoundWindow, foundSurface);

        m_bLastFocusOnLS = false;
    } else {
        if (*PRESIZEONBORDER && *PRESIZECURSORICON && m_eBorderIconDirection != BORDERICON_NONE) {
            m_eBorderIconDirection = BORDERICON_NONE;
            unsetCursorImage();
        }

        if (pFoundLayerSurface && (pFoundLayerSurface->layerSurface->current.interactivity != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE) && FOLLOWMOUSE != 3 &&
            (allowKeyboardRefocus || pFoundLayerSurface->layerSurface->current.interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)) {
            g_pCompositor->focusSurface(foundSurface);
        }

        if (pFoundLayerSurface)
            m_bLastFocusOnLS = true;
    }

    g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);
    g_pSeatManager->sendPointerMotion(time, surfaceLocal);
}

void CInputManager::onMouseButton(IPointer::SButtonEvent e) {
    EMIT_HOOK_EVENT_CANCELLABLE("mouseButton", e);

    if (e.mouse)
        recheckMouseWarpOnMouseInput();

    m_tmrLastCursorMovement.reset();

    if (e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
        m_lCurrentlyHeldButtons.push_back(e.button);
    } else {
        if (std::find_if(m_lCurrentlyHeldButtons.begin(), m_lCurrentlyHeldButtons.end(), [&](const auto& other) { return other == e.button; }) == m_lCurrentlyHeldButtons.end())
            return;
        std::erase_if(m_lCurrentlyHeldButtons, [&](const auto& other) { return other == e.button; });
    }

    switch (m_ecbClickBehavior) {
        case CLICKMODE_DEFAULT: processMouseDownNormal(e); break;
        case CLICKMODE_KILL: processMouseDownKill(e); break;
        default: break;
    }

    if (m_bFocusHeldByButtons && m_lCurrentlyHeldButtons.empty() && e.state == WL_POINTER_BUTTON_STATE_RELEASED) {
        if (m_bRefocusHeldByButtons)
            refocus();
        else
            simulateMouseMovement();

        m_bFocusHeldByButtons   = false;
        m_bRefocusHeldByButtons = false;
    }
}

void CInputManager::processMouseRequest(std::any E) {
    if (!cursorImageUnlocked())
        return;

    auto e = std::any_cast<CSeatManager::SSetCursorEvent>(E);

    Debug::log(LOG, "cursorImage request: surface {:x}", (uintptr_t)e.surf.get());

    if (e.surf != m_sCursorSurfaceInfo.wlSurface->resource()) {
        m_sCursorSurfaceInfo.wlSurface->unassign();

        if (e.surf)
            m_sCursorSurfaceInfo.wlSurface->assign(e.surf);
    }

    if (e.surf) {
        m_sCursorSurfaceInfo.vHotspot = e.hotspot;
        m_sCursorSurfaceInfo.hidden   = false;
    } else {
        m_sCursorSurfaceInfo.vHotspot = {};
        m_sCursorSurfaceInfo.hidden   = true;
    }

    m_sCursorSurfaceInfo.name = "";

    m_sCursorSurfaceInfo.inUse = true;
    g_pHyprRenderer->setCursorSurface(m_sCursorSurfaceInfo.wlSurface, e.hotspot.x, e.hotspot.y);
}

void CInputManager::restoreCursorIconToApp() {
    if (m_sCursorSurfaceInfo.inUse)
        return;

    if (m_sCursorSurfaceInfo.hidden) {
        g_pHyprRenderer->setCursorSurface(nullptr, 0, 0);
        return;
    }

    if (m_sCursorSurfaceInfo.name.empty()) {
        if (m_sCursorSurfaceInfo.wlSurface->exists())
            g_pHyprRenderer->setCursorSurface(m_sCursorSurfaceInfo.wlSurface, m_sCursorSurfaceInfo.vHotspot.x, m_sCursorSurfaceInfo.vHotspot.y);
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

void CInputManager::processMouseDownNormal(const IPointer::SButtonEvent& e) {

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
    if (*PRESIZEONBORDER && !m_bLastFocusOnLS && e.state == WL_POINTER_BUTTON_STATE_PRESSED && (!w || !w->isX11OverrideRedirect())) {
        if (w && !w->isFullscreen()) {
            const CBox real = {w->m_vRealPosition->value().x, w->m_vRealPosition->value().y, w->m_vRealSize->value().x, w->m_vRealSize->value().y};
            const CBox grab = {real.x - BORDER_GRAB_AREA, real.y - BORDER_GRAB_AREA, real.width + 2 * BORDER_GRAB_AREA, real.height + 2 * BORDER_GRAB_AREA};

            if ((grab.containsPoint(mouseCoords) && (!real.containsPoint(mouseCoords) || w->isInCurvedCorner(mouseCoords.x, mouseCoords.y))) && !w->hasPopupAt(mouseCoords)) {
                g_pKeybindManager->resizeWithBorder(e);
                return;
            }
        }
    }

    switch (e.state) {
        case WL_POINTER_BUTTON_STATE_PRESSED: {
            if (*PFOLLOWMOUSE == 3) // don't refocus on full loose
                break;

            if ((g_pSeatManager->mouse.expired() || !isConstrained()) /* No constraints */
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
            if (!g_pSeatManager->state.pointerFocus)
                break;

            auto HLSurf = CWLSurface::fromResource(g_pSeatManager->state.pointerFocus.lock());

            if (HLSurf && HLSurf->getWindow())
                g_pCompositor->changeWindowZOrder(HLSurf->getWindow(), true);

            break;
        }
        case WL_POINTER_BUTTON_STATE_RELEASED: break;
    }

    // notify app if we didnt handle it
    g_pSeatManager->sendPointerButton(e.timeMs, e.button, e.state);

    if (const auto PMON = g_pCompositor->getMonitorFromVector(mouseCoords); PMON != g_pCompositor->m_pLastMonitor && PMON)
        g_pCompositor->setActiveMonitor(PMON);

    if (g_pSeatManager->seatGrab && e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
        m_bHardInput = true;
        simulateMouseMovement();
        m_bHardInput = false;
    }
}

void CInputManager::processMouseDownKill(const IPointer::SButtonEvent& e) {
    switch (e.state) {
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

void CInputManager::onMouseWheel(IPointer::SAxisEvent e) {
    static auto POFFWINDOWAXIS        = CConfigValue<Hyprlang::INT>("input:off_window_axis_events");
    static auto PINPUTSCROLLFACTOR    = CConfigValue<Hyprlang::FLOAT>("input:scroll_factor");
    static auto PTOUCHPADSCROLLFACTOR = CConfigValue<Hyprlang::FLOAT>("input:touchpad:scroll_factor");
    static auto PEMULATEDISCRETE      = CConfigValue<Hyprlang::INT>("input:emulate_discrete_scroll");
    static auto PFOLLOWMOUSE          = CConfigValue<Hyprlang::INT>("input:follow_mouse");

    const bool  ISTOUCHPADSCROLL = *PTOUCHPADSCROLLFACTOR <= 0.f || e.source == WL_POINTER_AXIS_SOURCE_FINGER;
    auto        factor           = ISTOUCHPADSCROLL ? *PTOUCHPADSCROLLFACTOR : *PINPUTSCROLLFACTOR;

    const auto  EMAP = std::unordered_map<std::string, std::any>{{"event", e}};
    EMIT_HOOK_EVENT_CANCELLABLE("mouseAxis", EMAP);

    if (e.mouse)
        recheckMouseWarpOnMouseInput();

    bool passEvent = g_pKeybindManager->onAxisEvent(e);

    if (!passEvent)
        return;

    if (!m_bLastFocusOnLS) {
        const auto MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();
        const auto PWINDOW     = g_pCompositor->vectorToWindowUnified(MOUSECOORDS, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

        if (PWINDOW) {
            if (PWINDOW->checkInputOnDecos(INPUT_TYPE_AXIS, MOUSECOORDS, e))
                return;

            if (*POFFWINDOWAXIS != 1) {
                const auto BOX = PWINDOW->getWindowMainSurfaceBox();

                if (!BOX.containsPoint(MOUSECOORDS) && !PWINDOW->hasPopupAt(MOUSECOORDS)) {
                    if (*POFFWINDOWAXIS == 0)
                        return;

                    const auto TEMPCURX = std::clamp(MOUSECOORDS.x, BOX.x, BOX.x + BOX.w - 1);
                    const auto TEMPCURY = std::clamp(MOUSECOORDS.y, BOX.y, BOX.y + BOX.h - 1);

                    if (*POFFWINDOWAXIS == 3)
                        g_pCompositor->warpCursorTo({TEMPCURX, TEMPCURY}, true);

                    g_pSeatManager->sendPointerMotion(e.timeMs, Vector2D{TEMPCURX, TEMPCURY} - BOX.pos());
                    g_pSeatManager->sendPointerFrame();
                }
            }

            if (g_pSeatManager->state.pointerFocus) {
                const auto PCURRWINDOW = g_pCompositor->getWindowFromSurface(g_pSeatManager->state.pointerFocus.lock());

                if (*PFOLLOWMOUSE == 1 && PCURRWINDOW && PWINDOW != PCURRWINDOW)
                    simulateMouseMovement();
            }
            factor = ISTOUCHPADSCROLL ? PWINDOW->getScrollTouchpad() : PWINDOW->getScrollMouse();
        }
    }

    double discrete = (e.deltaDiscrete != 0) ? (factor * e.deltaDiscrete / std::abs(e.deltaDiscrete)) : 0;
    double delta    = e.delta * factor;

    if (e.source == 0) {
        // if an application supports v120, it should ignore discrete anyways
        if ((*PEMULATEDISCRETE >= 1 && std::abs(e.deltaDiscrete) != 120) || *PEMULATEDISCRETE >= 2) {

            const int interval = factor != 0 ? std::round(120 * (1 / factor)) : 120;

            // reset the accumulator when timeout is reached or direction/axis has changed
            if (std::signbit(e.deltaDiscrete) != m_ScrollWheelState.lastEventSign || e.axis != m_ScrollWheelState.lastEventAxis ||
                e.timeMs - m_ScrollWheelState.lastEventTime > 500 /* 500ms taken from libinput default timeout */) {

                m_ScrollWheelState.accumulatedScroll = 0;
                // send 1 discrete on first event for responsiveness
                discrete = std::copysign(1, e.deltaDiscrete);
            } else
                discrete = 0;

            for (int ac = m_ScrollWheelState.accumulatedScroll; ac >= interval; ac -= interval) {
                discrete += std::copysign(1, e.deltaDiscrete);
                m_ScrollWheelState.accumulatedScroll -= interval;
            }

            m_ScrollWheelState.lastEventSign = std::signbit(e.deltaDiscrete);
            m_ScrollWheelState.lastEventAxis = e.axis;
            m_ScrollWheelState.lastEventTime = e.timeMs;
            m_ScrollWheelState.accumulatedScroll += std::abs(e.deltaDiscrete);

            delta = 15.0 * discrete * factor;
        }
    }

    int32_t value120      = std::round(factor * e.deltaDiscrete);
    int32_t deltaDiscrete = std::abs(discrete) != 0 && std::abs(discrete) < 1 ? std::copysign(1, discrete) : std::round(discrete);

    g_pSeatManager->sendPointerAxis(e.timeMs, e.axis, delta, deltaDiscrete, value120, e.source, WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL);
}

Vector2D CInputManager::getMouseCoordsInternal() {
    return g_pPointerManager->position();
}

void CInputManager::newKeyboard(SP<Aquamarine::IKeyboard> keyboard) {
    const auto PNEWKEYBOARD = m_vKeyboards.emplace_back(CKeyboard::create(keyboard));

    setupKeyboard(PNEWKEYBOARD);

    Debug::log(LOG, "New keyboard created, pointers Hypr: {:x} and AQ: {:x}", (uintptr_t)PNEWKEYBOARD.get(), (uintptr_t)keyboard.get());
}

void CInputManager::newVirtualKeyboard(SP<CVirtualKeyboardV1Resource> keyboard) {
    const auto PNEWKEYBOARD = m_vKeyboards.emplace_back(CVirtualKeyboard::create(keyboard));

    setupKeyboard(PNEWKEYBOARD);

    Debug::log(LOG, "New virtual keyboard created at {:x}", (uintptr_t)PNEWKEYBOARD.get());
}

void CInputManager::setupKeyboard(SP<IKeyboard> keeb) {
    static auto PDPMS = CConfigValue<Hyprlang::INT>("misc:key_press_enables_dpms");

    m_vHIDs.emplace_back(keeb);

    try {
        keeb->hlName = getNameForNewDevice(keeb->deviceName);
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

            if (PKEEB->enabled)
                PROTO::idle->onActivity();

            if (PKEEB->enabled && *PDPMS && !g_pCompositor->m_bDPMSStateON)
                g_pKeybindManager->dpms("on");
        },
        keeb.get());

    keeb->keyboardEvents.modifiers.registerStaticListener(
        [this](void* owner, std::any data) {
            auto PKEEB = ((IKeyboard*)owner)->self.lock();

            onKeyboardMod(PKEEB);

            if (PKEEB->enabled)
                PROTO::idle->onActivity();

            if (PKEEB->enabled && *PDPMS && !g_pCompositor->m_bDPMSStateON)
                g_pKeybindManager->dpms("on");
        },
        keeb.get());

    keeb->keyboardEvents.keymap.registerStaticListener(
        [](void* owner, std::any data) {
            auto       PKEEB  = ((IKeyboard*)owner)->self.lock();
            const auto LAYOUT = PKEEB->getActiveLayout();

            if (PKEEB == g_pSeatManager->keyboard) {
                g_pSeatManager->updateActiveKeyboardData();
                g_pKeybindManager->m_mKeyToCodeCache.clear();
            }

            g_pEventManager->postEvent(SHyprIPCEvent{"activelayout", PKEEB->hlName + "," + LAYOUT});
            EMIT_HOOK_EVENT("activeLayout", (std::vector<std::any>{PKEEB, LAYOUT}));
        },
        keeb.get());

    disableAllKeyboards(false);

    applyConfigToKeyboard(keeb);

    g_pSeatManager->setKeyboard(keeb);

    keeb->updateLEDs();
}

void CInputManager::setKeyboardLayout() {
    for (auto const& k : m_vKeyboards)
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

    pKeyboard->repeatRate  = std::max(0, REPEATRATE);
    pKeyboard->repeatDelay = std::max(0, REPEATDELAY);
    pKeyboard->numlockOn   = NUMLOCKON;
    pKeyboard->xkbFilePath = FILEPATH;

    pKeyboard->setKeymap(IKeyboard::SStringRuleNames{LAYOUT, MODEL, VARIANT, OPTIONS, RULES});

    const auto LAYOUTSTR = pKeyboard->getActiveLayout();

    g_pEventManager->postEvent(SHyprIPCEvent{"activelayout", pKeyboard->hlName + "," + LAYOUTSTR});
    EMIT_HOOK_EVENT("activeLayout", (std::vector<std::any>{pKeyboard, LAYOUTSTR}));

    Debug::log(LOG, "Set the keyboard layout to {} and variant to {} for keyboard \"{}\"", pKeyboard->currentRules.layout, pKeyboard->currentRules.variant, pKeyboard->hlName);
}

void CInputManager::newVirtualMouse(SP<CVirtualPointerV1Resource> mouse) {
    const auto PMOUSE = m_vPointers.emplace_back(CVirtualPointer::create(mouse));

    setupMouse(PMOUSE);

    Debug::log(LOG, "New virtual mouse created");
}

void CInputManager::newMouse(SP<Aquamarine::IPointer> mouse) {
    const auto PMOUSE = m_vPointers.emplace_back(CMouse::create(mouse));

    setupMouse(PMOUSE);

    Debug::log(LOG, "New mouse created, pointer AQ: {:x}", (uintptr_t)mouse.get());
}

void CInputManager::setupMouse(SP<IPointer> mauz) {
    m_vHIDs.emplace_back(mauz);

    try {
        mauz->hlName = getNameForNewDevice(mauz->deviceName);
    } catch (std::exception& e) {
        Debug::log(ERR, "Mouse had no name???"); // logic error
    }

    if (mauz->aq() && mauz->aq()->getLibinputHandle()) {
        const auto LIBINPUTDEV = mauz->aq()->getLibinputHandle();

        Debug::log(LOG, "New mouse has libinput sens {:.2f} ({:.2f}) with accel profile {} ({})", libinput_device_config_accel_get_speed(LIBINPUTDEV),
                   libinput_device_config_accel_get_default_speed(LIBINPUTDEV), (int)libinput_device_config_accel_get_profile(LIBINPUTDEV),
                   (int)libinput_device_config_accel_get_default_profile(LIBINPUTDEV));
    }

    g_pPointerManager->attachPointer(mauz);

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

    g_pSeatManager->setMouse(mauz);

    m_tmrLastCursorMovement.reset();
}

void CInputManager::setPointerConfigs() {
    for (auto const& m : m_vPointers) {
        auto       devname = m->hlName;

        const auto HASCONFIG = g_pConfigManager->deviceConfigExists(devname);

        if (HASCONFIG) {
            const auto ENABLED = g_pConfigManager->getDeviceInt(devname, "enabled");
            if (ENABLED && !m->connected) {
                g_pPointerManager->attachPointer(m);
                m->connected = true;
            } else if (!ENABLED && m->connected) {
                g_pPointerManager->detachPointer(m);
                m->connected = false;
            }
        }

        if (m->aq() && m->aq()->getLibinputHandle()) {
            const auto LIBINPUTDEV = m->aq()->getLibinputHandle();

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
                libinput_device_config_tap_set_enabled(LIBINPUTDEV,
                                                       g_pConfigManager->getDeviceInt(devname, "tap-to-click", "input:touchpad:tap-to-click") == 1 ? LIBINPUT_CONFIG_TAP_ENABLED :
                                                                                                                                                     LIBINPUT_CONFIG_TAP_DISABLED);

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

            m->flipX = g_pConfigManager->getDeviceInt(devname, "flip_x", "input:touchpad:flip_x") != 0;
            m->flipY = g_pConfigManager->getDeviceInt(devname, "flip_y", "input:touchpad:flip_y") != 0;

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

static void removeFromHIDs(WP<IHID> hid) {
    std::erase_if(g_pInputManager->m_vHIDs, [hid](const auto& e) { return e.expired() || e == hid; });
    g_pInputManager->updateCapabilities();
}

void CInputManager::destroyKeyboard(SP<IKeyboard> pKeyboard) {
    Debug::log(LOG, "Keyboard at {:x} removed", (uintptr_t)pKeyboard.get());

    std::erase_if(m_vKeyboards, [pKeyboard](const auto& other) { return other == pKeyboard; });

    if (m_vKeyboards.size() > 0) {
        bool found = false;
        for (auto const& k : m_vKeyboards | std::views::reverse) {
            if (!k)
                continue;

            g_pSeatManager->setKeyboard(k);
            found = true;
            break;
        }

        if (!found)
            g_pSeatManager->setKeyboard(nullptr);
    } else
        g_pSeatManager->setKeyboard(nullptr);

    removeFromHIDs(pKeyboard);
}

void CInputManager::destroyPointer(SP<IPointer> mouse) {
    Debug::log(LOG, "Pointer at {:x} removed", (uintptr_t)mouse.get());

    std::erase_if(m_vPointers, [mouse](const auto& other) { return other == mouse; });

    g_pSeatManager->setMouse(m_vPointers.size() > 0 ? m_vPointers.front() : nullptr);

    if (!g_pSeatManager->mouse.expired())
        unconstrainMouse();

    removeFromHIDs(mouse);
}

void CInputManager::destroyTouchDevice(SP<ITouch> touch) {
    Debug::log(LOG, "Touch device at {:x} removed", (uintptr_t)touch.get());

    std::erase_if(m_vTouches, [touch](const auto& other) { return other == touch; });

    removeFromHIDs(touch);
}

void CInputManager::destroyTablet(SP<CTablet> tablet) {
    Debug::log(LOG, "Tablet device at {:x} removed", (uintptr_t)tablet.get());

    std::erase_if(m_vTablets, [tablet](const auto& other) { return other == tablet; });

    removeFromHIDs(tablet);
}

void CInputManager::destroyTabletTool(SP<CTabletTool> tool) {
    Debug::log(LOG, "Tablet tool at {:x} removed", (uintptr_t)tool.get());

    std::erase_if(m_vTabletTools, [tool](const auto& other) { return other == tool; });

    removeFromHIDs(tool);
}

void CInputManager::destroyTabletPad(SP<CTabletPad> pad) {
    Debug::log(LOG, "Tablet pad at {:x} removed", (uintptr_t)pad.get());

    std::erase_if(m_vTabletPads, [pad](const auto& other) { return other == pad; });

    removeFromHIDs(pad);
}

void CInputManager::updateKeyboardsLeds(SP<IKeyboard> pKeyboard) {
    if (!pKeyboard)
        return;

    std::optional<uint32_t> leds = pKeyboard->getLEDs();

    if (!leds.has_value())
        return;

    for (auto const& k : m_vKeyboards) {
        k->updateLEDs(leds.value());
    }
}

void CInputManager::onKeyboardKey(std::any event, SP<IKeyboard> pKeyboard) {
    if (!pKeyboard->enabled)
        return;

    const bool DISALLOWACTION = pKeyboard->isVirtual() && shouldIgnoreVirtualKeyboard(pKeyboard);

    const auto EMAP = std::unordered_map<std::string, std::any>{{"keyboard", pKeyboard}, {"event", event}};
    EMIT_HOOK_EVENT_CANCELLABLE("keyPress", EMAP);

    bool passEvent = DISALLOWACTION || g_pKeybindManager->onKeyEvent(event, pKeyboard);

    auto e = std::any_cast<IKeyboard::SKeyEvent>(event);

    if (passEvent) {
        const auto IME = m_sIMERelay.m_pIME.lock();

        if (IME && IME->hasGrab() && !DISALLOWACTION) {
            IME->setKeyboard(pKeyboard);
            IME->sendKey(e.timeMs, e.keycode, e.state);
        } else {
            g_pSeatManager->setKeyboard(pKeyboard);
            g_pSeatManager->sendKeyboardKey(e.timeMs, e.keycode, e.state);
        }

        updateKeyboardsLeds(pKeyboard);
    }
}

void CInputManager::onKeyboardMod(SP<IKeyboard> pKeyboard) {
    if (!pKeyboard->enabled)
        return;

    const bool DISALLOWACTION = pKeyboard->isVirtual() && shouldIgnoreVirtualKeyboard(pKeyboard);

    const auto ALLMODS = accumulateModsFromAllKBs();

    auto       MODS = pKeyboard->modifiersState;
    MODS.depressed  = ALLMODS;

    const auto IME = m_sIMERelay.m_pIME.lock();

    if (IME && IME->hasGrab() && !DISALLOWACTION) {
        IME->setKeyboard(pKeyboard);
        IME->sendMods(MODS.depressed, MODS.latched, MODS.locked, MODS.group);
    } else {
        g_pSeatManager->setKeyboard(pKeyboard);
        g_pSeatManager->sendKeyboardMods(MODS.depressed, MODS.latched, MODS.locked, MODS.group);
    }

    updateKeyboardsLeds(pKeyboard);

    if (pKeyboard->modifiersState.group != pKeyboard->activeLayout) {
        pKeyboard->activeLayout = pKeyboard->modifiersState.group;

        const auto LAYOUT = pKeyboard->getActiveLayout();

        Debug::log(LOG, "LAYOUT CHANGED TO {} GROUP {}", LAYOUT, MODS.group);

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

void CInputManager::refocusLastWindow(PHLMONITOR pMonitor) {
    if (!pMonitor) {
        refocus();
        return;
    }

    Vector2D               surfaceCoords;
    PHLLS                  pFoundLayerSurface;
    SP<CWLSurfaceResource> foundSurface = nullptr;

    g_pInputManager->releaseAllMouseButtons();

    // first try for an exclusive layer
    if (!m_dExclusiveLSes.empty())
        foundSurface = m_dExclusiveLSes[m_dExclusiveLSes.size() - 1]->surface->resource();

    // then any surfaces above windows on the same monitor
    if (!foundSurface) {
        foundSurface = g_pCompositor->vectorToLayerSurface(g_pInputManager->getMouseCoordsInternal(), &pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY],
                                                           &surfaceCoords, &pFoundLayerSurface);
        if (pFoundLayerSurface && pFoundLayerSurface->interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND)
            foundSurface = nullptr;
    }

    if (!foundSurface) {
        foundSurface = g_pCompositor->vectorToLayerSurface(g_pInputManager->getMouseCoordsInternal(), &pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP],
                                                           &surfaceCoords, &pFoundLayerSurface);
        if (pFoundLayerSurface && pFoundLayerSurface->interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND)
            foundSurface = nullptr;
    }

    if (!foundSurface && g_pCompositor->m_pLastWindow.lock() && g_pCompositor->m_pLastWindow->m_pWorkspace && g_pCompositor->m_pLastWindow->m_pWorkspace->isVisibleNotCovered()) {
        // then the last focused window if we're on the same workspace as it
        const auto PLASTWINDOW = g_pCompositor->m_pLastWindow.lock();
        g_pCompositor->focusWindow(PLASTWINDOW);
    } else {
        // otherwise fall back to a normal refocus.

        if (foundSurface && !foundSurface->hlSurface->keyboardFocusable()) {
            const auto PLASTWINDOW = g_pCompositor->m_pLastWindow.lock();
            g_pCompositor->focusWindow(PLASTWINDOW);
        }

        refocus();
    }
}

void CInputManager::unconstrainMouse() {
    if (g_pSeatManager->mouse.expired())
        return;

    for (auto const& c : m_vConstraints) {
        const auto C = c.lock();

        if (!C)
            continue;

        if (!C->isActive())
            continue;

        C->deactivate();
    }
}

bool CInputManager::isConstrained() {
    return std::any_of(m_vConstraints.begin(), m_vConstraints.end(), [](auto const& c) {
        const auto constraint = c.lock();
        return constraint && constraint->isActive() && constraint->owner()->resource() == g_pCompositor->m_pLastFocus;
    });
}

bool CInputManager::isLocked() {
    if (!isConstrained())
        return false;

    const auto SURF       = CWLSurface::fromResource(g_pCompositor->m_pLastFocus.lock());
    const auto CONSTRAINT = SURF ? SURF->constraint() : nullptr;

    return CONSTRAINT && CONSTRAINT->isLocked();
}

void CInputManager::updateCapabilities() {
    uint32_t caps = 0;

    for (auto const& h : m_vHIDs) {
        if (h.expired())
            continue;

        caps |= h->getCapabilities();
    }

    g_pSeatManager->updateCapabilities(caps);
    m_uiCapabilities = caps;
}

uint32_t CInputManager::accumulateModsFromAllKBs() {

    uint32_t finalMask = 0;

    for (auto const& kb : m_vKeyboards) {
        if (kb->isVirtual() && shouldIgnoreVirtualKeyboard(kb))
            continue;

        if (!kb->enabled)
            continue;

        finalMask |= kb->getModifiers();
    }

    return finalMask;
}

void CInputManager::disableAllKeyboards(bool virt) {

    for (auto const& k : m_vKeyboards) {
        if (k->isVirtual() != virt)
            continue;

        k->active = false;
    }
}

void CInputManager::newTouchDevice(SP<Aquamarine::ITouch> pDevice) {
    const auto PNEWDEV = m_vTouches.emplace_back(CTouchDevice::create(pDevice));
    m_vHIDs.emplace_back(PNEWDEV);

    try {
        PNEWDEV->hlName = getNameForNewDevice(PNEWDEV->deviceName);
    } catch (std::exception& e) {
        Debug::log(ERR, "Touch Device had no name???"); // logic error
    }

    setTouchDeviceConfigs(PNEWDEV);
    g_pPointerManager->attachTouch(PNEWDEV);

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
    auto setConfig = [](SP<ITouch> PTOUCHDEV) -> void {
        if (PTOUCHDEV->aq() && PTOUCHDEV->aq()->getLibinputHandle()) {
            const auto LIBINPUTDEV = PTOUCHDEV->aq()->getLibinputHandle();

            const auto ENABLED = g_pConfigManager->getDeviceInt(PTOUCHDEV->hlName, "enabled", "input:touchdevice:enabled");
            const auto mode    = ENABLED ? LIBINPUT_CONFIG_SEND_EVENTS_ENABLED : LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
            if (libinput_device_config_send_events_get_mode(LIBINPUTDEV) != mode)
                libinput_device_config_send_events_set_mode(LIBINPUTDEV, mode);

            if (libinput_device_config_calibration_has_matrix(LIBINPUTDEV)) {
                Debug::log(LOG, "Setting calibration matrix for device {}", PTOUCHDEV->hlName);
                // default value of transform being -1 means it's unset.
                const int ROTATION = std::clamp(g_pConfigManager->getDeviceInt(PTOUCHDEV->hlName, "transform", "input:touchdevice:transform"), -1, 7);
                if (ROTATION > -1)
                    libinput_device_config_calibration_set_matrix(LIBINPUTDEV, MATRICES[ROTATION]);
            }

            auto       output     = g_pConfigManager->getDeviceString(PTOUCHDEV->hlName, "output", "input:touchdevice:output");
            bool       bound      = !output.empty() && output != STRVAL_EMPTY;
            const bool AUTODETECT = output == "[[Auto]]";
            if (!bound && AUTODETECT) {
                // FIXME:
                // const auto DEFAULTOUTPUT = PTOUCHDEV->wlr()->output_name;
                // if (DEFAULTOUTPUT) {
                //     output = DEFAULTOUTPUT;
                //     bound  = true;
                // }
            }
            PTOUCHDEV->boundOutput = bound ? output : "";
            const auto PMONITOR    = bound ? g_pCompositor->getMonitorFromName(output) : nullptr;
            if (PMONITOR) {
                Debug::log(LOG, "Binding touch device {} to output {}", PTOUCHDEV->hlName, PMONITOR->szName);
                // wlr_cursor_map_input_to_output(g_pCompositor->m_sWLRCursor, &PTOUCHDEV->wlr()->base, PMONITOR->output);
            } else if (bound)
                Debug::log(ERR, "Failed to bind touch device {} to output '{}': monitor not found", PTOUCHDEV->hlName, output);
        }
    };

    if (dev) {
        setConfig(dev);
        return;
    }

    for (auto const& m : m_vTouches) {
        setConfig(m);
    }
}

void CInputManager::setTabletConfigs() {
    for (auto const& t : m_vTablets) {
        if (t->aq()->getLibinputHandle()) {
            const auto NAME        = t->hlName;
            const auto LIBINPUTDEV = t->aq()->getLibinputHandle();

            const auto RELINPUT = g_pConfigManager->getDeviceInt(NAME, "relative_input", "input:tablet:relative_input");
            t->relativeInput    = RELINPUT;

            const int ROTATION = std::clamp(g_pConfigManager->getDeviceInt(NAME, "transform", "input:tablet:transform"), -1, 7);
            Debug::log(LOG, "Setting calibration matrix for device {}", NAME);
            if (ROTATION > -1)
                libinput_device_config_calibration_set_matrix(LIBINPUTDEV, MATRICES[ROTATION]);

            if (g_pConfigManager->getDeviceInt(NAME, "left_handed", "input:tablet:left_handed") == 0)
                libinput_device_config_left_handed_set(LIBINPUTDEV, 0);
            else
                libinput_device_config_left_handed_set(LIBINPUTDEV, 1);

            const auto OUTPUT = g_pConfigManager->getDeviceString(NAME, "output", "input:tablet:output");
            if (OUTPUT != STRVAL_EMPTY) {
                Debug::log(LOG, "Binding tablet {} to output {}", NAME, OUTPUT);
                t->boundOutput = OUTPUT;
            } else
                t->boundOutput = "";

            const auto REGION_POS  = g_pConfigManager->getDeviceVec(NAME, "region_position", "input:tablet:region_position");
            const auto REGION_SIZE = g_pConfigManager->getDeviceVec(NAME, "region_size", "input:tablet:region_size");
            t->boundBox            = {REGION_POS, REGION_SIZE};

            const auto ABSOLUTE_REGION_POS = g_pConfigManager->getDeviceInt(NAME, "absolute_region_position", "input:tablet:absolute_region_position");
            t->absolutePos                 = ABSOLUTE_REGION_POS;

            const auto ACTIVE_AREA_SIZE = g_pConfigManager->getDeviceVec(NAME, "active_area_size", "input:tablet:active_area_size");
            const auto ACTIVE_AREA_POS  = g_pConfigManager->getDeviceVec(NAME, "active_area_position", "input:tablet:active_area_position");
            if (ACTIVE_AREA_SIZE.x != 0 || ACTIVE_AREA_SIZE.y != 0) {
                t->activeArea = CBox{ACTIVE_AREA_POS.x / t->aq()->physicalSize.x, ACTIVE_AREA_POS.y / t->aq()->physicalSize.y,
                                     (ACTIVE_AREA_POS.x + ACTIVE_AREA_SIZE.x) / t->aq()->physicalSize.x, (ACTIVE_AREA_POS.y + ACTIVE_AREA_SIZE.y) / t->aq()->physicalSize.y};
            }
        }
    }
}

void CInputManager::newSwitch(SP<Aquamarine::ISwitch> pDevice) {
    const auto PNEWDEV = &m_lSwitches.emplace_back();
    PNEWDEV->pDevice   = pDevice;

    Debug::log(LOG, "New switch with name \"{}\" added", pDevice->getName());

    PNEWDEV->listeners.destroy = pDevice->events.destroy.registerListener([this, PNEWDEV](std::any d) { destroySwitch(PNEWDEV); });

    PNEWDEV->listeners.fire = pDevice->events.fire.registerListener([PNEWDEV](std::any d) {
        const auto NAME = PNEWDEV->pDevice->getName();
        const auto E    = std::any_cast<Aquamarine::ISwitch::SFireEvent>(d);

        Debug::log(LOG, "Switch {} fired, triggering binds.", NAME);

        g_pKeybindManager->onSwitchEvent(NAME);

        if (E.enable) {
            Debug::log(LOG, "Switch {} turn on, triggering binds.", NAME);
            g_pKeybindManager->onSwitchOnEvent(NAME);
        } else {
            Debug::log(LOG, "Switch {} turn off, triggering binds.", NAME);
            g_pKeybindManager->onSwitchOffEvent(NAME);
        }
    });
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

    auto makeNewName = [&]() { return (proposedNewName.empty() ? "unknown-device" : proposedNewName) + (dupeno == 0 ? "" : ("-" + std::to_string(dupeno))); };

    while (std::find_if(m_vHIDs.begin(), m_vHIDs.end(), [&](const auto& other) { return other->hlName == makeNewName(); }) != m_vHIDs.end())
        dupeno++;

    return makeNewName();
}

void CInputManager::releaseAllMouseButtons() {
    const auto buttonsCopy = m_lCurrentlyHeldButtons;

    if (PROTO::data->dndActive())
        return;

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    for (auto const& mb : buttonsCopy) {
        g_pSeatManager->sendPointerButton(now.tv_sec * 1000 + now.tv_nsec / 1000000, mb, WL_POINTER_BUTTON_STATE_RELEASED);
    }

    m_lCurrentlyHeldButtons.clear();
}

void CInputManager::setCursorIconOnBorder(PHLWINDOW w) {
    // do not override cursor icons set by mouse binds
    if (g_pInputManager->currentlyDraggedWindow.expired()) {
        m_eBorderIconDirection = BORDERICON_NONE;
        return;
    }

    // ignore X11 OR windows, they shouldn't be touched
    if (w->m_bIsX11 && w->isX11OverrideRedirect())
        return;

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

        for (auto const& wd : w->m_dWindowDecorations) {
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

void CInputManager::recheckMouseWarpOnMouseInput() {
    static auto PWARPFORNONMOUSE = CConfigValue<Hyprlang::INT>("cursor:warp_back_after_non_mouse_input");

    if (!m_bLastInputMouse && *PWARPFORNONMOUSE)
        g_pPointerManager->warpTo(m_vLastMousePos);
}
