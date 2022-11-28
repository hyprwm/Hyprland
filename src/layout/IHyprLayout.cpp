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
            const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
            pWindow->m_vLastFloatingSize = PMONITOR->vecSize / 2.f;
        } else {
            pWindow->m_vLastFloatingSize = Vector2D(desiredGeometry.width, desiredGeometry.height);
        }

        onWindowCreatedTiling(pWindow);
    }
}

void IHyprLayout::onWindowRemoved(CWindow* pWindow) {
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
        const auto PWINDOWSURFACE = g_pXWaylandManager->getWindowSurface(pWindow);
        pWindow->m_vRealSize = Vector2D(PWINDOWSURFACE->current.width, PWINDOWSURFACE->current.height);

        if ((desiredGeometry.width <= 1 || desiredGeometry.height <= 1) && pWindow->m_bIsX11 && pWindow->m_iX11Type == 2) { // XDG windows should be fine. TODO: check for weird atoms?
            pWindow->setHidden(true);
            return;
        }

        // reject any windows with size <= 5x5
        if (pWindow->m_vRealSize.goalv().x <= 5 || pWindow->m_vRealSize.goalv().y <= 5) {
            pWindow->m_vRealSize = PMONITOR->vecSize / 2.f;
        }

        pWindow->m_vRealPosition = Vector2D(PMONITOR->vecPosition.x + (PMONITOR->vecSize.x - pWindow->m_vRealSize.goalv().x) / 2.f, PMONITOR->vecPosition.y + (PMONITOR->vecSize.y - pWindow->m_vRealSize.goalv().y) / 2.f);
    } else {
        // we respect the size.
        pWindow->m_vRealSize = Vector2D(desiredGeometry.width, desiredGeometry.height);

        // check if it's on the correct monitor!
        Vector2D middlePoint = Vector2D(desiredGeometry.x, desiredGeometry.y) + Vector2D(desiredGeometry.width, desiredGeometry.height) / 2.f;

        // check if it's visible on any monitor (only for XDG)
        bool visible = pWindow->m_bIsX11;

        if (!pWindow->m_bIsX11) {
            for (auto& m : g_pCompositor->m_vMonitors) {
                if (VECINRECT(Vector2D(desiredGeometry.x, desiredGeometry.y), m->vecPosition.x, m->vecPosition.y, m->vecPosition.x + m->vecSize.x, m->vecPosition.y + m->vecPosition.y)
                    || VECINRECT(Vector2D(desiredGeometry.x + desiredGeometry.width, desiredGeometry.y), m->vecPosition.x, m->vecPosition.y, m->vecPosition.x + m->vecSize.x, m->vecPosition.y + m->vecPosition.y)
                    || VECINRECT(Vector2D(desiredGeometry.x, desiredGeometry.y + desiredGeometry.height), m->vecPosition.x, m->vecPosition.y, m->vecPosition.x + m->vecSize.x, m->vecPosition.y + m->vecPosition.y)
                    || VECINRECT(Vector2D(desiredGeometry.x + desiredGeometry.width, desiredGeometry.y + desiredGeometry.height), m->vecPosition.x, m->vecPosition.y, m->vecPosition.x + m->vecSize.x, m->vecPosition.y + m->vecPosition.y)) {

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

    if (pWindow->m_bX11DoesntWantBorders) {
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
        return;
    }

    if (DRAGGINGWINDOW->m_bIsFullscreen) {
        Debug::log(LOG, "Rejecting drag on a fullscreen window.");
        return;
    }

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(DRAGGINGWINDOW->m_iWorkspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow) {
        Debug::log(LOG, "Rejecting drag on a fullscreen workspace.");
        return;
    }

    g_pInputManager->setCursorImageUntilUnset("hand1");

    DRAGGINGWINDOW->m_vRealPosition.setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsMove"));
    DRAGGINGWINDOW->m_vRealSize.setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsMove"));

    DRAGGINGWINDOW->m_bDraggingTiled = false;

    if (!DRAGGINGWINDOW->m_bIsFloating) {
        if (g_pInputManager->dragMode == MBIND_MOVE) {
            changeWindowFloatingMode(DRAGGINGWINDOW);
            DRAGGINGWINDOW->m_bIsFloating = true;
            DRAGGINGWINDOW->m_bDraggingTiled = true;

            DRAGGINGWINDOW->m_vRealPosition = g_pInputManager->getMouseCoordsInternal() - DRAGGINGWINDOW->m_vRealSize.goalv() / 2.f;
        }
    }

    m_vBeginDragXY = g_pInputManager->getMouseCoordsInternal();
    m_vBeginDragPositionXY = DRAGGINGWINDOW->m_vRealPosition.goalv();
    m_vBeginDragSizeXY = DRAGGINGWINDOW->m_vRealSize.goalv();
    m_vLastDragXY = m_vBeginDragXY;

    // get the grab corner
    if (m_vBeginDragXY.x < m_vBeginDragPositionXY.x + m_vBeginDragSizeXY.x / 2.0) {
        // left
        if (m_vBeginDragXY.y < m_vBeginDragPositionXY.y + m_vBeginDragSizeXY.y / 2.0)
            m_iGrabbedCorner = 0;
        else
            m_iGrabbedCorner = 4;
    } else {
        // right
        if (m_vBeginDragXY.y < m_vBeginDragPositionXY.y + m_vBeginDragSizeXY.y / 2.0)
            m_iGrabbedCorner = 1;
        else
            m_iGrabbedCorner = 3;
    }

    g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);

    // shadow to ignore any bound to MAIN_MOD
    g_pKeybindManager->shadowKeybinds();
}

void IHyprLayout::onEndDragWindow() {
    const auto DRAGGINGWINDOW = g_pInputManager->currentlyDraggedWindow;

    if (!g_pCompositor->windowValidMapped(DRAGGINGWINDOW))
        return;

    g_pInputManager->unsetCursorImage();

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
        g_pInputManager->currentlyDraggedWindow = nullptr;
        return;
    }

    const auto DELTA = Vector2D(mousePos.x - m_vBeginDragXY.x, mousePos.y - m_vBeginDragXY.y);
    const auto TICKDELTA = Vector2D(mousePos.x - m_vLastDragXY.x, mousePos.y - m_vLastDragXY.y);

    const auto PANIMATE = &g_pConfigManager->getConfigValuePtr("misc:animate_manual_resizes")->intValue;

    if (abs(TICKDELTA.x) < 1.f && abs(TICKDELTA.y) < 1.f)
        return;

    m_vLastDragXY = mousePos;

    g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);

    if (g_pInputManager->dragMode == MBIND_MOVE) {

        if (*PANIMATE) {
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
            Vector2D newPos = m_vBeginDragPositionXY;

            if (m_iGrabbedCorner == 3) {
                newSize = newSize + DELTA;
            } else if (m_iGrabbedCorner == 0) {
                newSize = newSize - DELTA;
                newPos = newPos + DELTA;
            } else if (m_iGrabbedCorner == 1) {
                newSize = newSize + Vector2D(DELTA.x, -DELTA.y);
                newPos = newPos + Vector2D(0, DELTA.y);
            } else if (m_iGrabbedCorner == 4) {
                newSize = newSize + Vector2D(-DELTA.x, DELTA.y);
                newPos = newPos + Vector2D(DELTA.x, 0);
            }

            newSize = newSize.clamp(Vector2D(20,20), MAXSIZE);

            if (*PANIMATE) {
                DRAGGINGWINDOW->m_vRealSize = newSize;
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

    if (PMONITOR) {
        DRAGGINGWINDOW->m_iMonitorID = PMONITOR->ID;
        DRAGGINGWINDOW->moveToWorkspace(PMONITOR->activeWorkspace);

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

    if (!TILED) {
        const auto PNEWMON = g_pCompositor->getMonitorFromVector(pWindow->m_vRealPosition.vec() + pWindow->m_vRealSize.vec() / 2.f);
        pWindow->m_iMonitorID = PNEWMON->ID;
        pWindow->moveToWorkspace(PNEWMON->activeWorkspace);

        // save real pos cuz the func applies the default 5,5 mid
        const auto PSAVEDPOS = pWindow->m_vRealPosition.goalv();
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

        pWindow->m_vRealPosition = pWindow->m_vRealPosition.goalv() + (pWindow->m_vRealSize.goalv() - pWindow->m_vLastFloatingSize) / 2.f;
        pWindow->m_vRealSize = pWindow->m_vLastFloatingSize;

        pWindow->m_vSize = pWindow->m_vRealSize.goalv();
        pWindow->m_vPosition = pWindow->m_vRealPosition.goalv();

        g_pHyprRenderer->damageMonitor(g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID));

        pWindow->m_sSpecialRenderData.rounding = true;

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
            if (w->m_bIsMapped && !w->isHidden() && w->m_bIsFloating && w->m_iX11Type != 2 && w->m_iWorkspaceID == pWindow->m_iWorkspaceID && !w->m_bX11ShouldntFocus && !w->m_bNoFocus) {
                if (VECINRECT((pWindow->m_vSize / 2.f + pWindow->m_vPosition), w->m_vPosition.x, w->m_vPosition.y, w->m_vPosition.x + w->m_vSize.x, w->m_vPosition.y + w->m_vSize.y)) {
                    return w.get();
                }
            }
        }

        // let's try the last tiled window.
        if (m_pLastTiledWindow && m_pLastTiledWindow->m_iWorkspaceID == pWindow->m_iWorkspaceID)
            return m_pLastTiledWindow;

        // if we don't, let's try to find any window that is in the middle
        if (const auto PWINDOWCANDIDATE = g_pCompositor->vectorToWindowIdeal(pWindow->m_vRealPosition.goalv() + pWindow->m_vRealSize.goalv() / 2.f); PWINDOWCANDIDATE)
            return PWINDOWCANDIDATE;

        // if not, floating window
        for (auto& w : g_pCompositor->m_vWindows) {
            if (w->m_bIsMapped && !w->isHidden() && w->m_bIsFloating && w->m_iX11Type != 2 && w->m_iWorkspaceID == pWindow->m_iWorkspaceID && !w->m_bX11ShouldntFocus && !w->m_bNoFocus)
                return w.get();
        }

        // if there is no candidate, too bad
        return nullptr;
    }

    // if it was a tiled window, we first try to find the window that will replace it.
    const auto PWINDOWCANDIDATE = g_pCompositor->vectorToWindowIdeal(pWindow->m_vRealPosition.goalv() + pWindow->m_vRealSize.goalv() / 2.f);

    if (!PWINDOWCANDIDATE || pWindow == PWINDOWCANDIDATE || !PWINDOWCANDIDATE->m_bIsMapped || PWINDOWCANDIDATE->isHidden() || PWINDOWCANDIDATE->m_bX11ShouldntFocus || PWINDOWCANDIDATE->m_iX11Type == 2 || PWINDOWCANDIDATE->m_iMonitorID != g_pCompositor->m_pLastMonitor->ID)
        return nullptr;

    return PWINDOWCANDIDATE;
}

IHyprLayout::~IHyprLayout() {
}
