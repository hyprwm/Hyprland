#include "CIHyprLayout.hpp"
#include "../defines.hpp"
#include "../Compositor.hpp"
#include "../render/decorations/CHyprGroupBarDecoration.hpp"
#include "../config/ConfigValue.hpp"
#include "../desktop/Window.hpp"
#include "../protocols/XDGShell.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../xwayland/XSurface.hpp"
#include "../render/Renderer.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/LayoutManager.hpp"
#include "../managers/EventManager.hpp"
#include "../managers/HookSystemManager.hpp"

void CIHyprLayout::onWindowCreated(PHLWINDOW pWindow, eDirection direction) {
    CBox desiredGeometry = g_pXWaylandManager->getGeometryForWindow(pWindow);

    if (desiredGeometry.width <= 5 || desiredGeometry.height <= 5) {
        const auto PMONITOR          = pWindow->m_pMonitor.lock();
        pWindow->m_vLastFloatingSize = PMONITOR->vecSize / 2.f;
    } else
        pWindow->m_vLastFloatingSize = Vector2D(desiredGeometry.width, desiredGeometry.height);

    pWindow->m_vPseudoSize = pWindow->m_vLastFloatingSize;

    bool autoGrouped = CIHyprLayout::onWindowCreatedAutoGroup(pWindow);
    if (autoGrouped)
        return;

    if (pWindow->m_bIsFloating)
        onWindowCreatedFloating(pWindow);
    else
        onWindowCreatedTiling(pWindow, direction);

    if (!g_pXWaylandManager->shouldBeFloated(pWindow)) // do not apply group rules to child windows
        pWindow->applyGroupRules();
}

void CIHyprLayout::onWindowRemoved(PHLWINDOW pWindow) {
    if (pWindow->isFullscreen())
        g_pCompositor->setWindowFullscreenInternal(pWindow, FSMODE_NONE);

    if (!pWindow->m_sGroupData.pNextWindow.expired()) {
        if (pWindow->m_sGroupData.pNextWindow.lock() == pWindow)
            pWindow->m_sGroupData.pNextWindow.reset();
        else {
            // find last window and update
            PHLWINDOW  PWINDOWPREV     = pWindow->getGroupPrevious();
            const auto WINDOWISVISIBLE = pWindow->getGroupCurrent() == pWindow;

            if (WINDOWISVISIBLE)
                PWINDOWPREV->setGroupCurrent(pWindow->m_sGroupData.head ? pWindow->m_sGroupData.pNextWindow.lock() : PWINDOWPREV);

            PWINDOWPREV->m_sGroupData.pNextWindow = pWindow->m_sGroupData.pNextWindow;

            pWindow->m_sGroupData.pNextWindow.reset();

            if (pWindow->m_sGroupData.head) {
                std::swap(PWINDOWPREV->m_sGroupData.pNextWindow->m_sGroupData.head, pWindow->m_sGroupData.head);
                std::swap(PWINDOWPREV->m_sGroupData.pNextWindow->m_sGroupData.locked, pWindow->m_sGroupData.locked);
            }

            if (pWindow == m_pLastTiledWindow)
                m_pLastTiledWindow.reset();

            pWindow->setHidden(false);

            pWindow->updateWindowDecos();
            PWINDOWPREV->getGroupCurrent()->updateWindowDecos();
            g_pCompositor->updateWindowAnimatedDecorationValues(pWindow);

            return;
        }
    }

    if (pWindow->m_bIsFloating) {
        onWindowRemovedFloating(pWindow);
    } else {
        onWindowRemovedTiling(pWindow);
    }

    if (pWindow == m_pLastTiledWindow)
        m_pLastTiledWindow.reset();
}

void CIHyprLayout::onWindowRemovedFloating(PHLWINDOW pWindow) {
    ; // no-op
}

void CIHyprLayout::onWindowCreatedFloating(PHLWINDOW pWindow) {

    CBox       desiredGeometry = g_pXWaylandManager->getGeometryForWindow(pWindow);
    const auto PMONITOR        = pWindow->m_pMonitor.lock();

    if (pWindow->m_bIsX11) {
        Vector2D xy       = {desiredGeometry.x, desiredGeometry.y};
        xy                = g_pXWaylandManager->xwaylandToWaylandCoords(xy);
        desiredGeometry.x = xy.x;
        desiredGeometry.y = xy.y;
    }

    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    if (!PMONITOR) {
        NDebug::log(ERR, "{:m} has an invalid monitor in onWindowCreatedFloating!!!", pWindow);
        return;
    }

    if (desiredGeometry.width <= 5 || desiredGeometry.height <= 5) {
        const auto PWINDOWSURFACE = pWindow->m_pWLSurface->resource();
        *pWindow->m_vRealSize     = PWINDOWSURFACE->current.size;

        if ((desiredGeometry.width <= 1 || desiredGeometry.height <= 1) && pWindow->m_bIsX11 &&
            pWindow->isX11OverrideRedirect()) { // XDG windows should be fine. TODO: check for weird atoms?
            pWindow->setHidden(true);
            return;
        }

        // reject any windows with size <= 5x5
        if (pWindow->m_vRealSize->goal().x <= 5 || pWindow->m_vRealSize->goal().y <= 5)
            *pWindow->m_vRealSize = PMONITOR->vecSize / 2.f;

        if (pWindow->m_bIsX11 && pWindow->isX11OverrideRedirect()) {

            if (pWindow->m_pXWaylandSurface->geometry.x != 0 && pWindow->m_pXWaylandSurface->geometry.y != 0)
                *pWindow->m_vRealPosition = g_pXWaylandManager->xwaylandToWaylandCoords(pWindow->m_pXWaylandSurface->geometry.pos());
            else
                *pWindow->m_vRealPosition = Vector2D(PMONITOR->vecPosition.x + (PMONITOR->vecSize.x - pWindow->m_vRealSize->goal().x) / 2.f,
                                                     PMONITOR->vecPosition.y + (PMONITOR->vecSize.y - pWindow->m_vRealSize->goal().y) / 2.f);
        } else {
            *pWindow->m_vRealPosition = Vector2D(PMONITOR->vecPosition.x + (PMONITOR->vecSize.x - pWindow->m_vRealSize->goal().x) / 2.f,
                                                 PMONITOR->vecPosition.y + (PMONITOR->vecSize.y - pWindow->m_vRealSize->goal().y) / 2.f);
        }
    } else {
        // we respect the size.
        *pWindow->m_vRealSize = Vector2D(desiredGeometry.width, desiredGeometry.height);

        // check if it's on the correct monitor!
        Vector2D middlePoint = Vector2D(desiredGeometry.x, desiredGeometry.y) + Vector2D(desiredGeometry.width, desiredGeometry.height) / 2.f;

        // check if it's visible on any monitor (only for XDG)
        bool visible = pWindow->m_bIsX11;

        if (!visible) {
            visible = g_pCompositor->isPointOnAnyMonitor(Vector2D(desiredGeometry.x, desiredGeometry.y)) &&
                g_pCompositor->isPointOnAnyMonitor(Vector2D(desiredGeometry.x + desiredGeometry.width, desiredGeometry.y)) &&
                g_pCompositor->isPointOnAnyMonitor(Vector2D(desiredGeometry.x, desiredGeometry.y + desiredGeometry.height)) &&
                g_pCompositor->isPointOnAnyMonitor(Vector2D(desiredGeometry.x + desiredGeometry.width, desiredGeometry.y + desiredGeometry.height));
        }

        // TODO: detect a popup in a more consistent way.
        if ((desiredGeometry.x == 0 && desiredGeometry.y == 0) || !visible || !pWindow->m_bIsX11) {
            // if the pos isn't set, fall back to the center placement if it's not a child, otherwise middle of parent if available
            if (!pWindow->m_bIsX11 && pWindow->m_pXDGSurface->toplevel->parent && validMapped(pWindow->m_pXDGSurface->toplevel->parent->window))
                *pWindow->m_vRealPosition = pWindow->m_pXDGSurface->toplevel->parent->window->m_vRealPosition->goal() +
                    pWindow->m_pXDGSurface->toplevel->parent->window->m_vRealSize->goal() / 2.F - desiredGeometry.size() / 2.F;
            else
                *pWindow->m_vRealPosition = PMONITOR->vecPosition + PMONITOR->vecSize / 2.F - desiredGeometry.size() / 2.F;
        } else {
            // if it is, we respect where it wants to put itself, but apply monitor offset if outside
            // most of these are popups

            if (const auto POPENMON = g_pCompositor->getMonitorFromVector(middlePoint); POPENMON->ID != PMONITOR->ID)
                *pWindow->m_vRealPosition = Vector2D(desiredGeometry.x, desiredGeometry.y) - POPENMON->vecPosition + PMONITOR->vecPosition;
            else
                *pWindow->m_vRealPosition = Vector2D(desiredGeometry.x, desiredGeometry.y);
        }
    }

    if (*PXWLFORCESCALEZERO && pWindow->m_bIsX11)
        *pWindow->m_vRealSize = pWindow->m_vRealSize->goal() / PMONITOR->scale;

    if (pWindow->m_bX11DoesntWantBorders || (pWindow->m_bIsX11 && pWindow->isX11OverrideRedirect())) {
        pWindow->m_vRealPosition->warp();
        pWindow->m_vRealSize->warp();
    }

    if (!pWindow->isX11OverrideRedirect())
        g_pCompositor->changeWindowZOrder(pWindow, true);
    else {
        pWindow->m_vPendingReportedSize = pWindow->m_vRealSize->goal();
        pWindow->m_vReportedSize        = pWindow->m_vPendingReportedSize;
    }
}

bool CIHyprLayout::onWindowCreatedAutoGroup(PHLWINDOW pWindow) {
    static auto     PAUTOGROUP       = CConfigValue<Hyprlang::INT>("group:auto_group");
    const PHLWINDOW OPENINGON        = g_pCompositor->m_pLastWindow.lock() && g_pCompositor->m_pLastWindow->m_pWorkspace == pWindow->m_pWorkspace ?
               g_pCompositor->m_pLastWindow.lock() :
               (pWindow->m_pWorkspace ? pWindow->m_pWorkspace->getFirstWindow() : nullptr);
    const bool      FLOATEDINTOTILED = pWindow->m_bIsFloating && !OPENINGON->m_bIsFloating;
    const bool      SWALLOWING       = pWindow->m_pSwallowed || pWindow->m_bGroupSwallowed;

    if ((*PAUTOGROUP || SWALLOWING)                      // continue if auto_group is enabled or if dealing with window swallowing.
        && OPENINGON                                     // this shouldn't be 0, but honestly, better safe than sorry.
        && OPENINGON != pWindow                          // prevent freeze when the "group set" window rule makes the new window to be already a group.
        && OPENINGON->m_sGroupData.pNextWindow.lock()    // check if OPENINGON is a group.
        && pWindow->canBeGroupedInto(OPENINGON)          // check if the new window can be grouped into OPENINGON.
        && !g_pXWaylandManager->shouldBeFloated(pWindow) // don't group child windows. Fix for floated groups. Tiled groups don't need this because we check if !FLOATEDINTOTILED.
        && !FLOATEDINTOTILED) {                          // don't group a new floated window into a tiled group (for convenience).

        pWindow->m_bIsFloating = OPENINGON->m_bIsFloating; // match the floating state. Needed to autogroup a new tiled window into a floated group.

        static auto USECURRPOS = CConfigValue<Hyprlang::INT>("group:insert_after_current");
        (*USECURRPOS ? OPENINGON : OPENINGON->getGroupTail())->insertWindowToGroup(pWindow);

        OPENINGON->setGroupCurrent(pWindow);
        pWindow->applyGroupRules();
        pWindow->updateWindowDecos();
        recalculateWindow(pWindow);

        if (!pWindow->getDecorationByType(DECORATION_GROUPBAR))
            pWindow->addWindowDeco(makeUnique<CHyprGroupBarDecoration>(pWindow));

        return true;
    }

    return false;
}

void CIHyprLayout::onBeginDragWindow() {
    const auto DRAGGINGWINDOW = g_pInputManager->currentlyDraggedWindow.lock();

    m_iMouseMoveEventCount = 1;
    m_vBeginDragSizeXY     = Vector2D();

    // Window will be floating. Let's check if it's valid. It should be, but I don't like crashing.
    if (!validMapped(DRAGGINGWINDOW)) {
        NDebug::log(ERR, "Dragging attempted on an invalid window!");
        g_pKeybindManager->changeMouseBindMode(MBIND_INVALID);
        return;
    }

    if (DRAGGINGWINDOW->isFullscreen()) {
        NDebug::log(LOG, "Dragging a fullscreen window");
        g_pCompositor->setWindowFullscreenInternal(DRAGGINGWINDOW, FSMODE_NONE);
    }

    const auto PWORKSPACE = DRAGGINGWINDOW->m_pWorkspace;

    if (PWORKSPACE->m_bHasFullscreenWindow && (!DRAGGINGWINDOW->m_bCreatedOverFullscreen || !DRAGGINGWINDOW->m_bIsFloating)) {
        NDebug::log(LOG, "Rejecting drag on a fullscreen workspace. (window under fullscreen)");
        g_pKeybindManager->changeMouseBindMode(MBIND_INVALID);
        return;
    }

    DRAGGINGWINDOW->m_bDraggingTiled = false;

    m_vDraggingWindowOriginalFloatSize = DRAGGINGWINDOW->m_vLastFloatingSize;

    if (!DRAGGINGWINDOW->m_bIsFloating) {
        if (g_pInputManager->dragMode == MBIND_MOVE) {
            DRAGGINGWINDOW->m_vLastFloatingSize = (DRAGGINGWINDOW->m_vRealSize->goal() * 0.8489).clamp(Vector2D{5, 5}, Vector2D{}).floor();
            changeWindowFloatingMode(DRAGGINGWINDOW);
            DRAGGINGWINDOW->m_bIsFloating    = true;
            DRAGGINGWINDOW->m_bDraggingTiled = true;

            *DRAGGINGWINDOW->m_vRealPosition = g_pInputManager->getMouseCoordsInternal() - DRAGGINGWINDOW->m_vRealSize->goal() / 2.f;
        }
    }

    m_vBeginDragXY         = g_pInputManager->getMouseCoordsInternal();
    m_vBeginDragPositionXY = DRAGGINGWINDOW->m_vRealPosition->goal();
    m_vBeginDragSizeXY     = DRAGGINGWINDOW->m_vRealSize->goal();
    m_vLastDragXY          = m_vBeginDragXY;

    // get the grab corner
    static auto RESIZECORNER = CConfigValue<Hyprlang::INT>("general:resize_corner");
    if (*RESIZECORNER != 0 && *RESIZECORNER <= 4 && DRAGGINGWINDOW->m_bIsFloating) {
        switch (*RESIZECORNER) {
            case 1:
                m_eGrabbedCorner = CORNER_TOPLEFT;
                g_pInputManager->setCursorImageUntilUnset("nw-resize");
                break;
            case 2:
                m_eGrabbedCorner = CORNER_TOPRIGHT;
                g_pInputManager->setCursorImageUntilUnset("ne-resize");
                break;
            case 3:
                m_eGrabbedCorner = CORNER_BOTTOMRIGHT;
                g_pInputManager->setCursorImageUntilUnset("se-resize");
                break;
            case 4:
                m_eGrabbedCorner = CORNER_BOTTOMLEFT;
                g_pInputManager->setCursorImageUntilUnset("sw-resize");
                break;
        }
    } else if (m_vBeginDragXY.x < m_vBeginDragPositionXY.x + m_vBeginDragSizeXY.x / 2.0) {
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

    if (g_pInputManager->dragMode != MBIND_RESIZE && g_pInputManager->dragMode != MBIND_RESIZE_FORCE_RATIO && g_pInputManager->dragMode != MBIND_RESIZE_BLOCK_RATIO)
        g_pInputManager->setCursorImageUntilUnset("grabbing");

    g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);

    g_pKeybindManager->shadowKeybinds();

    g_pCompositor->focusWindow(DRAGGINGWINDOW);
    g_pCompositor->changeWindowZOrder(DRAGGINGWINDOW, true);
}

void CIHyprLayout::onEndDragWindow() {
    const auto DRAGGINGWINDOW = g_pInputManager->currentlyDraggedWindow.lock();

    m_iMouseMoveEventCount = 1;

    if (!validMapped(DRAGGINGWINDOW)) {
        if (DRAGGINGWINDOW) {
            g_pInputManager->unsetCursorImage();
            g_pInputManager->currentlyDraggedWindow.reset();
        }
        return;
    }

    g_pInputManager->unsetCursorImage();
    g_pInputManager->currentlyDraggedWindow.reset();
    g_pInputManager->m_bWasDraggingWindow = true;

    if (g_pInputManager->dragMode == MBIND_MOVE) {
        g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);
        const auto MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();
        PHLWINDOW  pWindow     = g_pCompositor->vectorToWindowUnified(MOUSECOORDS, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING, DRAGGINGWINDOW);

        if (pWindow) {
            if (pWindow->checkInputOnDecos(INPUT_TYPE_DRAG_END, MOUSECOORDS, DRAGGINGWINDOW))
                return;

            const bool  FLOATEDINTOTILED = !pWindow->m_bIsFloating && !DRAGGINGWINDOW->m_bDraggingTiled;
            static auto PDRAGINTOGROUP   = CConfigValue<Hyprlang::INT>("group:drag_into_group");

            if (pWindow->m_sGroupData.pNextWindow.lock() && DRAGGINGWINDOW->canBeGroupedInto(pWindow) && *PDRAGINTOGROUP == 1 && !FLOATEDINTOTILED) {

                if (DRAGGINGWINDOW->m_sGroupData.pNextWindow) {
                    PHLWINDOW next = DRAGGINGWINDOW->m_sGroupData.pNextWindow.lock();
                    while (next != DRAGGINGWINDOW) {
                        next->m_bIsFloating    = pWindow->m_bIsFloating;           // match the floating state of group members
                        *next->m_vRealSize     = pWindow->m_vRealSize->goal();     // match the size of group members
                        *next->m_vRealPosition = pWindow->m_vRealPosition->goal(); // match the position of group members
                        next                   = next->m_sGroupData.pNextWindow.lock();
                    }
                }

                DRAGGINGWINDOW->m_bIsFloating       = pWindow->m_bIsFloating; // match the floating state of the window
                DRAGGINGWINDOW->m_vLastFloatingSize = m_vDraggingWindowOriginalFloatSize;
                DRAGGINGWINDOW->m_bDraggingTiled    = false;

                static auto USECURRPOS = CConfigValue<Hyprlang::INT>("group:insert_after_current");
                (*USECURRPOS ? pWindow : pWindow->getGroupTail())->insertWindowToGroup(DRAGGINGWINDOW);
                pWindow->setGroupCurrent(DRAGGINGWINDOW);
                DRAGGINGWINDOW->applyGroupRules();
                DRAGGINGWINDOW->updateWindowDecos();

                if (!DRAGGINGWINDOW->getDecorationByType(DECORATION_GROUPBAR))
                    DRAGGINGWINDOW->addWindowDeco(makeUnique<CHyprGroupBarDecoration>(DRAGGINGWINDOW));
            }
        }
    }

    if (DRAGGINGWINDOW->m_bDraggingTiled) {
        DRAGGINGWINDOW->m_bIsFloating = false;
        g_pInputManager->refocus();
        changeWindowFloatingMode(DRAGGINGWINDOW);
        DRAGGINGWINDOW->m_vLastFloatingSize = m_vDraggingWindowOriginalFloatSize;
    }

    g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);
    g_pCompositor->focusWindow(DRAGGINGWINDOW);

    g_pInputManager->m_bWasDraggingWindow = false;
}

static inline bool canSnap(const double SIDEA, const double SIDEB, const double GAP) {
    return std::abs(SIDEA - SIDEB) < GAP;
}

static void snapMove(double& start, double& end, const double P) {
    end   = P + (end - start);
    start = P;
}

static void snapResize(double& start, double& end, const double P) {
    start = P;
}

typedef std::function<void(double&, double&, const double)> SnapFn;

static void performSnap(Vector2D& sourcePos, Vector2D& sourceSize, PHLWINDOW DRAGGINGWINDOW, const eMouseBindMode MODE, const int CORNER, const Vector2D& BEGINSIZE) {
    static auto  SNAPWINDOWGAP     = CConfigValue<Hyprlang::INT>("general:snap:window_gap");
    static auto  SNAPMONITORGAP    = CConfigValue<Hyprlang::INT>("general:snap:monitor_gap");
    static auto  SNAPBORDEROVERLAP = CConfigValue<Hyprlang::INT>("general:snap:border_overlap");

    const SnapFn SNAP  = (MODE == MBIND_MOVE) ? snapMove : snapResize;
    int          snaps = 0;

    const bool   OVERLAP            = *SNAPBORDEROVERLAP;
    const int    DRAGGINGBORDERSIZE = DRAGGINGWINDOW->getRealBorderSize();

    struct SRange {
        double start = 0;
        double end   = 0;
    };
    SRange sourceX = {sourcePos.x, sourcePos.x + sourceSize.x};
    SRange sourceY = {sourcePos.y, sourcePos.y + sourceSize.y};

    if (*SNAPWINDOWGAP) {
        const double GAPSIZE       = *SNAPWINDOWGAP;
        const auto   WSID          = DRAGGINGWINDOW->workspaceID();
        const bool   HASFULLSCREEN = DRAGGINGWINDOW->m_pWorkspace && DRAGGINGWINDOW->m_pWorkspace->m_bHasFullscreenWindow;

        for (auto& other : g_pCompositor->m_vWindows) {
            if ((HASFULLSCREEN && !other->m_bCreatedOverFullscreen) || other == DRAGGINGWINDOW || other->workspaceID() != WSID || !other->m_bIsMapped || other->m_bFadingOut ||
                other->isX11OverrideRedirect())
                continue;

            const int    OTHERBORDERSIZE = other->getRealBorderSize();
            const double BORDERSIZE      = OVERLAP ? std::max(DRAGGINGBORDERSIZE, OTHERBORDERSIZE) : (DRAGGINGBORDERSIZE + OTHERBORDERSIZE);

            const CBox   SURF   = other->getWindowMainSurfaceBox();
            const SRange SURFBX = {SURF.x - BORDERSIZE, SURF.x + SURF.w + BORDERSIZE};
            const SRange SURFBY = {SURF.y - BORDERSIZE, SURF.y + SURF.h + BORDERSIZE};

            // only snap windows if their ranges overlap in the opposite axis
            if (sourceY.start <= SURFBY.end && SURFBY.start <= sourceY.end) {
                if (CORNER & (CORNER_TOPLEFT | CORNER_BOTTOMLEFT) && canSnap(sourceX.start, SURFBX.end, GAPSIZE)) {
                    SNAP(sourceX.start, sourceX.end, SURFBX.end);
                    snaps |= SNAP_LEFT;
                } else if (CORNER & (CORNER_TOPRIGHT | CORNER_BOTTOMRIGHT) && canSnap(sourceX.end, SURFBX.start, GAPSIZE)) {
                    SNAP(sourceX.end, sourceX.start, SURFBX.start);
                    snaps |= SNAP_RIGHT;
                }
            }
            if (sourceX.start <= SURFBX.end && SURFBX.start <= sourceX.end) {
                if (CORNER & (CORNER_TOPLEFT | CORNER_TOPRIGHT) && canSnap(sourceY.start, SURFBY.end, GAPSIZE)) {
                    SNAP(sourceY.start, sourceY.end, SURFBY.end);
                    snaps |= SNAP_UP;
                } else if (CORNER & (CORNER_BOTTOMLEFT | CORNER_BOTTOMRIGHT) && canSnap(sourceY.end, SURFBY.start, GAPSIZE)) {
                    SNAP(sourceY.end, sourceY.start, SURFBY.start);
                    snaps |= SNAP_DOWN;
                }
            }

            // corner snapping
            const double BORDERDIFF = OTHERBORDERSIZE - DRAGGINGBORDERSIZE;
            if (sourceX.start == SURFBX.end || SURFBX.start == sourceX.end) {
                const SRange SURFY = {SURF.y - BORDERDIFF, SURF.y + SURF.h + BORDERDIFF};
                if (CORNER & (CORNER_TOPLEFT | CORNER_TOPRIGHT) && !(snaps & SNAP_UP) && canSnap(sourceY.start, SURFY.start, GAPSIZE)) {
                    SNAP(sourceY.start, sourceY.end, SURFY.start);
                    snaps |= SNAP_UP;
                } else if (CORNER & (CORNER_BOTTOMLEFT | CORNER_BOTTOMRIGHT) && !(snaps & SNAP_DOWN) && canSnap(sourceY.end, SURFY.end, GAPSIZE)) {
                    SNAP(sourceY.end, sourceY.start, SURFY.end);
                    snaps |= SNAP_DOWN;
                }
            }
            if (sourceY.start == SURFBY.end || SURFBY.start == sourceY.end) {
                const SRange SURFX = {SURF.x - BORDERDIFF, SURF.x + SURF.w + BORDERDIFF};
                if (CORNER & (CORNER_TOPLEFT | CORNER_BOTTOMLEFT) && !(snaps & SNAP_LEFT) && canSnap(sourceX.start, SURFX.start, GAPSIZE)) {
                    SNAP(sourceX.start, sourceX.end, SURFX.start);
                    snaps |= SNAP_LEFT;
                } else if (CORNER & (CORNER_TOPRIGHT | CORNER_BOTTOMRIGHT) && !(snaps & SNAP_RIGHT) && canSnap(sourceX.end, SURFX.end, GAPSIZE)) {
                    SNAP(sourceX.end, sourceX.start, SURFX.end);
                    snaps |= SNAP_RIGHT;
                }
            }
        }
    }

    if (*SNAPMONITORGAP) {
        const double GAPSIZE    = *SNAPMONITORGAP;
        const double BORDERDIFF = OVERLAP ? DRAGGINGBORDERSIZE : 0;
        const auto   MON        = DRAGGINGWINDOW->m_pMonitor.lock();

        SRange       monX = {MON->vecPosition.x + MON->vecReservedTopLeft.x + DRAGGINGBORDERSIZE,
                             MON->vecPosition.x + MON->vecSize.x - MON->vecReservedBottomRight.x - DRAGGINGBORDERSIZE};
        SRange       monY = {MON->vecPosition.y + MON->vecReservedTopLeft.y + DRAGGINGBORDERSIZE,
                             MON->vecPosition.y + MON->vecSize.y - MON->vecReservedBottomRight.y - DRAGGINGBORDERSIZE};

        if (CORNER & (CORNER_TOPLEFT | CORNER_BOTTOMLEFT) &&
            ((MON->vecReservedTopLeft.x > 0 && canSnap(sourceX.start, monX.start, GAPSIZE)) ||
             canSnap(sourceX.start, (monX.start -= MON->vecReservedTopLeft.x + BORDERDIFF), GAPSIZE))) {
            SNAP(sourceX.start, sourceX.end, monX.start);
            snaps |= SNAP_LEFT;
        }
        if (CORNER & (CORNER_TOPRIGHT | CORNER_BOTTOMRIGHT) &&
            ((MON->vecReservedBottomRight.x > 0 && canSnap(sourceX.end, monX.end, GAPSIZE)) ||
             canSnap(sourceX.end, (monX.end += MON->vecReservedBottomRight.x + BORDERDIFF), GAPSIZE))) {
            SNAP(sourceX.end, sourceX.start, monX.end);
            snaps |= SNAP_RIGHT;
        }
        if (CORNER & (CORNER_TOPLEFT | CORNER_TOPRIGHT) &&
            ((MON->vecReservedTopLeft.y > 0 && canSnap(sourceY.start, monY.start, GAPSIZE)) ||
             canSnap(sourceY.start, (monY.start -= MON->vecReservedTopLeft.y + BORDERDIFF), GAPSIZE))) {
            SNAP(sourceY.start, sourceY.end, monY.start);
            snaps |= SNAP_UP;
        }
        if (CORNER & (CORNER_BOTTOMLEFT | CORNER_BOTTOMRIGHT) &&
            ((MON->vecReservedBottomRight.y > 0 && canSnap(sourceY.end, monY.end, GAPSIZE)) ||
             canSnap(sourceY.end, (monY.end += MON->vecReservedBottomRight.y + BORDERDIFF), GAPSIZE))) {
            SNAP(sourceY.end, sourceY.start, monY.end);
            snaps |= SNAP_DOWN;
        }
    }

    if (MODE == MBIND_RESIZE_FORCE_RATIO) {
        if ((CORNER & (CORNER_TOPLEFT | CORNER_BOTTOMLEFT) && snaps & SNAP_LEFT) || (CORNER & (CORNER_TOPRIGHT | CORNER_BOTTOMRIGHT) && snaps & SNAP_RIGHT)) {
            const double SIZEY = (sourceX.end - sourceX.start) * (BEGINSIZE.y / BEGINSIZE.x);
            if (CORNER & (CORNER_TOPLEFT | CORNER_TOPRIGHT))
                sourceY.start = sourceY.end - SIZEY;
            else
                sourceY.end = sourceY.start + SIZEY;
        } else if ((CORNER & (CORNER_TOPLEFT | CORNER_TOPRIGHT) && snaps & SNAP_UP) || (CORNER & (CORNER_BOTTOMLEFT | CORNER_BOTTOMRIGHT) && snaps & SNAP_DOWN)) {
            const double SIZEX = (sourceY.end - sourceY.start) * (BEGINSIZE.x / BEGINSIZE.y);
            if (CORNER & (CORNER_TOPLEFT | CORNER_BOTTOMLEFT))
                sourceX.start = sourceX.end - SIZEX;
            else
                sourceX.end = sourceX.start + SIZEX;
        }
    }

    sourcePos  = {sourceX.start, sourceY.start};
    sourceSize = {sourceX.end - sourceX.start, sourceY.end - sourceY.start};
}

void CIHyprLayout::onMouseMove(const Vector2D& mousePos) {
    if (g_pInputManager->currentlyDraggedWindow.expired())
        return;

    const auto DRAGGINGWINDOW = g_pInputManager->currentlyDraggedWindow.lock();

    // Window invalid or drag begin size 0,0 meaning we rejected it.
    if ((!validMapped(DRAGGINGWINDOW) || m_vBeginDragSizeXY == Vector2D())) {
        g_pKeybindManager->changeMouseBindMode(MBIND_INVALID);
        return;
    }

    static auto TIMER = std::chrono::high_resolution_clock::now(), MSTIMER = TIMER;

    const auto  SPECIAL = DRAGGINGWINDOW->onSpecialWorkspace();

    const auto  DELTA     = Vector2D(mousePos.x - m_vBeginDragXY.x, mousePos.y - m_vBeginDragXY.y);
    const auto  TICKDELTA = Vector2D(mousePos.x - m_vLastDragXY.x, mousePos.y - m_vLastDragXY.y);

    static auto PANIMATEMOUSE = CConfigValue<Hyprlang::INT>("misc:animate_mouse_windowdragging");
    static auto PANIMATE      = CConfigValue<Hyprlang::INT>("misc:animate_manual_resizes");

    static auto SNAPENABLED = CConfigValue<Hyprlang::INT>("general:snap:enabled");

    const auto  TIMERDELTA    = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - TIMER).count();
    const auto  MSDELTA       = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - MSTIMER).count();
    const auto  MSMONITOR     = 1000.0 / g_pHyprRenderer->m_pMostHzMonitor->refreshRate;
    static int  totalMs       = 0;
    bool        canSkipUpdate = true;

    MSTIMER = std::chrono::high_resolution_clock::now();

    if (m_iMouseMoveEventCount == 1)
        totalMs = 0;

    if (MSMONITOR > 16.0) {
        totalMs += MSDELTA < MSMONITOR ? MSDELTA : std::round(totalMs * 1.0 / m_iMouseMoveEventCount);
        m_iMouseMoveEventCount += 1;

        // check if time-window is enough to skip update on 60hz monitor
        canSkipUpdate = std::clamp(MSMONITOR - TIMERDELTA, 0.0, MSMONITOR) > totalMs * 1.0 / m_iMouseMoveEventCount;
    }

    if ((abs(TICKDELTA.x) < 1.f && abs(TICKDELTA.y) < 1.f) || (TIMERDELTA < MSMONITOR && canSkipUpdate && (g_pInputManager->dragMode != MBIND_MOVE || *PANIMATEMOUSE)))
        return;

    TIMER = std::chrono::high_resolution_clock::now();

    m_vLastDragXY = mousePos;

    g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);

    if (g_pInputManager->dragMode == MBIND_MOVE) {

        Vector2D newPos  = m_vBeginDragPositionXY + DELTA;
        Vector2D newSize = DRAGGINGWINDOW->m_vRealSize->goal();

        if (*SNAPENABLED && !DRAGGINGWINDOW->m_bDraggingTiled)
            performSnap(newPos, newSize, DRAGGINGWINDOW, MBIND_MOVE, -1, m_vBeginDragSizeXY);

        CBox wb = {newPos, newSize};
        wb.round();

        if (*PANIMATEMOUSE)
            *DRAGGINGWINDOW->m_vRealPosition = wb.pos();
        else {
            DRAGGINGWINDOW->m_vRealPosition->setValueAndWarp(wb.pos());
            DRAGGINGWINDOW->sendWindowSize();
        }

    } else if (g_pInputManager->dragMode == MBIND_RESIZE || g_pInputManager->dragMode == MBIND_RESIZE_FORCE_RATIO || g_pInputManager->dragMode == MBIND_RESIZE_BLOCK_RATIO) {
        if (DRAGGINGWINDOW->m_bIsFloating) {

            Vector2D MINSIZE = DRAGGINGWINDOW->requestedMinSize().clamp(DRAGGINGWINDOW->m_sWindowData.minSize.valueOr(Vector2D(20, 20)));
            Vector2D MAXSIZE;
            if (DRAGGINGWINDOW->m_sWindowData.maxSize.hasValue())
                MAXSIZE = DRAGGINGWINDOW->requestedMaxSize().clamp({}, DRAGGINGWINDOW->m_sWindowData.maxSize.value());
            else
                MAXSIZE = DRAGGINGWINDOW->requestedMaxSize().clamp({}, Vector2D(std::numeric_limits<double>::max(), std::numeric_limits<double>::max()));

            Vector2D newSize = m_vBeginDragSizeXY;
            Vector2D newPos  = m_vBeginDragPositionXY;

            if (m_eGrabbedCorner == CORNER_BOTTOMRIGHT)
                newSize = newSize + DELTA;
            else if (m_eGrabbedCorner == CORNER_TOPLEFT)
                newSize = newSize - DELTA;
            else if (m_eGrabbedCorner == CORNER_TOPRIGHT)
                newSize = newSize + Vector2D(DELTA.x, -DELTA.y);
            else if (m_eGrabbedCorner == CORNER_BOTTOMLEFT)
                newSize = newSize + Vector2D(-DELTA.x, DELTA.y);

            eMouseBindMode mode = g_pInputManager->dragMode;
            if (DRAGGINGWINDOW->m_sWindowData.keepAspectRatio.valueOrDefault() && mode != MBIND_RESIZE_BLOCK_RATIO)
                mode = MBIND_RESIZE_FORCE_RATIO;

            if (m_vBeginDragSizeXY.x >= 1 && m_vBeginDragSizeXY.y >= 1 && mode == MBIND_RESIZE_FORCE_RATIO) {

                const float RATIO = m_vBeginDragSizeXY.y / m_vBeginDragSizeXY.x;

                if (MINSIZE.x * RATIO > MINSIZE.y)
                    MINSIZE = Vector2D(MINSIZE.x, MINSIZE.x * RATIO);
                else
                    MINSIZE = Vector2D(MINSIZE.y / RATIO, MINSIZE.y);

                if (MAXSIZE.x * RATIO < MAXSIZE.y)
                    MAXSIZE = Vector2D(MAXSIZE.x, MAXSIZE.x * RATIO);
                else
                    MAXSIZE = Vector2D(MAXSIZE.y / RATIO, MAXSIZE.y);

                if (newSize.x * RATIO > newSize.y)
                    newSize = Vector2D(newSize.x, newSize.x * RATIO);
                else
                    newSize = Vector2D(newSize.y / RATIO, newSize.y);
            }

            newSize = newSize.clamp(MINSIZE, MAXSIZE);

            if (m_eGrabbedCorner == CORNER_TOPLEFT)
                newPos = newPos - newSize + m_vBeginDragSizeXY;
            else if (m_eGrabbedCorner == CORNER_TOPRIGHT)
                newPos = newPos + Vector2D(0.0, (m_vBeginDragSizeXY - newSize).y);
            else if (m_eGrabbedCorner == CORNER_BOTTOMLEFT)
                newPos = newPos + Vector2D((m_vBeginDragSizeXY - newSize).x, 0.0);

            if (*SNAPENABLED) {
                performSnap(newPos, newSize, DRAGGINGWINDOW, mode, m_eGrabbedCorner, m_vBeginDragSizeXY);
                newSize = newSize.clamp(MINSIZE, MAXSIZE);
            }

            CBox wb = {newPos, newSize};
            wb.round();

            if (*PANIMATE) {
                *DRAGGINGWINDOW->m_vRealSize     = wb.size();
                *DRAGGINGWINDOW->m_vRealPosition = wb.pos();
            } else {
                DRAGGINGWINDOW->m_vRealSize->setValueAndWarp(wb.size());
                DRAGGINGWINDOW->m_vRealPosition->setValueAndWarp(wb.pos());
                DRAGGINGWINDOW->sendWindowSize();
            }
        } else {
            resizeActiveWindow(TICKDELTA, m_eGrabbedCorner, DRAGGINGWINDOW);
        }
    }

    // get middle point
    Vector2D middle = DRAGGINGWINDOW->m_vRealPosition->value() + DRAGGINGWINDOW->m_vRealSize->value() / 2.f;

    // and check its monitor
    const auto PMONITOR = g_pCompositor->getMonitorFromVector(middle);

    if (PMONITOR && !SPECIAL) {
        DRAGGINGWINDOW->m_pMonitor = PMONITOR;
        DRAGGINGWINDOW->moveToWorkspace(PMONITOR->activeWorkspace);
        DRAGGINGWINDOW->updateGroupOutputs();

        DRAGGINGWINDOW->updateToplevel();
    }

    DRAGGINGWINDOW->updateWindowDecos();

    g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);
}

void CIHyprLayout::changeWindowFloatingMode(PHLWINDOW pWindow) {

    if (pWindow->isFullscreen()) {
        NDebug::log(LOG, "changeWindowFloatingMode: fullscreen");
        g_pCompositor->setWindowFullscreenInternal(pWindow, FSMODE_NONE);
    }

    pWindow->m_bPinned = false;

    g_pHyprRenderer->damageWindow(pWindow, true);

    const auto TILED = isWindowTiled(pWindow);

    // event
    g_pEventManager->postEvent(SHyprIPCEvent{"changefloatingmode", std::format("{:x},{}", (uintptr_t)pWindow.get(), (int)TILED)});
    EMIT_HOOK_EVENT("changeFloatingMode", pWindow);

    if (!TILED) {
        const auto PNEWMON  = g_pCompositor->getMonitorFromVector(pWindow->m_vRealPosition->value() + pWindow->m_vRealSize->value() / 2.f);
        pWindow->m_pMonitor = PNEWMON;
        pWindow->moveToWorkspace(PNEWMON->activeSpecialWorkspace ? PNEWMON->activeSpecialWorkspace : PNEWMON->activeWorkspace);
        pWindow->updateGroupOutputs();

        const auto PWORKSPACE = PNEWMON->activeSpecialWorkspace ? PNEWMON->activeSpecialWorkspace : PNEWMON->activeWorkspace;

        if (PWORKSPACE->m_bHasFullscreenWindow)
            g_pCompositor->setWindowFullscreenInternal(PWORKSPACE->getFullscreenWindow(), FSMODE_NONE);

        // save real pos cuz the func applies the default 5,5 mid
        const auto PSAVEDPOS  = pWindow->m_vRealPosition->goal();
        const auto PSAVEDSIZE = pWindow->m_vRealSize->goal();

        // if the window is pseudo, update its size
        if (!pWindow->m_bDraggingTiled)
            pWindow->m_vPseudoSize = pWindow->m_vRealSize->goal();

        pWindow->m_vLastFloatingSize = PSAVEDSIZE;

        // move to narnia because we don't wanna find our own node. onWindowCreatedTiling should apply the coords back.
        pWindow->m_vPosition = Vector2D(-999999, -999999);

        onWindowCreatedTiling(pWindow);

        pWindow->m_vRealPosition->setValue(PSAVEDPOS);
        pWindow->m_vRealSize->setValue(PSAVEDSIZE);

        // fix pseudo leaving artifacts
        g_pHyprRenderer->damageMonitor(pWindow->m_pMonitor.lock());

        if (pWindow == g_pCompositor->m_pLastWindow)
            m_pLastTiledWindow = pWindow;
    } else {
        onWindowRemovedTiling(pWindow);

        g_pCompositor->changeWindowZOrder(pWindow, true);

        CBox wb = {pWindow->m_vRealPosition->goal() + (pWindow->m_vRealSize->goal() - pWindow->m_vLastFloatingSize) / 2.f, pWindow->m_vLastFloatingSize};
        wb.round();

        if (!(pWindow->m_bIsFloating && pWindow->m_bIsPseudotiled) && DELTALESSTHAN(pWindow->m_vRealSize->value().x, pWindow->m_vLastFloatingSize.x, 10) &&
            DELTALESSTHAN(pWindow->m_vRealSize->value().y, pWindow->m_vLastFloatingSize.y, 10)) {
            wb = {wb.pos() + Vector2D{10, 10}, wb.size() - Vector2D{20, 20}};
        }

        *pWindow->m_vRealPosition = wb.pos();
        *pWindow->m_vRealSize     = wb.size();

        pWindow->m_vSize     = wb.pos();
        pWindow->m_vPosition = wb.size();

        g_pHyprRenderer->damageMonitor(pWindow->m_pMonitor.lock());

        pWindow->unsetWindowData(PRIORITY_LAYOUT);
        pWindow->updateWindowData();

        if (pWindow == m_pLastTiledWindow)
            m_pLastTiledWindow.reset();
    }

    g_pCompositor->updateWindowAnimatedDecorationValues(pWindow);
    pWindow->updateToplevel();
    g_pHyprRenderer->damageWindow(pWindow);
}

void CIHyprLayout::moveActiveWindow(const Vector2D& delta, PHLWINDOW pWindow) {
    const auto PWINDOW = pWindow ? pWindow : g_pCompositor->m_pLastWindow.lock();

    if (!validMapped(PWINDOW))
        return;

    if (!PWINDOW->m_bIsFloating) {
        NDebug::log(LOG, "Dwindle cannot move a tiled window in moveActiveWindow!");
        return;
    }

    PWINDOW->setAnimationsToMove();

    *PWINDOW->m_vRealPosition = PWINDOW->m_vRealPosition->goal() + delta;

    g_pHyprRenderer->damageWindow(PWINDOW);
}

void CIHyprLayout::onWindowFocusChange(PHLWINDOW pNewFocus) {
    m_pLastTiledWindow = pNewFocus && !pNewFocus->m_bIsFloating ? pNewFocus : m_pLastTiledWindow;
}

PHLWINDOW CIHyprLayout::getNextWindowCandidate(PHLWINDOW pWindow) {
    // although we don't expect nullptrs here, let's verify jic
    if (!pWindow)
        return nullptr;

    const auto PWORKSPACE = pWindow->m_pWorkspace;

    // first of all, if this is a fullscreen workspace,
    if (PWORKSPACE->m_bHasFullscreenWindow)
        return PWORKSPACE->getFullscreenWindow();

    if (pWindow->m_bIsFloating) {

        // find whether there is a floating window below this one
        for (auto const& w : g_pCompositor->m_vWindows) {
            if (w->m_bIsMapped && !w->isHidden() && w->m_bIsFloating && !w->isX11OverrideRedirect() && w->m_pWorkspace == pWindow->m_pWorkspace && !w->m_bX11ShouldntFocus &&
                !w->m_sWindowData.noFocus.valueOrDefault() && w != pWindow) {
                if (VECINRECT((pWindow->m_vSize / 2.f + pWindow->m_vPosition), w->m_vPosition.x, w->m_vPosition.y, w->m_vPosition.x + w->m_vSize.x,
                              w->m_vPosition.y + w->m_vSize.y)) {
                    return w;
                }
            }
        }

        // let's try the last tiled window.
        if (m_pLastTiledWindow.lock() && m_pLastTiledWindow->m_pWorkspace == pWindow->m_pWorkspace)
            return m_pLastTiledWindow.lock();

        // if we don't, let's try to find any window that is in the middle
        if (const auto PWINDOWCANDIDATE = g_pCompositor->vectorToWindowUnified(pWindow->middle(), RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);
            PWINDOWCANDIDATE && PWINDOWCANDIDATE != pWindow)
            return PWINDOWCANDIDATE;

        // if not, floating window
        for (auto const& w : g_pCompositor->m_vWindows) {
            if (w->m_bIsMapped && !w->isHidden() && w->m_bIsFloating && !w->isX11OverrideRedirect() && w->m_pWorkspace == pWindow->m_pWorkspace && !w->m_bX11ShouldntFocus &&
                !w->m_sWindowData.noFocus.valueOrDefault() && w != pWindow)
                return w;
        }

        // if there is no candidate, too bad
        return nullptr;
    }

    // if it was a tiled window, we first try to find the window that will replace it.
    auto pWindowCandidate = g_pCompositor->vectorToWindowUnified(pWindow->middle(), RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

    if (!pWindowCandidate)
        pWindowCandidate = PWORKSPACE->getTopLeftWindow();

    if (!pWindowCandidate)
        pWindowCandidate = PWORKSPACE->getFirstWindow();

    if (!pWindowCandidate || pWindow == pWindowCandidate || !pWindowCandidate->m_bIsMapped || pWindowCandidate->isHidden() || pWindowCandidate->m_bX11ShouldntFocus ||
        pWindowCandidate->isX11OverrideRedirect() || pWindowCandidate->m_pMonitor != g_pCompositor->m_pLastMonitor)
        return nullptr;

    return pWindowCandidate;
}

bool CIHyprLayout::isWindowReachable(PHLWINDOW pWindow) {
    return pWindow && (!pWindow->isHidden() || pWindow->m_sGroupData.pNextWindow);
}

void CIHyprLayout::bringWindowToTop(PHLWINDOW pWindow) {
    if (pWindow == nullptr)
        return;

    if (pWindow->isHidden() && pWindow->m_sGroupData.pNextWindow) {
        // grouped, change the current to this window
        pWindow->setGroupCurrent(pWindow);
    }
}

void CIHyprLayout::requestFocusForWindow(PHLWINDOW pWindow) {
    bringWindowToTop(pWindow);
    g_pCompositor->focusWindow(pWindow);
    g_pCompositor->warpCursorTo(pWindow->middle());
}

Vector2D CIHyprLayout::predictSizeForNewWindowFloating(PHLWINDOW pWindow) { // get all rules, see if we have any size overrides.
    Vector2D sizeOverride = {};
    if (g_pCompositor->m_pLastMonitor) {
        for (auto const& r : g_pConfigManager->getMatchingRules(pWindow, true, true)) {
            if (r->m_ruleType != CWindowRule::RULE_SIZE)
                continue;

            try {
                const auto  VALUE    = r->szRule.substr(r->szRule.find(' ') + 1);
                const auto  SIZEXSTR = VALUE.substr(0, VALUE.find(' '));
                const auto  SIZEYSTR = VALUE.substr(VALUE.find(' ') + 1);

                const auto  MAXSIZE = pWindow->requestedMaxSize();

                const float SIZEX = SIZEXSTR == "max" ? std::clamp(MAXSIZE.x, MIN_WINDOW_SIZE, g_pCompositor->m_pLastMonitor->vecSize.x) :
                                                        stringToPercentage(SIZEXSTR, g_pCompositor->m_pLastMonitor->vecSize.x);

                const float SIZEY = SIZEYSTR == "max" ? std::clamp(MAXSIZE.y, MIN_WINDOW_SIZE, g_pCompositor->m_pLastMonitor->vecSize.y) :
                                                        stringToPercentage(SIZEYSTR, g_pCompositor->m_pLastMonitor->vecSize.y);

                sizeOverride = {SIZEX, SIZEY};

            } catch (...) { NDebug::log(LOG, "Rule size failed, rule: {} -> {}", r->szRule, r->szValue); }
            break;
        }
    }

    return sizeOverride;
}

Vector2D CIHyprLayout::predictSizeForNewWindow(PHLWINDOW pWindow) {
    bool shouldBeFloated = g_pXWaylandManager->shouldBeFloated(pWindow, true);

    if (!shouldBeFloated) {
        for (auto const& r : g_pConfigManager->getMatchingRules(pWindow, true, true)) {
            if (r->m_ruleType != CWindowRule::RULE_FLOAT)
                continue;

            shouldBeFloated = true;
            break;
        }
    }

    Vector2D sizePredicted = {};

    if (!shouldBeFloated)
        sizePredicted = predictSizeForNewWindowTiled();
    else
        sizePredicted = predictSizeForNewWindowFloating(pWindow);

    Vector2D maxSize = pWindow->m_pXDGSurface->toplevel->pending.maxSize;

    if ((maxSize.x > 0 && maxSize.x < sizePredicted.x) || (maxSize.y > 0 && maxSize.y < sizePredicted.y))
        sizePredicted = {};

    return sizePredicted;
}
