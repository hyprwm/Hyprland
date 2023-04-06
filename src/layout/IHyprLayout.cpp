#include "IHyprLayout.hpp"
#include "../defines.hpp"
#include "../Compositor.hpp"

void IHyprLayout::onWindowCreated(CWindow* pWindow) {
    if (pWindow->m_bIsFloating) {
        onWindowCreatedFloating(pWindow);
    } else {
        wlr_box desiredGeometry = {0};
        g_pXWaylandManager->getGeometryForWindow(pWindow, &desiredGeometry);

        if (desiredGeometry.width <= 5 || desiredGeometry.height <= 5) {
            const auto PMONITOR          = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
            pWindow->m_vLastFloatingSize = PMONITOR->vecSize / 2.f;
        } else {
            pWindow->m_vLastFloatingSize = Vector2D(desiredGeometry.width, desiredGeometry.height);
        }

        onWindowCreatedTiling(pWindow);
    }
}

void IHyprLayout::onWindowRemoved(CWindow* pWindow) {
    if (pWindow->m_bIsFullscreen)
        g_pCompositor->setWindowFullscreen(pWindow, false, FULLSCREEN_FULL);

    if (pWindow->m_sGroupData.pNextWindow) {
        if (pWindow->m_sGroupData.pNextWindow == pWindow)
            pWindow->m_sGroupData.pNextWindow = nullptr;
        else {
            // find last window and update
            CWindow*   curr           = pWindow;
            const auto CURRWASVISIBLE = curr->getGroupCurrent() == curr;

            while (curr->m_sGroupData.pNextWindow != pWindow)
                curr = curr->m_sGroupData.pNextWindow;

            if (CURRWASVISIBLE)
                curr->setGroupCurrent(curr);

            curr->m_sGroupData.pNextWindow = pWindow->m_sGroupData.pNextWindow;

            pWindow->m_sGroupData.pNextWindow = nullptr;

            if (pWindow->m_sGroupData.head) {
                pWindow->m_sGroupData.head = false;
                curr->m_sGroupData.head    = true;
            }

            if (pWindow == m_pLastTiledWindow)
                m_pLastTiledWindow = nullptr;

            pWindow->setHidden(false);

            return;
        }
    }

    if (pWindow->m_bIsFloating) {
        onWindowRemovedFloating(pWindow);
    } else {
        onWindowRemovedTiling(pWindow);
    }

    if (pWindow == m_pLastTiledWindow)
        m_pLastTiledWindow = nullptr;
}

void IHyprLayout::onWindowRemovedFloating(CWindow* pWindow) {
    return; // no-op
}

void IHyprLayout::onWindowCreatedFloating(CWindow* pWindow) {
    wlr_box desiredGeometry = {0};
    g_pXWaylandManager->getGeometryForWindow(pWindow, &desiredGeometry);
    const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    if (!PMONITOR) {
        Debug::log(ERR, "Window %x (%s) has an invalid monitor in onWindowCreatedFloating!!!", pWindow, pWindow->m_szTitle.c_str());
        return;
    }

    if (desiredGeometry.width <= 5 || desiredGeometry.height <= 5) {
        const auto PWINDOWSURFACE = pWindow->m_pWLSurface.wlr();
        pWindow->m_vRealSize      = Vector2D(PWINDOWSURFACE->current.width, PWINDOWSURFACE->current.height);

        if ((desiredGeometry.width <= 1 || desiredGeometry.height <= 1) && pWindow->m_bIsX11 &&
            pWindow->m_iX11Type == 2) { // XDG windows should be fine. TODO: check for weird atoms?
            pWindow->setHidden(true);
            return;
        }

        // reject any windows with size <= 5x5
        if (pWindow->m_vRealSize.goalv().x <= 5 || pWindow->m_vRealSize.goalv().y <= 5) {
            pWindow->m_vRealSize = PMONITOR->vecSize / 2.f;
        }

        if (pWindow->m_bIsX11 && pWindow->m_uSurface.xwayland->override_redirect) {

            if (pWindow->m_uSurface.xwayland->x != 0 && pWindow->m_uSurface.xwayland->y != 0)
                pWindow->m_vRealPosition = Vector2D{pWindow->m_uSurface.xwayland->x, pWindow->m_uSurface.xwayland->y};
            else
                pWindow->m_vRealPosition = Vector2D(PMONITOR->vecPosition.x + (PMONITOR->vecSize.x - pWindow->m_vRealSize.goalv().x) / 2.f,
                                                    PMONITOR->vecPosition.y + (PMONITOR->vecSize.y - pWindow->m_vRealSize.goalv().y) / 2.f);
        } else {
            pWindow->m_vRealPosition = Vector2D(PMONITOR->vecPosition.x + (PMONITOR->vecSize.x - pWindow->m_vRealSize.goalv().x) / 2.f,
                                                PMONITOR->vecPosition.y + (PMONITOR->vecSize.y - pWindow->m_vRealSize.goalv().y) / 2.f);
        }
    } else {
        // we respect the size.
        pWindow->m_vRealSize = Vector2D(desiredGeometry.width, desiredGeometry.height);

        // check if it's on the correct monitor!
        Vector2D middlePoint = Vector2D(desiredGeometry.x, desiredGeometry.y) + Vector2D(desiredGeometry.width, desiredGeometry.height) / 2.f;

        // check if it's visible on any monitor (only for XDG)
        bool visible = pWindow->m_bIsX11;

        if (!pWindow->m_bIsX11) {
            for (auto& m : g_pCompositor->m_vMonitors) {
                if (VECINRECT(Vector2D(desiredGeometry.x, desiredGeometry.y), m->vecPosition.x, m->vecPosition.y, m->vecPosition.x + m->vecSize.x,
                              m->vecPosition.y + m->vecPosition.y) ||
                    VECINRECT(Vector2D(desiredGeometry.x + desiredGeometry.width, desiredGeometry.y), m->vecPosition.x, m->vecPosition.y, m->vecPosition.x + m->vecSize.x,
                              m->vecPosition.y + m->vecPosition.y) ||
                    VECINRECT(Vector2D(desiredGeometry.x, desiredGeometry.y + desiredGeometry.height), m->vecPosition.x, m->vecPosition.y, m->vecPosition.x + m->vecSize.x,
                              m->vecPosition.y + m->vecPosition.y) ||
                    VECINRECT(Vector2D(desiredGeometry.x + desiredGeometry.width, desiredGeometry.y + desiredGeometry.height), m->vecPosition.x, m->vecPosition.y,
                              m->vecPosition.x + m->vecSize.x, m->vecPosition.y + m->vecPosition.y)) {

                    visible = true;
                    break;
                }
            }
        }

        // TODO: detect a popup in a more consistent way.
        if ((desiredGeometry.x == 0 && desiredGeometry.y == 0) || !visible) {
            // if it's not, fall back to the center placement
            pWindow->m_vRealPosition = PMONITOR->vecPosition + Vector2D((PMONITOR->vecSize.x - desiredGeometry.width) / 2.f, (PMONITOR->vecSize.y - desiredGeometry.height) / 2.f);
        } else {
            // if it is, we respect where it wants to put itself, but apply monitor offset if outside
            // most of these are popups

            if (const auto POPENMON = g_pCompositor->getMonitorFromVector(middlePoint); POPENMON->ID != PMONITOR->ID) {
                pWindow->m_vRealPosition = Vector2D(desiredGeometry.x, desiredGeometry.y) - POPENMON->vecPosition + PMONITOR->vecPosition;
            } else {
                pWindow->m_vRealPosition = Vector2D(desiredGeometry.x, desiredGeometry.y);
            }
        }
    }

    if (pWindow->m_bX11DoesntWantBorders || (pWindow->m_bIsX11 && pWindow->m_uSurface.xwayland->override_redirect)) {
        pWindow->m_vRealPosition.setValue(pWindow->m_vRealPosition.goalv());
        pWindow->m_vRealSize.setValue(pWindow->m_vRealSize.goalv());
    }

    if (pWindow->m_iX11Type != 2) {
        g_pXWaylandManager->setWindowSize(pWindow, pWindow->m_vRealSize.goalv());

        g_pCompositor->moveWindowToTop(pWindow);
    }
}

void IHyprLayout::onBeginDragWindow() {
    const auto DRAGGINGWINDOW = g_pInputManager->currentlyDraggedWindow;

    m_vBeginDragSizeXY = Vector2D();

    // Window will be floating. Let's check if it's valid. It should be, but I don't like crashing.
    if (!g_pCompositor->windowValidMapped(DRAGGINGWINDOW)) {
        Debug::log(ERR, "Dragging attempted on an invalid window!");
        g_pInputManager->currentlyDraggedWindow = nullptr;
        return;
    }

    if (DRAGGINGWINDOW->m_bIsFullscreen) {
        Debug::log(LOG, "Rejecting drag on a fullscreen window.");
        g_pInputManager->currentlyDraggedWindow = nullptr;
        return;
    }

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(DRAGGINGWINDOW->m_iWorkspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow && (!DRAGGINGWINDOW->m_bCreatedOverFullscreen || !DRAGGINGWINDOW->m_bIsFloating)) {
        Debug::log(LOG, "Rejecting drag on a fullscreen workspace. (window under fullscreen)");
        g_pInputManager->currentlyDraggedWindow = nullptr;
        return;
    }

    DRAGGINGWINDOW->m_bDraggingTiled = false;

    if (!DRAGGINGWINDOW->m_bIsFloating) {
        if (g_pInputManager->dragMode == MBIND_MOVE) {
            changeWindowFloatingMode(DRAGGINGWINDOW);
            DRAGGINGWINDOW->m_bIsFloating    = true;
            DRAGGINGWINDOW->m_bDraggingTiled = true;

            DRAGGINGWINDOW->m_vRealPosition = g_pInputManager->getMouseCoordsInternal() - DRAGGINGWINDOW->m_vRealSize.goalv() / 2.f;
        }
    }

    m_vBeginDragXY         = g_pInputManager->getMouseCoordsInternal();
    m_vBeginDragPositionXY = DRAGGINGWINDOW->m_vRealPosition.goalv();
    m_vBeginDragSizeXY     = DRAGGINGWINDOW->m_vRealSize.goalv();
    m_vLastDragXY          = m_vBeginDragXY;

    // get the grab corner
    if (m_vBeginDragXY.x < m_vBeginDragPositionXY.x + m_vBeginDragSizeXY.x / 2.0) {
        if (m_vBeginDragXY.y < m_vBeginDragPositionXY.y + m_vBeginDragSizeXY.y / 2.0) {
            m_eGrabbedCorner = CORNER_TOPLEFT;
            g_pInputManager->setCursorImageUntilUnset("nw-resize");
        } else {
            m_eGrabbedCorner = CORNER_BOTTOMLEFT;
            g_pInputManager->setCursorImageUntilUnset("sw-resize");
        }
    } else {
        if (m_vBeginDragXY.y < m_vBeginDragPositionXY.y + m_vBeginDragSizeXY.y / 2.0) {
            m_eGrabbedCorner = CORNER_TOPRIGHT;
            g_pInputManager->setCursorImageUntilUnset("ne-resize");
        } else {
            m_eGrabbedCorner = CORNER_BOTTOMRIGHT;
            g_pInputManager->setCursorImageUntilUnset("se-resize");
        }
    }

    if (g_pInputManager->dragMode != MBIND_RESIZE)
        g_pInputManager->setCursorImageUntilUnset("grab");

    g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);

    g_pKeybindManager->shadowKeybinds();
}

void IHyprLayout::onEndDragWindow() {
    const auto DRAGGINGWINDOW = g_pInputManager->currentlyDraggedWindow;

    if (!g_pCompositor->windowValidMapped(DRAGGINGWINDOW)) {
        if (DRAGGINGWINDOW) {
            g_pInputManager->unsetCursorImage();
            g_pInputManager->currentlyDraggedWindow = nullptr;
        }
        return;
    }

    g_pInputManager->unsetCursorImage();

    g_pInputManager->currentlyDraggedWindow = nullptr;

    if (DRAGGINGWINDOW->m_bDraggingTiled) {
        DRAGGINGWINDOW->m_bIsFloating = false;
        g_pInputManager->refocus();
        changeWindowFloatingMode(DRAGGINGWINDOW);
    }

    g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);

    g_pCompositor->focusWindow(DRAGGINGWINDOW);
}

void IHyprLayout::onMouseMove(const Vector2D& mousePos) {
    const auto DRAGGINGWINDOW = g_pInputManager->currentlyDraggedWindow;

    // Window invalid or drag begin size 0,0 meaning we rejected it.
    if (!g_pCompositor->windowValidMapped(DRAGGINGWINDOW) || m_vBeginDragSizeXY == Vector2D()) {
        onEndDragWindow();
        g_pInputManager->currentlyDraggedWindow = nullptr;
        return;
    }

    static auto TIMER = std::chrono::high_resolution_clock::now();

    const auto  SPECIAL = g_pCompositor->isWorkspaceSpecial(DRAGGINGWINDOW->m_iWorkspaceID);

    const auto  DELTA     = Vector2D(mousePos.x - m_vBeginDragXY.x, mousePos.y - m_vBeginDragXY.y);
    const auto  TICKDELTA = Vector2D(mousePos.x - m_vLastDragXY.x, mousePos.y - m_vLastDragXY.y);

    const auto  PANIMATEMOUSE = &g_pConfigManager->getConfigValuePtr("misc:animate_mouse_windowdragging")->intValue;
    const auto  PANIMATE      = &g_pConfigManager->getConfigValuePtr("misc:animate_manual_resizes")->intValue;

    if ((abs(TICKDELTA.x) < 1.f && abs(TICKDELTA.y) < 1.f) ||
        (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - TIMER).count() <
         1000.0 / g_pHyprRenderer->m_pMostHzMonitor->refreshRate))
        return;

    TIMER = std::chrono::high_resolution_clock::now();

    m_vLastDragXY = mousePos;

    g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);

    if (g_pInputManager->dragMode == MBIND_MOVE) {

        if (*PANIMATEMOUSE) {
            DRAGGINGWINDOW->m_vRealPosition = m_vBeginDragPositionXY + DELTA;
        } else {
            DRAGGINGWINDOW->m_vRealPosition.setValueAndWarp(m_vBeginDragPositionXY + DELTA);
        }

        g_pXWaylandManager->setWindowSize(DRAGGINGWINDOW, DRAGGINGWINDOW->m_vRealSize.goalv());
    } else if (g_pInputManager->dragMode == MBIND_RESIZE) {
        if (DRAGGINGWINDOW->m_bIsFloating) {

            const auto MAXSIZE = g_pXWaylandManager->getMaxSizeForWindow(DRAGGINGWINDOW);

            // calc the new size and pos

            Vector2D newSize = m_vBeginDragSizeXY;
            Vector2D newPos  = m_vBeginDragPositionXY;

            if (m_eGrabbedCorner == CORNER_BOTTOMRIGHT) {
                newSize = (newSize + DELTA).clamp(Vector2D(20, 20), MAXSIZE);
            } else if (m_eGrabbedCorner == CORNER_TOPLEFT) {
                newSize = (newSize - DELTA).clamp(Vector2D(20, 20), MAXSIZE);
                newPos  = newPos - newSize + m_vBeginDragSizeXY;
            } else if (m_eGrabbedCorner == CORNER_TOPRIGHT) {
                newSize = (newSize + Vector2D(DELTA.x, -DELTA.y)).clamp(Vector2D(20, 20), MAXSIZE);
                newPos  = newPos + Vector2D(0, (m_vBeginDragSizeXY - newSize).y);
            } else if (m_eGrabbedCorner == CORNER_BOTTOMLEFT) {
                newSize = (newSize + Vector2D(-DELTA.x, DELTA.y)).clamp(Vector2D(20, 20), MAXSIZE);
                newPos  = newPos + Vector2D((m_vBeginDragSizeXY - newSize).x, 0);
            }

            if (*PANIMATE) {
                DRAGGINGWINDOW->m_vRealSize     = newSize;
                DRAGGINGWINDOW->m_vRealPosition = newPos;
            } else {
                DRAGGINGWINDOW->m_vRealSize.setValueAndWarp(newSize);
                DRAGGINGWINDOW->m_vRealPosition.setValueAndWarp(newPos);
            }

            g_pXWaylandManager->setWindowSize(DRAGGINGWINDOW, DRAGGINGWINDOW->m_vRealSize.goalv());
        } else {
            resizeActiveWindow(TICKDELTA, DRAGGINGWINDOW);
        }
    }

    // get middle point
    Vector2D middle = DRAGGINGWINDOW->m_vRealPosition.vec() + DRAGGINGWINDOW->m_vRealSize.vec() / 2.f;

    // and check its monitor
    const auto PMONITOR = g_pCompositor->getMonitorFromVector(middle);

    if (PMONITOR && !SPECIAL) {
        DRAGGINGWINDOW->m_iMonitorID = PMONITOR->ID;
        DRAGGINGWINDOW->moveToWorkspace(PMONITOR->activeWorkspace);
        DRAGGINGWINDOW->updateGroupOutputs();

        DRAGGINGWINDOW->updateToplevel();
    }

    DRAGGINGWINDOW->updateWindowDecos();

    g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);
}

void IHyprLayout::changeWindowFloatingMode(CWindow* pWindow) {

    if (pWindow->m_bIsFullscreen) {
        Debug::log(LOG, "Rejecting a change float order because window is fullscreen.");

        // restore its' floating mode
        pWindow->m_bIsFloating = !pWindow->m_bIsFloating;
        return;
    }

    pWindow->m_bPinned = false;

    const auto TILED = isWindowTiled(pWindow);

    // event
    g_pEventManager->postEvent(SHyprIPCEvent{"changefloatingmode", getFormat("%x,%d", pWindow, (int)TILED)});
    EMIT_HOOK_EVENT("changeFloatingMode", pWindow);

    if (!TILED) {
        const auto PNEWMON    = g_pCompositor->getMonitorFromVector(pWindow->m_vRealPosition.vec() + pWindow->m_vRealSize.vec() / 2.f);
        pWindow->m_iMonitorID = PNEWMON->ID;
        pWindow->moveToWorkspace(PNEWMON->activeWorkspace);
        pWindow->updateGroupOutputs();

        // save real pos cuz the func applies the default 5,5 mid
        const auto PSAVEDPOS  = pWindow->m_vRealPosition.goalv();
        const auto PSAVEDSIZE = pWindow->m_vRealSize.goalv();

        // if the window is pseudo, update its size
        pWindow->m_vPseudoSize = pWindow->m_vRealSize.goalv();

        pWindow->m_vLastFloatingSize = PSAVEDSIZE;

        // move to narnia because we don't wanna find our own node. onWindowCreatedTiling should apply the coords back.
        pWindow->m_vPosition = Vector2D(-999999, -999999);

        onWindowCreatedTiling(pWindow);

        pWindow->m_vRealPosition.setValue(PSAVEDPOS);
        pWindow->m_vRealSize.setValue(PSAVEDSIZE);

        // fix pseudo leaving artifacts
        g_pHyprRenderer->damageMonitor(g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID));

        if (pWindow == g_pCompositor->m_pLastWindow)
            m_pLastTiledWindow = pWindow;
    } else {
        onWindowRemovedTiling(pWindow);

        g_pCompositor->moveWindowToTop(pWindow);

        if (DELTALESSTHAN(pWindow->m_vRealSize.vec().x, pWindow->m_vLastFloatingSize.x, 10) && DELTALESSTHAN(pWindow->m_vRealSize.vec().y, pWindow->m_vLastFloatingSize.y, 10)) {
            pWindow->m_vRealPosition = pWindow->m_vRealPosition.goalv() + (pWindow->m_vRealSize.goalv() - pWindow->m_vLastFloatingSize) / 2.f + Vector2D{10, 10};
            pWindow->m_vRealSize     = pWindow->m_vLastFloatingSize - Vector2D{20, 20};
        }

        pWindow->m_vRealPosition = pWindow->m_vRealPosition.goalv() + (pWindow->m_vRealSize.goalv() - pWindow->m_vLastFloatingSize) / 2.f;
        pWindow->m_vRealSize     = pWindow->m_vLastFloatingSize;

        pWindow->m_vSize     = pWindow->m_vRealSize.goalv();
        pWindow->m_vPosition = pWindow->m_vRealPosition.goalv();

        g_pHyprRenderer->damageMonitor(g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID));

        pWindow->m_sSpecialRenderData.rounding = true;
        pWindow->m_sSpecialRenderData.border   = true;
        pWindow->m_sSpecialRenderData.decorate = true;

        if (pWindow == m_pLastTiledWindow)
            m_pLastTiledWindow = nullptr;
    }

    g_pCompositor->updateWindowAnimatedDecorationValues(pWindow);

    pWindow->updateToplevel();
}

void IHyprLayout::moveActiveWindow(const Vector2D& delta, CWindow* pWindow) {
    const auto PWINDOW = pWindow ? pWindow : g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    if (!PWINDOW->m_bIsFloating) {
        Debug::log(LOG, "Dwindle cannot move a tiled window in moveActiveWindow!");
        return;
    }

    PWINDOW->m_vRealPosition = PWINDOW->m_vRealPosition.goalv() + delta;

    g_pHyprRenderer->damageWindow(PWINDOW);
}

void IHyprLayout::onWindowFocusChange(CWindow* pNewFocus) {
    m_pLastTiledWindow = pNewFocus && !pNewFocus->m_bIsFloating ? pNewFocus : m_pLastTiledWindow;
}

CWindow* IHyprLayout::getNextWindowCandidate(CWindow* pWindow) {
    // although we don't expect nullptrs here, let's verify jic
    if (!pWindow)
        return nullptr;

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    // first of all, if this is a fullscreen workspace,
    if (PWORKSPACE->m_bHasFullscreenWindow)
        return g_pCompositor->getFullscreenWindowOnWorkspace(pWindow->m_iWorkspaceID);

    if (pWindow->m_bIsFloating) {

        // find whether there is a floating window below this one
        for (auto& w : g_pCompositor->m_vWindows) {
            if (w->m_bIsMapped && !w->isHidden() && w->m_bIsFloating && w->m_iX11Type != 2 && w->m_iWorkspaceID == pWindow->m_iWorkspaceID && !w->m_bX11ShouldntFocus &&
                !w->m_bNoFocus && w.get() != pWindow) {
                if (VECINRECT((pWindow->m_vSize / 2.f + pWindow->m_vPosition), w->m_vPosition.x, w->m_vPosition.y, w->m_vPosition.x + w->m_vSize.x,
                              w->m_vPosition.y + w->m_vSize.y)) {
                    return w.get();
                }
            }
        }

        // let's try the last tiled window.
        if (m_pLastTiledWindow && m_pLastTiledWindow->m_iWorkspaceID == pWindow->m_iWorkspaceID)
            return m_pLastTiledWindow;

        // if we don't, let's try to find any window that is in the middle
        if (const auto PWINDOWCANDIDATE = g_pCompositor->vectorToWindowIdeal(pWindow->m_vRealPosition.goalv() + pWindow->m_vRealSize.goalv() / 2.f);
            PWINDOWCANDIDATE && PWINDOWCANDIDATE != pWindow)
            return PWINDOWCANDIDATE;

        // if not, floating window
        for (auto& w : g_pCompositor->m_vWindows) {
            if (w->m_bIsMapped && !w->isHidden() && w->m_bIsFloating && w->m_iX11Type != 2 && w->m_iWorkspaceID == pWindow->m_iWorkspaceID && !w->m_bX11ShouldntFocus &&
                !w->m_bNoFocus && w.get() != pWindow)
                return w.get();
        }

        // if there is no candidate, too bad
        return nullptr;
    }

    // if it was a tiled window, we first try to find the window that will replace it.
    const auto PWINDOWCANDIDATE = g_pCompositor->vectorToWindowIdeal(pWindow->m_vRealPosition.goalv() + pWindow->m_vRealSize.goalv() / 2.f);

    if (!PWINDOWCANDIDATE || pWindow == PWINDOWCANDIDATE || !PWINDOWCANDIDATE->m_bIsMapped || PWINDOWCANDIDATE->isHidden() || PWINDOWCANDIDATE->m_bX11ShouldntFocus ||
        PWINDOWCANDIDATE->m_iX11Type == 2 || PWINDOWCANDIDATE->m_iMonitorID != g_pCompositor->m_pLastMonitor->ID)
        return nullptr;

    return PWINDOWCANDIDATE;
}

IHyprLayout::~IHyprLayout() {}
