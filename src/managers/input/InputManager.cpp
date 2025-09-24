#include "InputManager.hpp"
#include "../../Compositor.hpp"
#include <aquamarine/output/Output.hpp>
#include <cstdint>
#include <hyprutils/math/Vector2D.hpp>
#include <ranges>
#include "../../config/ConfigValue.hpp"
#include "../../config/ConfigManager.hpp"
#include "../../desktop/Window.hpp"
#include "../../desktop/LayerSurface.hpp"
#include "../../protocols/CursorShape.hpp"
#include "../../protocols/IdleInhibit.hpp"
#include "../../protocols/RelativePointer.hpp"
#include "../../protocols/PointerConstraints.hpp"
#include "../../protocols/PointerGestures.hpp"
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
#include "../../managers/permissions/DynamicPermissionManager.hpp"

#include "../../helpers/time/Time.hpp"
#include "../../helpers/MiscFunctions.hpp"

#include "trackpad/TrackpadGestures.hpp"

#include <aquamarine/input/Input.hpp>

CInputManager::CInputManager() {
    m_listeners.setCursorShape = PROTO::cursorShape->m_events.setShape.listen([this](const CCursorShapeProtocol::SSetShapeEvent& event) {
        if (!cursorImageUnlocked())
            return;

        if (!g_pSeatManager->m_state.pointerFocusResource)
            return;

        if (wl_resource_get_client(event.pMgr->resource()) != g_pSeatManager->m_state.pointerFocusResource->client())
            return;

        Debug::log(LOG, "cursorImage request: shape {} -> {}", sc<uint32_t>(event.shape), event.shapeName);

        m_cursorSurfaceInfo.wlSurface->unassign();
        m_cursorSurfaceInfo.vHotspot = {};
        m_cursorSurfaceInfo.name     = event.shapeName;
        m_cursorSurfaceInfo.hidden   = false;

        m_cursorSurfaceInfo.inUse = true;
        g_pHyprRenderer->setCursorFromName(m_cursorSurfaceInfo.name);
    });

    m_listeners.newIdleInhibitor   = PROTO::idleInhibit->m_events.newIdleInhibitor.listen([this](const auto& data) { this->newIdleInhibitor(data); });
    m_listeners.newVirtualKeyboard = PROTO::virtualKeyboard->m_events.newKeyboard.listen([this](const auto& keyboard) {
        this->newVirtualKeyboard(keyboard);
        updateCapabilities();
    });
    m_listeners.newVirtualMouse    = PROTO::virtualPointer->m_events.newPointer.listen([this](const auto& mouse) {
        this->newVirtualMouse(mouse);
        updateCapabilities();
    });
    m_listeners.setCursor          = g_pSeatManager->m_events.setCursor.listen([this](const auto& event) { this->processMouseRequest(event); });

    m_cursorSurfaceInfo.wlSurface = CWLSurface::create();
}

CInputManager::~CInputManager() {
    m_constraints.clear();
    m_keyboards.clear();
    m_pointers.clear();
    m_touches.clear();
    m_tablets.clear();
    m_tabletTools.clear();
    m_tabletPads.clear();
    m_idleInhibitors.clear();
    m_switches.clear();
}

void CInputManager::onMouseMoved(IPointer::SMotionEvent e) {
    static auto PNOACCEL = CConfigValue<Hyprlang::INT>("input:force_no_accel");

    Vector2D    delta   = e.delta;
    Vector2D    unaccel = e.unaccel;

    if (e.device) {
        if (e.device->m_isTouchpad) {
            if (e.device->m_flipX) {
                delta.x   = -delta.x;
                unaccel.x = -unaccel.x;
            }
            if (e.device->m_flipY) {
                delta.y   = -delta.y;
                unaccel.y = -unaccel.y;
            }
        }
    }

    const auto DELTA = *PNOACCEL == 1 ? unaccel : delta;

    if (g_pSeatManager->m_isPointerFrameSkipped)
        g_pPointerManager->storeMovement(e.timeMs, DELTA, unaccel);
    else
        g_pPointerManager->setStoredMovement(e.timeMs, DELTA, unaccel);

    PROTO::relativePointer->sendRelativeMotion(sc<uint64_t>(e.timeMs) * 1000, DELTA, unaccel);

    if (e.mouse)
        recheckMouseWarpOnMouseInput();

    g_pPointerManager->move(DELTA);

    mouseMoveUnified(e.timeMs, false, e.mouse);

    m_lastCursorMovement.reset();

    m_lastInputTouch = false;

    if (e.mouse)
        m_lastMousePos = getMouseCoordsInternal();
}

void CInputManager::onMouseWarp(IPointer::SMotionAbsoluteEvent e) {
    g_pPointerManager->warpAbsolute(e.absolute, e.device);

    mouseMoveUnified(e.timeMs);

    m_lastCursorMovement.reset();

    m_lastInputTouch = false;
}

void CInputManager::simulateMouseMovement() {
    m_lastCursorPosFloored = m_lastCursorPosFloored - Vector2D(1, 1); // hack: force the mouseMoveUnified to report without making this a refocus.
    mouseMoveUnified(Time::millis(Time::steadyNow()));
}

void CInputManager::sendMotionEventsToFocused() {
    if (!g_pCompositor->m_lastFocus || isConstrained())
        return;

    // todo: this sucks ass
    const auto PWINDOW = g_pCompositor->getWindowFromSurface(g_pCompositor->m_lastFocus.lock());
    const auto PLS     = g_pCompositor->getLayerSurfaceFromSurface(g_pCompositor->m_lastFocus.lock());

    const auto LOCAL = getMouseCoordsInternal() - (PWINDOW ? PWINDOW->m_realPosition->goal() : (PLS ? Vector2D{PLS->m_geometry.x, PLS->m_geometry.y} : Vector2D{}));

    m_emptyFocusCursorSet = false;

    g_pSeatManager->setPointerFocus(g_pCompositor->m_lastFocus.lock(), LOCAL);
}

void CInputManager::mouseMoveUnified(uint32_t time, bool refocus, bool mouse, std::optional<Vector2D> overridePos) {
    m_lastInputMouse = mouse;

    if (!g_pCompositor->m_readyToProcess || g_pCompositor->m_isShuttingDown || g_pCompositor->m_unsafeState)
        return;

    Vector2D const mouseCoords        = overridePos.value_or(getMouseCoordsInternal());
    auto const     MOUSECOORDSFLOORED = mouseCoords.floor();

    if (MOUSECOORDSFLOORED == m_lastCursorPosFloored && !refocus)
        return;

    static auto PFOLLOWMOUSE          = CConfigValue<Hyprlang::INT>("input:follow_mouse");
    static auto PFOLLOWMOUSETHRESHOLD = CConfigValue<Hyprlang::FLOAT>("input:follow_mouse_threshold");
    static auto PMOUSEREFOCUS         = CConfigValue<Hyprlang::INT>("input:mouse_refocus");
    static auto PFOLLOWONDND          = CConfigValue<Hyprlang::INT>("misc:always_follow_on_dnd");
    static auto PFLOATBEHAVIOR        = CConfigValue<Hyprlang::INT>("input:float_switch_override_focus");
    static auto PMOUSEFOCUSMON        = CConfigValue<Hyprlang::INT>("misc:mouse_move_focuses_monitor");
    static auto PRESIZEONBORDER       = CConfigValue<Hyprlang::INT>("general:resize_on_border");
    static auto PRESIZECURSORICON     = CConfigValue<Hyprlang::INT>("general:hover_icon_on_border");

    const auto  FOLLOWMOUSE = *PFOLLOWONDND && PROTO::data->dndActive() ? 1 : *PFOLLOWMOUSE;

    if (FOLLOWMOUSE == 1 && m_lastCursorMovement.getSeconds() < 0.5)
        m_mousePosDelta += MOUSECOORDSFLOORED.distance(m_lastCursorPosFloored);
    else
        m_mousePosDelta = 0;

    m_foundSurfaceToFocus.reset();
    m_foundLSToFocus.reset();
    m_foundWindowToFocus.reset();
    SP<CWLSurfaceResource> foundSurface;
    Vector2D               surfaceCoords;
    Vector2D               surfacePos = Vector2D(-1337, -1337);
    PHLWINDOW              pFoundWindow;
    PHLLS                  pFoundLayerSurface;

    EMIT_HOOK_EVENT_CANCELLABLE("mouseMove", MOUSECOORDSFLOORED);

    m_lastCursorPosFloored = MOUSECOORDSFLOORED;

    const auto PMONITOR = isLocked() && g_pCompositor->m_lastMonitor ? g_pCompositor->m_lastMonitor.lock() : g_pCompositor->getMonitorFromCursor();

    // this can happen if there are no displays hooked up to Hyprland
    if (PMONITOR == nullptr)
        return;

    if (PMONITOR->m_cursorZoom->value() != 1.f)
        g_pHyprRenderer->damageMonitor(PMONITOR);

    bool skipFrameSchedule = PMONITOR->shouldSkipScheduleFrameOnMouseEvent();

    if (!PMONITOR->m_solitaryClient.lock() && g_pHyprRenderer->shouldRenderCursor() && g_pPointerManager->softwareLockedFor(PMONITOR->m_self.lock()) && !skipFrameSchedule)
        g_pCompositor->scheduleFrameForMonitor(PMONITOR, Aquamarine::IOutput::AQ_SCHEDULE_CURSOR_MOVE);

    // constraints
    if (!g_pSeatManager->m_mouse.expired() && isConstrained()) {
        const auto SURF       = CWLSurface::fromResource(g_pCompositor->m_lastFocus.lock());
        const auto CONSTRAINT = SURF ? SURF->constraint() : nullptr;

        if (CONSTRAINT) {
            if (CONSTRAINT->isLocked()) {
                const auto HINT = CONSTRAINT->logicPositionHint();
                g_pCompositor->warpCursorTo(HINT, true);
            } else {
                const auto RG           = CONSTRAINT->logicConstraintRegion();
                const auto CLOSEST      = RG.closestPoint(mouseCoords);
                const auto BOX          = SURF->getSurfaceBoxGlobal();
                const auto CLOSESTLOCAL = (CLOSEST - (BOX.has_value() ? BOX->pos() : Vector2D{})) * (SURF->getWindow() ? SURF->getWindow()->m_X11SurfaceScaledBy : 1.0);

                g_pCompositor->warpCursorTo(CLOSEST, true);
                g_pSeatManager->sendPointerMotion(time, CLOSESTLOCAL);
                PROTO::relativePointer->sendRelativeMotion(sc<uint64_t>(time) * 1000, {}, {});
            }

            return;

        } else
            Debug::log(ERR, "BUG THIS: Null SURF/CONSTRAINT in mouse refocus. Ignoring constraints. {:x} {:x}", rc<uintptr_t>(SURF.get()), rc<uintptr_t>(CONSTRAINT.get()));
    }

    if (PMONITOR != g_pCompositor->m_lastMonitor && (*PMOUSEFOCUSMON || refocus) && m_forcedFocus.expired())
        g_pCompositor->setActiveMonitor(PMONITOR);

    // check for windows that have focus priority like our permission popups
    pFoundWindow = g_pCompositor->vectorToWindowUnified(mouseCoords, FOCUS_PRIORITY);
    if (pFoundWindow)
        foundSurface = g_pCompositor->vectorWindowToSurface(mouseCoords, pFoundWindow, surfaceCoords);

    if (!foundSurface && g_pSessionLockManager->isSessionLocked()) {

        // set keyboard focus on session lock surface regardless of layers
        const auto PSESSIONLOCKSURFACE = g_pSessionLockManager->getSessionLockSurfaceForMonitor(PMONITOR->m_id);
        const auto foundLockSurface    = PSESSIONLOCKSURFACE ? PSESSIONLOCKSURFACE->surface->surface() : nullptr;

        g_pCompositor->focusSurface(foundLockSurface);

        // search for interactable abovelock surfaces for pointer focus, or use session lock surface if not found
        for (auto& lsl : PMONITOR->m_layerSurfaceLayers | std::views::reverse) {
            foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &lsl, &surfaceCoords, &pFoundLayerSurface, true);

            if (foundSurface)
                break;
        }

        if (!foundSurface) {
            surfaceCoords = mouseCoords - PMONITOR->m_position;
            foundSurface  = foundLockSurface;
        }

        if (refocus) {
            m_foundLSToFocus      = pFoundLayerSurface;
            m_foundWindowToFocus  = pFoundWindow;
            m_foundSurfaceToFocus = foundSurface;
        }

        g_pSeatManager->setPointerFocus(foundSurface, surfaceCoords);
        g_pSeatManager->sendPointerMotion(time, surfaceCoords);

        return;
    }

    PHLWINDOW forcedFocus = m_forcedFocus.lock();

    if (!forcedFocus)
        forcedFocus = g_pCompositor->getForceFocus();

    if (forcedFocus && !foundSurface) {
        pFoundWindow = forcedFocus;
        surfacePos   = pFoundWindow->m_realPosition->value();
        foundSurface = pFoundWindow->m_wlSurface->resource();
    }

    // if we are holding a pointer button,
    // and we're not dnd-ing, don't refocus. Keep focus on last surface.
    if (!PROTO::data->dndActive() && !m_currentlyHeldButtons.empty() && g_pCompositor->m_lastFocus && g_pCompositor->m_lastFocus->m_mapped &&
        g_pSeatManager->m_state.pointerFocus && !m_hardInput) {
        foundSurface = g_pSeatManager->m_state.pointerFocus.lock();

        // IME popups aren't desktop-like elements
        // TODO: make them.
        CInputPopup* foundPopup = m_relay.popupFromSurface(foundSurface);
        if (foundPopup) {
            surfacePos             = foundPopup->globalBox().pos();
            m_focusHeldByButtons   = true;
            m_refocusHeldByButtons = refocus;
        } else {
            auto HLSurface = CWLSurface::fromResource(foundSurface);

            if (HLSurface) {
                const auto BOX = HLSurface->getSurfaceBoxGlobal();

                if (BOX) {
                    const auto PWINDOW = HLSurface->getWindow();
                    surfacePos         = BOX->pos();
                    pFoundLayerSurface = HLSurface->getLayer();
                    if (!pFoundLayerSurface)
                        pFoundWindow = !PWINDOW || PWINDOW->isHidden() ? g_pCompositor->m_lastWindow.lock() : PWINDOW;
                } else // reset foundSurface, find one normally
                    foundSurface = nullptr;
            } else // reset foundSurface, find one normally
                foundSurface = nullptr;
        }
    }

    g_pLayoutManager->getCurrentLayout()->onMouseMove(getMouseCoordsInternal());

    // forced above all
    if (!g_pInputManager->m_exclusiveLSes.empty()) {
        if (!foundSurface)
            foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &g_pInputManager->m_exclusiveLSes, &surfaceCoords, &pFoundLayerSurface);

        if (!foundSurface) {
            foundSurface = (*g_pInputManager->m_exclusiveLSes.begin())->m_surface->resource();
            surfacePos   = (*g_pInputManager->m_exclusiveLSes.begin())->m_realPosition->goal();
        }
    }

    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerPopupSurface(mouseCoords, PMONITOR, &surfaceCoords, &pFoundLayerSurface);

    // overlays are above fullscreen
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], &surfaceCoords, &pFoundLayerSurface);

    // also IME popups
    if (!foundSurface) {
        auto popup = g_pInputManager->m_relay.popupFromCoords(mouseCoords);
        if (popup) {
            foundSurface = popup->getSurface();
            surfacePos   = popup->globalBox().pos();
        }
    }

    // also top layers
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], &surfaceCoords, &pFoundLayerSurface);

    // then, we check if the workspace doesn't have a fullscreen window
    const auto PWORKSPACE   = PMONITOR->m_activeSpecialWorkspace ? PMONITOR->m_activeSpecialWorkspace : PMONITOR->m_activeWorkspace;
    const auto PWINDOWIDEAL = g_pCompositor->vectorToWindowUnified(mouseCoords, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);
    if (PWORKSPACE->m_hasFullscreenWindow && !foundSurface && PWORKSPACE->m_fullscreenMode == FSMODE_FULLSCREEN) {
        pFoundWindow = PWORKSPACE->getFullscreenWindow();

        if (!pFoundWindow) {
            // what the fuck, somehow happens occasionally??
            PWORKSPACE->m_hasFullscreenWindow = false;
            return;
        }

        if (PWINDOWIDEAL &&
            ((PWINDOWIDEAL->m_isFloating && (PWINDOWIDEAL->m_createdOverFullscreen || PWINDOWIDEAL->m_pinned)) /* floating over fullscreen or pinned */
             || (PMONITOR->m_activeSpecialWorkspace == PWINDOWIDEAL->m_workspace) /* on an open special workspace */))
            pFoundWindow = PWINDOWIDEAL;

        if (!pFoundWindow->m_isX11) {
            foundSurface = g_pCompositor->vectorWindowToSurface(mouseCoords, pFoundWindow, surfaceCoords);
            surfacePos   = Vector2D(-1337, -1337);
        } else {
            foundSurface = pFoundWindow->m_wlSurface->resource();
            surfacePos   = pFoundWindow->m_realPosition->value();
        }
    }

    // then windows
    if (!foundSurface) {
        if (PWORKSPACE->m_hasFullscreenWindow && PWORKSPACE->m_fullscreenMode == FSMODE_MAXIMIZED) {
            if (!foundSurface) {
                if (PMONITOR->m_activeSpecialWorkspace) {
                    if (pFoundWindow != PWINDOWIDEAL)
                        pFoundWindow = g_pCompositor->vectorToWindowUnified(mouseCoords, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

                    if (pFoundWindow && !pFoundWindow->onSpecialWorkspace()) {
                        pFoundWindow = PWORKSPACE->getFullscreenWindow();
                    }
                } else {
                    // if we have a maximized window, allow focusing on a bar or something if in reserved area.
                    if (g_pCompositor->isPointOnReservedArea(mouseCoords, PMONITOR)) {
                        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM], &surfaceCoords,
                                                                           &pFoundLayerSurface);
                    }

                    if (!foundSurface) {
                        if (pFoundWindow != PWINDOWIDEAL)
                            pFoundWindow = g_pCompositor->vectorToWindowUnified(mouseCoords, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

                        if (!(pFoundWindow && (pFoundWindow->m_isFloating && (pFoundWindow->m_createdOverFullscreen || pFoundWindow->m_pinned))))
                            pFoundWindow = PWORKSPACE->getFullscreenWindow();
                    }
                }
            }

        } else {
            if (pFoundWindow != PWINDOWIDEAL)
                pFoundWindow = g_pCompositor->vectorToWindowUnified(mouseCoords, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);
        }

        if (pFoundWindow) {
            if (!pFoundWindow->m_isX11) {
                foundSurface = g_pCompositor->vectorWindowToSurface(mouseCoords, pFoundWindow, surfaceCoords);
                if (!foundSurface) {
                    foundSurface = pFoundWindow->m_wlSurface->resource();
                    surfacePos   = pFoundWindow->m_realPosition->value();
                }
            } else {
                foundSurface = pFoundWindow->m_wlSurface->resource();
                surfacePos   = pFoundWindow->m_realPosition->value();
            }
        }
    }

    // then surfaces below
    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM], &surfaceCoords, &pFoundLayerSurface);

    if (!foundSurface)
        foundSurface = g_pCompositor->vectorToLayerSurface(mouseCoords, &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND], &surfaceCoords, &pFoundLayerSurface);

    if (g_pPointerManager->softwareLockedFor(PMONITOR->m_self.lock()) > 0 && !skipFrameSchedule)
        g_pCompositor->scheduleFrameForMonitor(g_pCompositor->m_lastMonitor.lock(), Aquamarine::IOutput::AQ_SCHEDULE_CURSOR_MOVE);

    // FIXME: This will be disabled during DnD operations because we do not exactly follow the spec
    // xdg-popup grabs should be keyboard-only, while they are absolute in our case...
    if (g_pSeatManager->m_seatGrab && !g_pSeatManager->m_seatGrab->accepts(foundSurface) && !PROTO::data->dndActive()) {
        if (m_hardInput || refocus) {
            g_pSeatManager->setGrab(nullptr);
            return; // setGrab will refocus
        } else {
            // we need to grab the last surface.
            foundSurface = g_pSeatManager->m_state.pointerFocus.lock();

            auto HLSurface = CWLSurface::fromResource(foundSurface);

            if (HLSurface) {
                const auto BOX = HLSurface->getSurfaceBoxGlobal();

                if (BOX.has_value())
                    surfacePos = BOX->pos();
            }
        }
    }

    if (!foundSurface) {
        if (!m_emptyFocusCursorSet) {
            if (*PRESIZEONBORDER && *PRESIZECURSORICON && m_borderIconDirection != BORDERICON_NONE) {
                m_borderIconDirection = BORDERICON_NONE;
                unsetCursorImage();
            }

            // TODO: maybe wrap?
            if (m_clickBehavior == CLICKMODE_KILL)
                setCursorImageOverride("crosshair");
            else
                setCursorImageOverride("left_ptr");

            m_emptyFocusCursorSet = true;
        }

        g_pSeatManager->setPointerFocus(nullptr, {});

        if (refocus || g_pCompositor->m_lastWindow.expired()) // if we are forcing a refocus, and we don't find a surface, clear the kb focus too!
            g_pCompositor->focusWindow(nullptr);

        return;
    }

    m_emptyFocusCursorSet = false;

    Vector2D surfaceLocal = surfacePos == Vector2D(-1337, -1337) ? surfaceCoords : mouseCoords - surfacePos;

    if (pFoundWindow && !pFoundWindow->m_isX11 && surfacePos != Vector2D(-1337, -1337)) {
        // calc for oversized windows... fucking bullshit.
        CBox geom = pFoundWindow->m_xdgSurface->m_current.geometry;

        surfaceLocal = mouseCoords - surfacePos + geom.pos();
    }

    if (pFoundWindow && pFoundWindow->m_isX11) // for x11 force scale zero
        surfaceLocal = surfaceLocal * pFoundWindow->m_X11SurfaceScaledBy;

    bool allowKeyboardRefocus = true;

    if (!refocus && g_pCompositor->m_lastFocus) {
        const auto PLS = g_pCompositor->getLayerSurfaceFromSurface(g_pCompositor->m_lastFocus.lock());

        if (PLS && PLS->m_layerSurface->m_current.interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
            allowKeyboardRefocus = false;
    }

    // set the values for use
    if (refocus) {
        m_foundLSToFocus      = pFoundLayerSurface;
        m_foundWindowToFocus  = pFoundWindow;
        m_foundSurfaceToFocus = foundSurface;
    }

    if (m_currentlyDraggedWindow.lock() && pFoundWindow != m_currentlyDraggedWindow) {
        g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);
        return;
    }

    if (pFoundWindow && foundSurface == pFoundWindow->m_wlSurface->resource() && !m_cursorImageOverridden) {
        const auto BOX = pFoundWindow->getWindowMainSurfaceBox();
        if (VECNOTINRECT(mouseCoords, BOX.x, BOX.y, BOX.x + BOX.width, BOX.y + BOX.height))
            setCursorImageOverride("left_ptr");
        else
            restoreCursorIconToApp();
    }

    if (pFoundWindow) {
        // change cursor icon if hovering over border
        if (*PRESIZEONBORDER && *PRESIZECURSORICON) {
            if (!pFoundWindow->isFullscreen() && !pFoundWindow->hasPopupAt(mouseCoords)) {
                setCursorIconOnBorder(pFoundWindow);
            } else if (m_borderIconDirection != BORDERICON_NONE) {
                unsetCursorImage();
            }
        }

        if (FOLLOWMOUSE != 1 && !refocus) {
            if (pFoundWindow != g_pCompositor->m_lastWindow.lock() && g_pCompositor->m_lastWindow.lock() &&
                ((pFoundWindow->m_isFloating && *PFLOATBEHAVIOR == 2) || (g_pCompositor->m_lastWindow->m_isFloating != pFoundWindow->m_isFloating && *PFLOATBEHAVIOR != 0))) {
                // enter if change floating style
                if (FOLLOWMOUSE != 3 && allowKeyboardRefocus)
                    g_pCompositor->focusWindow(pFoundWindow, foundSurface);
                g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);
            } else if (FOLLOWMOUSE == 2 || FOLLOWMOUSE == 3)
                g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);

            if (pFoundWindow == g_pCompositor->m_lastWindow)
                g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);

            if (FOLLOWMOUSE != 0 || pFoundWindow == g_pCompositor->m_lastWindow)
                g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);

            if (g_pSeatManager->m_state.pointerFocus == foundSurface)
                g_pSeatManager->sendPointerMotion(time, surfaceLocal);

            m_lastFocusOnLS = false;
            return; // don't enter any new surfaces
        } else {
            if (allowKeyboardRefocus && ((FOLLOWMOUSE != 3 && (*PMOUSEREFOCUS || m_lastMouseFocus.lock() != pFoundWindow)) || refocus)) {
                if (m_lastMouseFocus.lock() != pFoundWindow || g_pCompositor->m_lastWindow.lock() != pFoundWindow || g_pCompositor->m_lastFocus != foundSurface || refocus) {
                    m_lastMouseFocus = pFoundWindow;

                    // TODO: this looks wrong. When over a popup, it constantly is switching.
                    // Temp fix until that's figured out. Otherwise spams windowrule lookups and other shit.
                    if (m_lastMouseFocus.lock() != pFoundWindow || g_pCompositor->m_lastWindow.lock() != pFoundWindow) {
                        if (m_mousePosDelta > *PFOLLOWMOUSETHRESHOLD || refocus) {
                            const bool hasNoFollowMouse = pFoundWindow && pFoundWindow->m_windowData.noFollowMouse.valueOrDefault();

                            if (refocus || !hasNoFollowMouse)
                                g_pCompositor->focusWindow(pFoundWindow, foundSurface);
                        }
                    } else
                        g_pCompositor->focusSurface(foundSurface, pFoundWindow);
                }
            }
        }

        if (g_pSeatManager->m_state.keyboardFocus == nullptr)
            g_pCompositor->focusWindow(pFoundWindow, foundSurface);

        m_lastFocusOnLS = false;
    } else {
        if (*PRESIZEONBORDER && *PRESIZECURSORICON && m_borderIconDirection != BORDERICON_NONE) {
            m_borderIconDirection = BORDERICON_NONE;
            unsetCursorImage();
        }

        if (pFoundLayerSurface && (pFoundLayerSurface->m_layerSurface->m_current.interactivity != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE) && FOLLOWMOUSE != 3 &&
            (allowKeyboardRefocus || pFoundLayerSurface->m_layerSurface->m_current.interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)) {
            g_pCompositor->focusSurface(foundSurface);
        }

        if (pFoundLayerSurface)
            m_lastFocusOnLS = true;
    }

    g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);
    g_pSeatManager->sendPointerMotion(time, surfaceLocal);
}

void CInputManager::onMouseButton(IPointer::SButtonEvent e) {
    EMIT_HOOK_EVENT_CANCELLABLE("mouseButton", e);

    if (e.mouse)
        recheckMouseWarpOnMouseInput();

    m_lastCursorMovement.reset();

    if (e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
        m_currentlyHeldButtons.push_back(e.button);
    } else {
        if (std::ranges::find_if(m_currentlyHeldButtons, [&](const auto& other) { return other == e.button; }) == m_currentlyHeldButtons.end())
            return;
        std::erase_if(m_currentlyHeldButtons, [&](const auto& other) { return other == e.button; });
    }

    switch (m_clickBehavior) {
        case CLICKMODE_DEFAULT: processMouseDownNormal(e); break;
        case CLICKMODE_KILL: processMouseDownKill(e); break;
        default: break;
    }

    if (m_focusHeldByButtons && m_currentlyHeldButtons.empty() && e.state == WL_POINTER_BUTTON_STATE_RELEASED) {
        if (m_refocusHeldByButtons)
            refocus();
        else
            simulateMouseMovement();

        m_focusHeldByButtons   = false;
        m_refocusHeldByButtons = false;
    }
}

void CInputManager::processMouseRequest(const CSeatManager::SSetCursorEvent& event) {
    if (!cursorImageUnlocked())
        return;

    Debug::log(LOG, "cursorImage request: surface {:x}", rc<uintptr_t>(event.surf.get()));

    if (event.surf != m_cursorSurfaceInfo.wlSurface->resource()) {
        m_cursorSurfaceInfo.wlSurface->unassign();

        if (event.surf)
            m_cursorSurfaceInfo.wlSurface->assign(event.surf);
    }

    if (event.surf) {
        m_cursorSurfaceInfo.vHotspot = event.hotspot;
        m_cursorSurfaceInfo.hidden   = false;
    } else {
        m_cursorSurfaceInfo.vHotspot = {};
        m_cursorSurfaceInfo.hidden   = true;
    }

    m_cursorSurfaceInfo.name = "";

    m_cursorSurfaceInfo.inUse = true;
    g_pHyprRenderer->setCursorSurface(m_cursorSurfaceInfo.wlSurface, event.hotspot.x, event.hotspot.y);
}

void CInputManager::restoreCursorIconToApp() {
    if (m_cursorSurfaceInfo.inUse)
        return;

    if (m_cursorSurfaceInfo.hidden) {
        g_pHyprRenderer->setCursorSurface(nullptr, 0, 0);
        return;
    }

    if (m_cursorSurfaceInfo.name.empty()) {
        if (m_cursorSurfaceInfo.wlSurface->exists())
            g_pHyprRenderer->setCursorSurface(m_cursorSurfaceInfo.wlSurface, m_cursorSurfaceInfo.vHotspot.x, m_cursorSurfaceInfo.vHotspot.y);
    } else {
        g_pHyprRenderer->setCursorFromName(m_cursorSurfaceInfo.name);
    }

    m_cursorSurfaceInfo.inUse = true;
}

void CInputManager::setCursorImageOverride(const std::string& name) {
    if (m_cursorImageOverridden)
        return;

    m_cursorSurfaceInfo.inUse = false;
    g_pHyprRenderer->setCursorFromName(name);
}

bool CInputManager::cursorImageUnlocked() {
    if (m_clickBehavior == CLICKMODE_KILL)
        return false;

    if (m_cursorImageOverridden)
        return false;

    return true;
}

eClickBehaviorMode CInputManager::getClickMode() {
    return m_clickBehavior;
}

void CInputManager::setClickMode(eClickBehaviorMode mode) {
    switch (mode) {
        case CLICKMODE_DEFAULT:
            Debug::log(LOG, "SetClickMode: DEFAULT");
            m_clickBehavior = CLICKMODE_DEFAULT;
            g_pHyprRenderer->setCursorFromName("left_ptr");
            break;

        case CLICKMODE_KILL:
            Debug::log(LOG, "SetClickMode: KILL");
            m_clickBehavior = CLICKMODE_KILL;

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

    if (w && !m_lastFocusOnLS && !g_pSessionLockManager->isSessionLocked() && w->checkInputOnDecos(INPUT_TYPE_BUTTON, mouseCoords, e))
        return;

    // clicking on border triggers resize
    // TODO detect click on LS properly
    if (*PRESIZEONBORDER && !g_pSessionLockManager->isSessionLocked() && !m_lastFocusOnLS && e.state == WL_POINTER_BUTTON_STATE_PRESSED && (!w || !w->isX11OverrideRedirect())) {
        if (w && !w->isFullscreen()) {
            const CBox real = {w->m_realPosition->value().x, w->m_realPosition->value().y, w->m_realSize->value().x, w->m_realSize->value().y};
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

            if ((g_pSeatManager->m_mouse.expired() || !isConstrained()) /* No constraints */
                && (w && g_pCompositor->m_lastWindow.lock() != w) /* window should change */) {
                // a bit hacky
                // if we only pressed one button, allow us to refocus. m_lCurrentlyHeldButtons.size() > 0 will stick the focus
                if (m_currentlyHeldButtons.size() == 1) {
                    const auto COPY = m_currentlyHeldButtons;
                    m_currentlyHeldButtons.clear();
                    refocus();
                    m_currentlyHeldButtons = COPY;
                } else
                    refocus();
            }

            // if clicked on a floating window make it top
            if (!g_pSeatManager->m_state.pointerFocus)
                break;

            auto HLSurf = CWLSurface::fromResource(g_pSeatManager->m_state.pointerFocus.lock());

            if (HLSurf && HLSurf->getWindow())
                g_pCompositor->changeWindowZOrder(HLSurf->getWindow(), true);

            break;
        }
        case WL_POINTER_BUTTON_STATE_RELEASED: break;
    }

    // notify app if we didn't handle it
    g_pSeatManager->sendPointerButton(e.timeMs, e.button, e.state);

    if (const auto PMON = g_pCompositor->getMonitorFromVector(mouseCoords); PMON != g_pCompositor->m_lastMonitor && PMON)
        g_pCompositor->setActiveMonitor(PMON);

    if (g_pSeatManager->m_seatGrab && e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
        m_hardInput = true;
        simulateMouseMovement();
        m_hardInput = false;
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
    m_clickBehavior = CLICKMODE_DEFAULT;
}

void CInputManager::onMouseWheel(IPointer::SAxisEvent e, SP<IPointer> pointer) {
    static auto POFFWINDOWAXIS        = CConfigValue<Hyprlang::INT>("input:off_window_axis_events");
    static auto PINPUTSCROLLFACTOR    = CConfigValue<Hyprlang::FLOAT>("input:scroll_factor");
    static auto PTOUCHPADSCROLLFACTOR = CConfigValue<Hyprlang::FLOAT>("input:touchpad:scroll_factor");
    static auto PEMULATEDISCRETE      = CConfigValue<Hyprlang::INT>("input:emulate_discrete_scroll");
    static auto PFOLLOWMOUSE          = CConfigValue<Hyprlang::INT>("input:follow_mouse");

    const bool  ISTOUCHPADSCROLL = *PTOUCHPADSCROLLFACTOR <= 0.f || e.source == WL_POINTER_AXIS_SOURCE_FINGER;
    auto        factor           = ISTOUCHPADSCROLL ? *PTOUCHPADSCROLLFACTOR : *PINPUTSCROLLFACTOR;

    if (pointer && pointer->m_scrollFactor.has_value())
        factor = *pointer->m_scrollFactor;

    const auto EMAP = std::unordered_map<std::string, std::any>{{"event", e}};
    EMIT_HOOK_EVENT_CANCELLABLE("mouseAxis", EMAP);

    if (e.mouse)
        recheckMouseWarpOnMouseInput();

    bool passEvent = g_pKeybindManager->onAxisEvent(e);

    if (!passEvent)
        return;

    if (!m_lastFocusOnLS) {
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

            if (g_pSeatManager->m_state.pointerFocus) {
                const auto PCURRWINDOW = g_pCompositor->getWindowFromSurface(g_pSeatManager->m_state.pointerFocus.lock());

                if (*PFOLLOWMOUSE == 1 && PCURRWINDOW && PWINDOW != PCURRWINDOW)
                    simulateMouseMovement();
            }

            if (!ISTOUCHPADSCROLL && PWINDOW->isScrollMouseOverridden())
                factor = PWINDOW->getScrollMouse();
            else if (ISTOUCHPADSCROLL && PWINDOW->isScrollTouchpadOverridden())
                factor = PWINDOW->getScrollTouchpad();
        }
    }

    double discrete = (e.deltaDiscrete != 0) ? (factor * e.deltaDiscrete / std::abs(e.deltaDiscrete)) : 0;
    double delta    = e.delta * factor;

    if (e.source == 0) {
        // if an application supports v120, it should ignore discrete anyways
        if ((*PEMULATEDISCRETE >= 1 && std::abs(e.deltaDiscrete) != 120) || *PEMULATEDISCRETE >= 2) {

            const int interval = factor != 0 ? std::round(120 * (1 / factor)) : 120;

            // reset the accumulator when timeout is reached or direction/axis has changed
            if (std::signbit(e.deltaDiscrete) != m_scrollWheelState.lastEventSign || e.axis != m_scrollWheelState.lastEventAxis ||
                e.timeMs - m_scrollWheelState.lastEventTime > 500 /* 500ms taken from libinput default timeout */) {

                m_scrollWheelState.accumulatedScroll = 0;
                // send 1 discrete on first event for responsiveness
                discrete = std::copysign(1, e.deltaDiscrete);
            } else
                discrete = 0;

            for (int ac = m_scrollWheelState.accumulatedScroll; ac >= interval; ac -= interval) {
                discrete += std::copysign(1, e.deltaDiscrete);
                m_scrollWheelState.accumulatedScroll -= interval;
            }

            m_scrollWheelState.lastEventSign = std::signbit(e.deltaDiscrete);
            m_scrollWheelState.lastEventAxis = e.axis;
            m_scrollWheelState.lastEventTime = e.timeMs;
            m_scrollWheelState.accumulatedScroll += std::abs(e.deltaDiscrete);

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

void CInputManager::newKeyboard(SP<IKeyboard> keeb) {
    const auto PNEWKEYBOARD = m_keyboards.emplace_back(keeb);

    setupKeyboard(PNEWKEYBOARD);

    Debug::log(LOG, "New keyboard created, pointers Hypr: {:x}", rc<uintptr_t>(PNEWKEYBOARD.get()));
}

void CInputManager::newKeyboard(SP<Aquamarine::IKeyboard> keyboard) {
    const auto PNEWKEYBOARD = m_keyboards.emplace_back(CKeyboard::create(keyboard));

    setupKeyboard(PNEWKEYBOARD);

    Debug::log(LOG, "New keyboard created, pointers Hypr: {:x} and AQ: {:x}", rc<uintptr_t>(PNEWKEYBOARD.get()), rc<uintptr_t>(keyboard.get()));
}

void CInputManager::newVirtualKeyboard(SP<CVirtualKeyboardV1Resource> keyboard) {
    const auto PNEWKEYBOARD = m_keyboards.emplace_back(CVirtualKeyboard::create(keyboard));

    setupKeyboard(PNEWKEYBOARD);

    Debug::log(LOG, "New virtual keyboard created at {:x}", rc<uintptr_t>(PNEWKEYBOARD.get()));
}

void CInputManager::setupKeyboard(SP<IKeyboard> keeb) {
    static auto PDPMS = CConfigValue<Hyprlang::INT>("misc:key_press_enables_dpms");

    m_hids.emplace_back(keeb);

    try {
        keeb->m_hlName = getNameForNewDevice(keeb->m_deviceName);
    } catch (std::exception& e) {
        Debug::log(ERR, "Keyboard had no name???"); // logic error
    }

    keeb->m_events.destroy.listenStatic([this, keeb = keeb.get()] {
        auto PKEEB = keeb->m_self.lock();

        if (!PKEEB)
            return;

        destroyKeyboard(PKEEB);
        Debug::log(LOG, "Destroyed keyboard {:x}", rc<uintptr_t>(keeb));
    });

    keeb->m_keyboardEvents.key.listenStatic([this, keeb = keeb.get()](const IKeyboard::SKeyEvent& event) {
        auto PKEEB = keeb->m_self.lock();

        onKeyboardKey(event, PKEEB);

        if (PKEEB->m_enabled)
            PROTO::idle->onActivity();

        if (PKEEB->m_enabled && *PDPMS && !g_pCompositor->m_dpmsStateOn)
            g_pKeybindManager->dpms("on");
    });

    keeb->m_keyboardEvents.modifiers.listenStatic([this, keeb = keeb.get()] {
        auto PKEEB = keeb->m_self.lock();

        onKeyboardMod(PKEEB);

        if (PKEEB->m_enabled)
            PROTO::idle->onActivity();

        if (PKEEB->m_enabled && *PDPMS && !g_pCompositor->m_dpmsStateOn)
            g_pKeybindManager->dpms("on");
    });

    keeb->m_keyboardEvents.keymap.listenStatic([keeb = keeb.get()] {
        auto       PKEEB  = keeb->m_self.lock();
        const auto LAYOUT = PKEEB->getActiveLayout();

        if (PKEEB == g_pSeatManager->m_keyboard) {
            g_pSeatManager->updateActiveKeyboardData();
            g_pKeybindManager->m_keyToCodeCache.clear();
        }

        g_pEventManager->postEvent(SHyprIPCEvent{"activelayout", PKEEB->m_hlName + "," + LAYOUT});
        EMIT_HOOK_EVENT("activeLayout", (std::vector<std::any>{PKEEB, LAYOUT}));
    });

    disableAllKeyboards(false);

    applyConfigToKeyboard(keeb);

    g_pSeatManager->setKeyboard(keeb);

    keeb->updateLEDs();

    // in case m_lastFocus was set without a keyboard
    if (m_keyboards.size() == 1 && g_pCompositor->m_lastFocus)
        g_pSeatManager->setKeyboardFocus(g_pCompositor->m_lastFocus.lock());
}

void CInputManager::setKeyboardLayout() {
    for (auto const& k : m_keyboards)
        applyConfigToKeyboard(k);

    g_pKeybindManager->updateXKBTranslationState();
}

void CInputManager::applyConfigToKeyboard(SP<IKeyboard> pKeyboard) {
    auto       devname = pKeyboard->m_hlName;

    const auto HASCONFIG = g_pConfigManager->deviceConfigExists(devname);

    Debug::log(LOG, "ApplyConfigToKeyboard for \"{}\", hasconfig: {}", devname, sc<int>(HASCONFIG));

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

    const auto ENABLED    = HASCONFIG ? g_pConfigManager->getDeviceInt(devname, "enabled") : true;
    const auto ALLOWBINDS = HASCONFIG ? g_pConfigManager->getDeviceInt(devname, "keybinds") : true;

    pKeyboard->m_enabled           = ENABLED;
    pKeyboard->m_resolveBindsBySym = RESOLVEBINDSBYSYM;
    pKeyboard->m_allowBinds        = ALLOWBINDS;

    const auto PERM = g_pDynamicPermissionManager->clientPermissionModeWithString(-1, pKeyboard->m_hlName, PERMISSION_TYPE_KEYBOARD);
    if (PERM == PERMISSION_RULE_ALLOW_MODE_PENDING) {
        const auto PROMISE = g_pDynamicPermissionManager->promiseFor(-1, pKeyboard->m_hlName, PERMISSION_TYPE_KEYBOARD);
        if (!PROMISE)
            Debug::log(ERR, "BUG THIS: No promise for client permission for keyboard");
        else {
            PROMISE->then([k = WP<IKeyboard>{pKeyboard}](SP<CPromiseResult<eDynamicPermissionAllowMode>> r) {
                if (r->hasError()) {
                    Debug::log(ERR, "BUG THIS: No permission returned for keyboard");
                    return;
                }

                if (!k)
                    return;

                k->m_allowed = r->result() == PERMISSION_RULE_ALLOW_MODE_ALLOW;
            });
        }
    } else
        pKeyboard->m_allowed = PERM == PERMISSION_RULE_ALLOW_MODE_ALLOW;

    try {
        if (NUMLOCKON == pKeyboard->m_numlockOn && REPEATDELAY == pKeyboard->m_repeatDelay && REPEATRATE == pKeyboard->m_repeatRate && RULES == pKeyboard->m_currentRules.rules &&
            MODEL == pKeyboard->m_currentRules.model && LAYOUT == pKeyboard->m_currentRules.layout && VARIANT == pKeyboard->m_currentRules.variant &&
            OPTIONS == pKeyboard->m_currentRules.options && FILEPATH == pKeyboard->m_xkbFilePath) {
            Debug::log(LOG, "Not applying config to keyboard, it did not change.");
            return;
        }
    } catch (std::exception& e) {
        // can be libc errors for null std::string
        // we can ignore those and just apply
    }

    pKeyboard->m_repeatRate  = std::max(0, REPEATRATE);
    pKeyboard->m_repeatDelay = std::max(0, REPEATDELAY);
    pKeyboard->m_numlockOn   = NUMLOCKON;
    pKeyboard->m_xkbFilePath = FILEPATH;
    pKeyboard->setKeymap(IKeyboard::SStringRuleNames{LAYOUT, MODEL, VARIANT, OPTIONS, RULES});

    const auto LAYOUTSTR = pKeyboard->getActiveLayout();

    g_pEventManager->postEvent(SHyprIPCEvent{"activelayout", pKeyboard->m_hlName + "," + LAYOUTSTR});
    EMIT_HOOK_EVENT("activeLayout", (std::vector<std::any>{pKeyboard, LAYOUTSTR}));

    Debug::log(LOG, "Set the keyboard layout to {} and variant to {} for keyboard \"{}\"", pKeyboard->m_currentRules.layout, pKeyboard->m_currentRules.variant,
               pKeyboard->m_hlName);
}

void CInputManager::newVirtualMouse(SP<CVirtualPointerV1Resource> mouse) {
    const auto PMOUSE = m_pointers.emplace_back(CVirtualPointer::create(mouse));

    setupMouse(PMOUSE);

    Debug::log(LOG, "New virtual mouse created");
}

void CInputManager::newMouse(SP<IPointer> mouse) {
    m_pointers.emplace_back(mouse);

    setupMouse(mouse);

    Debug::log(LOG, "New mouse created, pointer Hypr: {:x}", rc<uintptr_t>(mouse.get()));
}

void CInputManager::newMouse(SP<Aquamarine::IPointer> mouse) {
    const auto PMOUSE = m_pointers.emplace_back(CMouse::create(mouse));

    setupMouse(PMOUSE);

    Debug::log(LOG, "New mouse created, pointer AQ: {:x}", rc<uintptr_t>(mouse.get()));
}

void CInputManager::setupMouse(SP<IPointer> mauz) {
    m_hids.emplace_back(mauz);

    try {
        mauz->m_hlName = getNameForNewDevice(mauz->m_deviceName);
    } catch (std::exception& e) {
        Debug::log(ERR, "Mouse had no name???"); // logic error
    }

    if (mauz->aq() && mauz->aq()->getLibinputHandle()) {
        const auto LIBINPUTDEV = mauz->aq()->getLibinputHandle();

        Debug::log(LOG, "New mouse has libinput sens {:.2f} ({:.2f}) with accel profile {} ({})", libinput_device_config_accel_get_speed(LIBINPUTDEV),
                   libinput_device_config_accel_get_default_speed(LIBINPUTDEV), sc<int>(libinput_device_config_accel_get_profile(LIBINPUTDEV)),
                   sc<int>(libinput_device_config_accel_get_default_profile(LIBINPUTDEV)));
    }

    g_pPointerManager->attachPointer(mauz);

    mauz->m_connected = true;

    setPointerConfigs();

    mauz->m_events.destroy.listenStatic([this, PMOUSE = mauz.get()] { destroyPointer(PMOUSE->m_self.lock()); });

    g_pSeatManager->setMouse(mauz);

    m_lastCursorMovement.reset();
}

void CInputManager::setPointerConfigs() {
    for (auto const& m : m_pointers) {
        auto       devname = m->m_hlName;

        const auto HASCONFIG = g_pConfigManager->deviceConfigExists(devname);

        if (HASCONFIG) {
            const auto ENABLED = g_pConfigManager->getDeviceInt(devname, "enabled");
            if (ENABLED && !m->m_connected) {
                g_pPointerManager->attachPointer(m);
                m->m_connected = true;
            } else if (!ENABLED && m->m_connected) {
                g_pPointerManager->detachPointer(m);
                m->m_connected = false;
            }
        }

        if (g_pConfigManager->deviceConfigExplicitlySet(devname, "scroll_factor"))
            m->m_scrollFactor = std::clamp(g_pConfigManager->getDeviceFloat(devname, "scroll_factor", "input:scroll_factor"), 0.F, 100.F);
        else
            m->m_scrollFactor = std::nullopt;

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
                if (TAP_MAP.empty() || TAP_MAP == "lrm")
                    libinput_device_config_tap_set_button_map(LIBINPUTDEV, LIBINPUT_CONFIG_TAP_MAP_LRM);
                else if (TAP_MAP == "lmr")
                    libinput_device_config_tap_set_button_map(LIBINPUTDEV, LIBINPUT_CONFIG_TAP_MAP_LMR);
                else
                    Debug::log(WARN, "Tap button mapping unknown");
            }

            const auto SCROLLMETHOD = g_pConfigManager->getDeviceString(devname, "scroll_method", "input:scroll_method");
            if (SCROLLMETHOD.empty()) {
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

            const auto TAP_DRAG_LOCK = g_pConfigManager->getDeviceInt(devname, "drag_lock", "input:touchpad:drag_lock");
            if (TAP_DRAG_LOCK >= 0 && TAP_DRAG_LOCK <= 2) {
                libinput_device_config_tap_set_drag_lock_enabled(LIBINPUTDEV, sc<libinput_config_drag_lock_state>(TAP_DRAG_LOCK));
            }

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

            if (libinput_device_config_3fg_drag_get_finger_count(LIBINPUTDEV) >= 3) {
                const auto DRAG_3FG_STATE = sc<libinput_config_3fg_drag_state>(g_pConfigManager->getDeviceInt(devname, "drag_3fg", "input:touchpad:drag_3fg"));
                libinput_device_config_3fg_drag_set_enabled(LIBINPUTDEV, DRAG_3FG_STATE);
            }

            if (libinput_device_config_dwt_is_available(LIBINPUTDEV)) {
                const auto DWT = sc<enum libinput_config_dwt_state>(g_pConfigManager->getDeviceInt(devname, "disable_while_typing", "input:touchpad:disable_while_typing") != 0);
                libinput_device_config_dwt_set_enabled(LIBINPUTDEV, DWT);
            }

            const auto LIBINPUTSENS = std::clamp(g_pConfigManager->getDeviceFloat(devname, "sensitivity", "input:sensitivity"), -1.f, 1.f);
            libinput_device_config_accel_set_speed(LIBINPUTDEV, LIBINPUTSENS);

            m->m_flipX = g_pConfigManager->getDeviceInt(devname, "flip_x", "input:touchpad:flip_x") != 0;
            m->m_flipY = g_pConfigManager->getDeviceInt(devname, "flip_y", "input:touchpad:flip_y") != 0;

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

            Debug::log(LOG, "Applied config to mouse {}, sens {:.2f}", m->m_hlName, LIBINPUTSENS);
        }
    }
}

static void removeFromHIDs(WP<IHID> hid) {
    std::erase_if(g_pInputManager->m_hids, [hid](const auto& e) { return e.expired() || e == hid; });
    g_pInputManager->updateCapabilities();
}

void CInputManager::destroyKeyboard(SP<IKeyboard> pKeyboard) {
    Debug::log(LOG, "Keyboard at {:x} removed", rc<uintptr_t>(pKeyboard.get()));

    std::erase_if(m_keyboards, [pKeyboard](const auto& other) { return other == pKeyboard; });

    if (!m_keyboards.empty()) {
        bool found = false;
        for (auto const& k : m_keyboards | std::views::reverse) {
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
    Debug::log(LOG, "Pointer at {:x} removed", rc<uintptr_t>(mouse.get()));

    std::erase_if(m_pointers, [mouse](const auto& other) { return other == mouse; });

    g_pSeatManager->setMouse(!m_pointers.empty() ? m_pointers.front() : nullptr);

    if (!g_pSeatManager->m_mouse.expired())
        unconstrainMouse();

    removeFromHIDs(mouse);
}

void CInputManager::destroyTouchDevice(SP<ITouch> touch) {
    Debug::log(LOG, "Touch device at {:x} removed", rc<uintptr_t>(touch.get()));

    std::erase_if(m_touches, [touch](const auto& other) { return other == touch; });

    removeFromHIDs(touch);
}

void CInputManager::destroyTablet(SP<CTablet> tablet) {
    Debug::log(LOG, "Tablet device at {:x} removed", rc<uintptr_t>(tablet.get()));

    std::erase_if(m_tablets, [tablet](const auto& other) { return other == tablet; });

    removeFromHIDs(tablet);
}

void CInputManager::destroyTabletTool(SP<CTabletTool> tool) {
    Debug::log(LOG, "Tablet tool at {:x} removed", rc<uintptr_t>(tool.get()));

    std::erase_if(m_tabletTools, [tool](const auto& other) { return other == tool; });

    removeFromHIDs(tool);
}

void CInputManager::destroyTabletPad(SP<CTabletPad> pad) {
    Debug::log(LOG, "Tablet pad at {:x} removed", rc<uintptr_t>(pad.get()));

    std::erase_if(m_tabletPads, [pad](const auto& other) { return other == pad; });

    removeFromHIDs(pad);
}

void CInputManager::updateKeyboardsLeds(SP<IKeyboard> pKeyboard) {
    if (!pKeyboard || pKeyboard->isVirtual())
        return;

    std::optional<uint32_t> leds = pKeyboard->getLEDs();

    if (!leds.has_value())
        return;

    for (auto const& k : m_keyboards) {
        k->updateLEDs(leds.value());
    }
}

void CInputManager::onKeyboardKey(const IKeyboard::SKeyEvent& event, SP<IKeyboard> pKeyboard) {
    if (!pKeyboard->m_enabled || !pKeyboard->m_allowed)
        return;

    const bool DISALLOWACTION = pKeyboard->isVirtual() && shouldIgnoreVirtualKeyboard(pKeyboard);

    const auto IME    = m_relay.m_inputMethod.lock();
    const bool HASIME = IME && IME->hasGrab();
    const bool USEIME = HASIME && !DISALLOWACTION;

    const auto EMAP = std::unordered_map<std::string, std::any>{{"keyboard", pKeyboard}, {"event", event}};
    EMIT_HOOK_EVENT_CANCELLABLE("keyPress", EMAP);

    bool passEvent = DISALLOWACTION;

    if (!DISALLOWACTION)
        passEvent = g_pKeybindManager->onKeyEvent(event, pKeyboard);

    if (passEvent) {
        if (USEIME) {
            IME->setKeyboard(pKeyboard);
            IME->sendKey(event.timeMs, event.keycode, event.state);
        } else {
            const auto PRESSED  = shareKeyFromAllKBs(event.keycode, event.state == WL_KEYBOARD_KEY_STATE_PRESSED);
            const auto CONTAINS = std::ranges::contains(m_pressed, event.keycode);

            if (CONTAINS && PRESSED)
                return;
            if (!CONTAINS && !PRESSED)
                return;

            if (CONTAINS)
                std::erase(m_pressed, event.keycode);
            else
                m_pressed.emplace_back(event.keycode);

            g_pSeatManager->setKeyboard(pKeyboard);
            g_pSeatManager->sendKeyboardKey(event.timeMs, event.keycode, event.state);
        }

        updateKeyboardsLeds(pKeyboard);
    }
}

void CInputManager::onKeyboardMod(SP<IKeyboard> pKeyboard) {
    if (!pKeyboard->m_enabled)
        return;

    const bool DISALLOWACTION = pKeyboard->isVirtual() && shouldIgnoreVirtualKeyboard(pKeyboard);

    auto       MODS    = pKeyboard->m_modifiersState;
    const auto ALLMODS = shareModsFromAllKBs(MODS.depressed);
    MODS.depressed     = ALLMODS;
    m_lastMods         = MODS.depressed;

    const auto IME = m_relay.m_inputMethod.lock();

    if (IME && IME->hasGrab() && !DISALLOWACTION) {
        IME->setKeyboard(pKeyboard);
        IME->sendMods(MODS.depressed, MODS.latched, MODS.locked, MODS.group);
    } else {
        g_pSeatManager->setKeyboard(pKeyboard);
        g_pSeatManager->sendKeyboardMods(MODS.depressed, MODS.latched, MODS.locked, MODS.group);
    }

    updateKeyboardsLeds(pKeyboard);

    if (pKeyboard->m_modifiersState.group != pKeyboard->m_activeLayout) {
        pKeyboard->m_activeLayout = pKeyboard->m_modifiersState.group;

        const auto LAYOUT = pKeyboard->getActiveLayout();

        Debug::log(LOG, "LAYOUT CHANGED TO {} GROUP {}", LAYOUT, MODS.group);

        g_pEventManager->postEvent(SHyprIPCEvent{"activelayout", pKeyboard->m_hlName + "," + LAYOUT});
        EMIT_HOOK_EVENT("activeLayout", (std::vector<std::any>{pKeyboard, LAYOUT}));
    }
}

bool CInputManager::shouldIgnoreVirtualKeyboard(SP<IKeyboard> pKeyboard) {
    if (!pKeyboard)
        return true;

    if (!pKeyboard->isVirtual())
        return false;

    const auto CLIENT = pKeyboard->getClient();

    const auto DISALLOWACTION = CLIENT && !m_relay.m_inputMethod.expired() && m_relay.m_inputMethod->grabClient() == CLIENT;

    if (DISALLOWACTION)
        pKeyboard->setShareStatesAuto(false);

    return DISALLOWACTION;
}

void CInputManager::refocus(std::optional<Vector2D> overridePos) {
    mouseMoveUnified(0, true, false, overridePos);
}

bool CInputManager::refocusLastWindow(PHLMONITOR pMonitor) {
    if (!m_exclusiveLSes.empty()) {
        Debug::log(LOG, "CInputManager::refocusLastWindow: ignoring, exclusive LS present.");
        return false;
    }

    if (!pMonitor) {
        refocus();
        return true;
    }

    Vector2D               surfaceCoords;
    PHLLS                  pFoundLayerSurface;
    SP<CWLSurfaceResource> foundSurface = nullptr;

    g_pInputManager->releaseAllMouseButtons();

    // then any surfaces above windows on the same monitor
    if (!foundSurface) {
        foundSurface = g_pCompositor->vectorToLayerSurface(g_pInputManager->getMouseCoordsInternal(), &pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY],
                                                           &surfaceCoords, &pFoundLayerSurface);
        if (pFoundLayerSurface && pFoundLayerSurface->m_interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND)
            foundSurface = nullptr;
    }

    if (!foundSurface) {
        foundSurface = g_pCompositor->vectorToLayerSurface(g_pInputManager->getMouseCoordsInternal(), &pMonitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP],
                                                           &surfaceCoords, &pFoundLayerSurface);
        if (pFoundLayerSurface && pFoundLayerSurface->m_interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND)
            foundSurface = nullptr;
    }

    if (!foundSurface && g_pCompositor->m_lastWindow.lock() && g_pCompositor->m_lastWindow->m_workspace && g_pCompositor->m_lastWindow->m_workspace->isVisibleNotCovered()) {
        // then the last focused window if we're on the same workspace as it
        const auto PLASTWINDOW = g_pCompositor->m_lastWindow.lock();
        g_pCompositor->focusWindow(PLASTWINDOW);
    } else {
        // otherwise fall back to a normal refocus.

        if (foundSurface && !foundSurface->m_hlSurface->keyboardFocusable()) {
            const auto PLASTWINDOW = g_pCompositor->m_lastWindow.lock();
            g_pCompositor->focusWindow(PLASTWINDOW);
        }

        refocus();
    }

    return true;
}

void CInputManager::unconstrainMouse() {
    if (g_pSeatManager->m_mouse.expired())
        return;

    for (auto const& c : m_constraints) {
        const auto C = c.lock();

        if (!C)
            continue;

        if (!C->isActive())
            continue;

        C->deactivate();
    }
}

bool CInputManager::isConstrained() {
    return std::ranges::any_of(m_constraints, [](auto const& c) {
        const auto constraint = c.lock();
        return constraint && constraint->isActive() && constraint->owner()->resource() == g_pCompositor->m_lastFocus;
    });
}

bool CInputManager::isLocked() {
    if (!isConstrained())
        return false;

    const auto SURF       = CWLSurface::fromResource(g_pCompositor->m_lastFocus.lock());
    const auto CONSTRAINT = SURF ? SURF->constraint() : nullptr;

    return CONSTRAINT && CONSTRAINT->isLocked();
}

void CInputManager::updateCapabilities() {
    uint32_t caps = 0;

    for (auto const& h : m_hids) {
        if (h.expired())
            continue;

        caps |= h->getCapabilities();
    }

    g_pSeatManager->updateCapabilities(caps);
    m_capabilities = caps;
}

const std::vector<uint32_t>& CInputManager::getKeysFromAllKBs() {
    return m_pressed;
}

uint32_t CInputManager::getModsFromAllKBs() {
    return m_lastMods;
}

bool CInputManager::shareKeyFromAllKBs(uint32_t key, bool pressed) {
    bool finalState = pressed;

    if (finalState)
        return finalState;

    for (auto const& kb : m_keyboards) {
        if (!kb->shareStates())
            continue;

        if (kb->isVirtual() && shouldIgnoreVirtualKeyboard(kb))
            continue;

        if (!kb->m_enabled)
            continue;

        const bool PRESSED = kb->getPressed(key);
        if (PRESSED)
            return PRESSED;
    }

    return finalState;
}

uint32_t CInputManager::shareModsFromAllKBs(uint32_t depressed) {
    uint32_t finalMask = depressed;

    for (auto const& kb : m_keyboards) {
        if (!kb->shareStates())
            continue;

        if (kb->isVirtual() && shouldIgnoreVirtualKeyboard(kb))
            continue;

        if (!kb->m_enabled)
            continue;

        finalMask |= kb->getModifiers();
    }

    return finalMask;
}

void CInputManager::disableAllKeyboards(bool virt) {

    for (auto const& k : m_keyboards) {
        if (k->isVirtual() != virt)
            continue;

        k->m_active = false;
    }
}

void CInputManager::newTouchDevice(SP<Aquamarine::ITouch> pDevice) {
    const auto PNEWDEV = m_touches.emplace_back(CTouchDevice::create(pDevice));
    m_hids.emplace_back(PNEWDEV);

    try {
        PNEWDEV->m_hlName = getNameForNewDevice(PNEWDEV->m_deviceName);
    } catch (std::exception& e) {
        Debug::log(ERR, "Touch Device had no name???"); // logic error
    }

    setTouchDeviceConfigs(PNEWDEV);
    g_pPointerManager->attachTouch(PNEWDEV);

    PNEWDEV->m_events.destroy.listenStatic([this, dev = PNEWDEV.get()] {
        auto PDEV = dev->m_self.lock();

        if (!PDEV)
            return;

        destroyTouchDevice(PDEV);
    });

    Debug::log(LOG, "New touch device added at {:x}", rc<uintptr_t>(PNEWDEV.get()));
}

void CInputManager::setTouchDeviceConfigs(SP<ITouch> dev) {
    auto setConfig = [](SP<ITouch> PTOUCHDEV) -> void {
        if (PTOUCHDEV->aq() && PTOUCHDEV->aq()->getLibinputHandle()) {
            const auto LIBINPUTDEV = PTOUCHDEV->aq()->getLibinputHandle();

            const auto ENABLED = g_pConfigManager->getDeviceInt(PTOUCHDEV->m_hlName, "enabled", "input:touchdevice:enabled");
            const auto mode    = ENABLED ? LIBINPUT_CONFIG_SEND_EVENTS_ENABLED : LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
            if (libinput_device_config_send_events_get_mode(LIBINPUTDEV) != mode)
                libinput_device_config_send_events_set_mode(LIBINPUTDEV, mode);

            if (libinput_device_config_calibration_has_matrix(LIBINPUTDEV)) {
                Debug::log(LOG, "Setting calibration matrix for device {}", PTOUCHDEV->m_hlName);
                // default value of transform being -1 means it's unset.
                const int ROTATION = std::clamp(g_pConfigManager->getDeviceInt(PTOUCHDEV->m_hlName, "transform", "input:touchdevice:transform"), -1, 7);
                if (ROTATION > -1)
                    libinput_device_config_calibration_set_matrix(LIBINPUTDEV, MATRICES[ROTATION]);
            }

            auto       output     = g_pConfigManager->getDeviceString(PTOUCHDEV->m_hlName, "output", "input:touchdevice:output");
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
            PTOUCHDEV->m_boundOutput = bound ? output : "";
            const auto PMONITOR      = bound ? g_pCompositor->getMonitorFromName(output) : nullptr;
            if (PMONITOR) {
                Debug::log(LOG, "Binding touch device {} to output {}", PTOUCHDEV->m_hlName, PMONITOR->m_name);
                // wlr_cursor_map_input_to_output(g_pCompositor->m_sWLRCursor, &PTOUCHDEV->wlr()->base, PMONITOR->output);
            } else if (bound)
                Debug::log(ERR, "Failed to bind touch device {} to output '{}': monitor not found", PTOUCHDEV->m_hlName, output);
        }
    };

    if (dev) {
        setConfig(dev);
        return;
    }

    for (auto const& m : m_touches) {
        setConfig(m);
    }
}

void CInputManager::setTabletConfigs() {
    for (auto const& t : m_tablets) {
        if (t->aq()->getLibinputHandle()) {
            const auto NAME        = t->m_hlName;
            const auto LIBINPUTDEV = t->aq()->getLibinputHandle();

            const auto RELINPUT = g_pConfigManager->getDeviceInt(NAME, "relative_input", "input:tablet:relative_input");
            t->m_relativeInput  = RELINPUT;

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
                t->m_boundOutput = OUTPUT;
            } else
                t->m_boundOutput = "";

            const auto REGION_POS  = g_pConfigManager->getDeviceVec(NAME, "region_position", "input:tablet:region_position");
            const auto REGION_SIZE = g_pConfigManager->getDeviceVec(NAME, "region_size", "input:tablet:region_size");
            t->m_boundBox          = {REGION_POS, REGION_SIZE};

            const auto ABSOLUTE_REGION_POS = g_pConfigManager->getDeviceInt(NAME, "absolute_region_position", "input:tablet:absolute_region_position");
            t->m_absolutePos               = ABSOLUTE_REGION_POS;

            const auto ACTIVE_AREA_SIZE = g_pConfigManager->getDeviceVec(NAME, "active_area_size", "input:tablet:active_area_size");
            const auto ACTIVE_AREA_POS  = g_pConfigManager->getDeviceVec(NAME, "active_area_position", "input:tablet:active_area_position");
            if (ACTIVE_AREA_SIZE.x != 0 || ACTIVE_AREA_SIZE.y != 0) {
                // Rotations with an odd index (90 and 270 degrees, and their flipped variants) swap the X and Y axes.
                // Use swapped dimensions when the axes are rotated, otherwise keep the original ones.
                const Vector2D effectivePhysicalSize = (ROTATION % 2) ? Vector2D{t->aq()->physicalSize.y, t->aq()->physicalSize.x} : t->aq()->physicalSize;

                // Scale the active area coordinates into normalized space (0–1) using the effective dimensions.
                t->m_activeArea = CBox{ACTIVE_AREA_POS.x / effectivePhysicalSize.x, ACTIVE_AREA_POS.y / effectivePhysicalSize.y,
                                       (ACTIVE_AREA_POS.x + ACTIVE_AREA_SIZE.x) / effectivePhysicalSize.x, (ACTIVE_AREA_POS.y + ACTIVE_AREA_SIZE.y) / effectivePhysicalSize.y};
            }
        }
    }
}

void CInputManager::newSwitch(SP<Aquamarine::ISwitch> pDevice) {
    const auto PNEWDEV = &m_switches.emplace_back();
    PNEWDEV->pDevice   = pDevice;

    Debug::log(LOG, "New switch with name \"{}\" added", pDevice->getName());

    PNEWDEV->listeners.destroy = pDevice->events.destroy.listen([this, PNEWDEV] { destroySwitch(PNEWDEV); });

    PNEWDEV->listeners.fire = pDevice->events.fire.listen([PNEWDEV](const Aquamarine::ISwitch::SFireEvent& event) {
        const auto NAME = PNEWDEV->pDevice->getName();

        Debug::log(LOG, "Switch {} fired, triggering binds.", NAME);

        g_pKeybindManager->onSwitchEvent(NAME);

        if (event.enable) {
            Debug::log(LOG, "Switch {} turn on, triggering binds.", NAME);
            g_pKeybindManager->onSwitchOnEvent(NAME);
        } else {
            Debug::log(LOG, "Switch {} turn off, triggering binds.", NAME);
            g_pKeybindManager->onSwitchOffEvent(NAME);
        }
    });
}

void CInputManager::destroySwitch(SSwitchDevice* pDevice) {
    m_switches.remove(*pDevice);
}

void CInputManager::setCursorImageUntilUnset(std::string name) {
    g_pHyprRenderer->setCursorFromName(name);
    m_cursorImageOverridden   = true;
    m_cursorSurfaceInfo.inUse = false;
}

void CInputManager::unsetCursorImage() {
    if (!m_cursorImageOverridden)
        return;

    m_cursorImageOverridden = false;
    restoreCursorIconToApp();
}

std::string CInputManager::getNameForNewDevice(std::string internalName) {

    auto proposedNewName = deviceNameToInternalString(internalName);
    int  dupeno          = 0;

    auto makeNewName = [&]() { return (proposedNewName.empty() ? "unknown-device" : proposedNewName) + (dupeno == 0 ? "" : ("-" + std::to_string(dupeno))); };

    while (std::ranges::find_if(m_hids, [&](const auto& other) { return other->m_hlName == makeNewName(); }) != m_hids.end())
        dupeno++;

    return makeNewName();
}

void CInputManager::releaseAllMouseButtons() {
    const auto buttonsCopy = m_currentlyHeldButtons;

    if (PROTO::data->dndActive())
        return;

    for (auto const& mb : buttonsCopy) {
        g_pSeatManager->sendPointerButton(Time::millis(Time::steadyNow()), mb, WL_POINTER_BUTTON_STATE_RELEASED);
    }

    m_currentlyHeldButtons.clear();
}

void CInputManager::setCursorIconOnBorder(PHLWINDOW w) {
    // do not override cursor icons set by mouse binds
    if (g_pInputManager->m_currentlyDraggedWindow.expired()) {
        m_borderIconDirection = BORDERICON_NONE;
        return;
    }

    // ignore X11 OR windows, they shouldn't be touched
    if (w->m_isX11 && w->isX11OverrideRedirect())
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
    else if (!boxFullGrabInput.containsPoint(mouseCoords) || (!m_currentlyHeldButtons.empty() && m_currentlyDraggedWindow.expired()))
        direction = BORDERICON_NONE;
    else {

        bool onDeco = false;

        for (auto const& wd : w->m_windowDecorations) {
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

    if (direction == m_borderIconDirection)
        return;

    m_borderIconDirection = direction;

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

    if (!m_lastInputMouse && *PWARPFORNONMOUSE)
        g_pPointerManager->warpTo(m_lastMousePos);
}

void CInputManager::onSwipeBegin(IPointer::SSwipeBeginEvent e) {
    EMIT_HOOK_EVENT_CANCELLABLE("swipeBegin", e);

    g_pTrackpadGestures->gestureBegin(e);

    PROTO::pointerGestures->swipeBegin(e.timeMs, e.fingers);
}

void CInputManager::onSwipeUpdate(IPointer::SSwipeUpdateEvent e) {
    EMIT_HOOK_EVENT_CANCELLABLE("swipeUpdate", e);

    g_pTrackpadGestures->gestureUpdate(e);

    PROTO::pointerGestures->swipeUpdate(e.timeMs, e.delta);
}

void CInputManager::onSwipeEnd(IPointer::SSwipeEndEvent e) {
    EMIT_HOOK_EVENT_CANCELLABLE("swipeEnd", e);

    g_pTrackpadGestures->gestureEnd(e);

    PROTO::pointerGestures->swipeEnd(e.timeMs, e.cancelled);
}

void CInputManager::onPinchBegin(IPointer::SPinchBeginEvent e) {
    EMIT_HOOK_EVENT_CANCELLABLE("pinchBegin", e);

    g_pTrackpadGestures->gestureBegin(e);

    PROTO::pointerGestures->pinchBegin(e.timeMs, e.fingers);
}

void CInputManager::onPinchUpdate(IPointer::SPinchUpdateEvent e) {
    EMIT_HOOK_EVENT_CANCELLABLE("pinchUpdate", e);

    g_pTrackpadGestures->gestureUpdate(e);

    PROTO::pointerGestures->pinchUpdate(e.timeMs, e.delta, e.scale, e.rotation);
}

void CInputManager::onPinchEnd(IPointer::SPinchEndEvent e) {
    EMIT_HOOK_EVENT_CANCELLABLE("pinchEnd", e);

    g_pTrackpadGestures->gestureEnd(e);

    PROTO::pointerGestures->pinchEnd(e.timeMs, e.cancelled);
}
