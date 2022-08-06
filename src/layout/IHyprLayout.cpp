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
            pWindow->m_bHidden = true;
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

        // TODO: detect a popup in a more consistent way.
        if ((desiredGeometry.x == 0 && desiredGeometry.y == 0)) {
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
        g_pCompositor->fixXWaylandWindowsOnWorkspace(PMONITOR->activeWorkspace);

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

    DRAGGINGWINDOW->m_vRealPosition.setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsMove"));
    DRAGGINGWINDOW->m_vRealSize.setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsMove"));

    DRAGGINGWINDOW->m_bDraggingTiled = false;

    if (!DRAGGINGWINDOW->m_bIsFloating) {
        if (g_pInputManager->dragButton == BTN_LEFT) {
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

    g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);

    // shadow to ignore any bound to MAIN_MOD
    g_pKeybindManager->shadowKeybinds();
}

void IHyprLayout::onEndDragWindow() {
    const auto DRAGGINGWINDOW = g_pInputManager->currentlyDraggedWindow;

    if (!g_pCompositor->windowValidMapped(DRAGGINGWINDOW))
        return;

    if (DRAGGINGWINDOW->m_bDraggingTiled) {
        DRAGGINGWINDOW->m_bIsFloating = false;
        changeWindowFloatingMode(DRAGGINGWINDOW);
    }

    g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);
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

    if (abs(TICKDELTA.x) < 1.f && abs(TICKDELTA.y) < 1.f)
        return;

    m_vLastDragXY = mousePos;

    g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);

    if (g_pInputManager->dragButton == BTN_LEFT) {
        DRAGGINGWINDOW->m_vRealPosition.setValueAndWarp(m_vBeginDragPositionXY + DELTA);

        g_pXWaylandManager->setWindowSize(DRAGGINGWINDOW, DRAGGINGWINDOW->m_vRealSize.goalv());
    } else {
        if (DRAGGINGWINDOW->m_bIsFloating) {

            const auto MAXSIZE = g_pXWaylandManager->getMaxSizeForWindow(DRAGGINGWINDOW);

            DRAGGINGWINDOW->m_vRealSize.setValueAndWarp(m_vBeginDragSizeXY + DELTA);
            DRAGGINGWINDOW->m_vRealSize.setValueAndWarp(Vector2D(std::clamp(DRAGGINGWINDOW->m_vRealSize.vec().x, (double)20, (double)MAXSIZE.x), std::clamp(DRAGGINGWINDOW->m_vRealSize.vec().y, (double)20, (double)MAXSIZE.y)));

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
        DRAGGINGWINDOW->m_iWorkspaceID = PMONITOR->activeWorkspace;

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

    const auto TILED = isWindowTiled(pWindow);

    if (!TILED) {
        const auto PNEWMON = g_pCompositor->getMonitorFromVector(pWindow->m_vRealPosition.vec() + pWindow->m_vRealSize.vec() / 2.f);
        pWindow->m_iMonitorID = PNEWMON->ID;
        pWindow->m_iWorkspaceID = PNEWMON->activeWorkspace;

        // save real pos cuz the func applies the default 5,5 mid
        const auto PSAVEDPOS = pWindow->m_vRealPosition.vec();
        const auto PSAVEDSIZE = pWindow->m_vRealSize.vec();

        // if the window is pseudo, update its size
        pWindow->m_vPseudoSize = pWindow->m_vRealSize.vec();

        pWindow->m_vLastFloatingSize = PSAVEDSIZE;

        onWindowCreatedTiling(pWindow);

        pWindow->m_vRealPosition.setValue(PSAVEDPOS);
        pWindow->m_vRealSize.setValue(PSAVEDSIZE);

        // fix pseudo leaving artifacts
        g_pHyprRenderer->damageMonitor(g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID));
    } else {
        pWindow->m_vSize = pWindow->m_vRealSize.vec();
        pWindow->m_vPosition = pWindow->m_vRealPosition.vec();

        onWindowRemovedTiling(pWindow);

        g_pCompositor->moveWindowToTop(pWindow);

        pWindow->m_vRealPosition = pWindow->m_vRealPosition.vec() + (pWindow->m_vRealSize.vec() - pWindow->m_vLastFloatingSize) / 2.f;
        pWindow->m_vRealSize = pWindow->m_vLastFloatingSize;

        g_pHyprRenderer->damageMonitor(g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID));

        pWindow->m_sSpecialRenderData.rounding = true;
    }

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