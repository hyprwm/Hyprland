#include "IHyprLayout.hpp"
#include "../defines.hpp"
#include "../Compositor.hpp"
#include "../render/decorations/CHyprGroupBarDecoration.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "../desktop/Window.hpp"
#include "../protocols/XDGShell.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../xwayland/XSurface.hpp"
#include "../render/Renderer.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/LayoutManager.hpp"
#include "../managers/EventManager.hpp"
#include "../managers/HookSystemManager.hpp"

void IHyprLayout::onWindowCreated(PHLWINDOW pWindow, eDirection direction) {
    CBox       desiredGeometry = g_pXWaylandManager->getGeometryForWindow(pWindow);

    const bool HASPERSISTENTSIZE = std::ranges::any_of(pWindow->m_matchedRules, [](const auto& rule) { return rule->m_ruleType == CWindowRule::RULE_PERSISTENTSIZE; });

    const auto STOREDSIZE = HASPERSISTENTSIZE ? g_pConfigManager->getStoredFloatingSize(pWindow) : std::nullopt;

    if (STOREDSIZE.has_value()) {
        Debug::log(LOG, "using stored size {}x{} for new window {}::{}", STOREDSIZE->x, STOREDSIZE->y, pWindow->m_class, pWindow->m_title);
        pWindow->m_lastFloatingSize = STOREDSIZE.value();
    } else if (desiredGeometry.width <= 5 || desiredGeometry.height <= 5) {
        const auto PMONITOR         = pWindow->m_monitor.lock();
        pWindow->m_lastFloatingSize = PMONITOR->m_size / 2.f;
    } else
        pWindow->m_lastFloatingSize = Vector2D(desiredGeometry.width, desiredGeometry.height);

    pWindow->m_pseudoSize = pWindow->m_lastFloatingSize;

    bool autoGrouped = IHyprLayout::onWindowCreatedAutoGroup(pWindow);
    if (autoGrouped)
        return;

    if (pWindow->m_isFloating)
        onWindowCreatedFloating(pWindow);
    else
        onWindowCreatedTiling(pWindow, direction);

    if (!g_pXWaylandManager->shouldBeFloated(pWindow)) // do not apply group rules to child windows
        pWindow->applyGroupRules();
}

void IHyprLayout::onWindowRemoved(PHLWINDOW pWindow) {
    if (pWindow->isFullscreen())
        g_pCompositor->setWindowFullscreenInternal(pWindow, FSMODE_NONE);

    if (!pWindow->m_groupData.pNextWindow.expired()) {
        if (pWindow->m_groupData.pNextWindow.lock() == pWindow)
            pWindow->m_groupData.pNextWindow.reset();
        else {
            // find last window and update
            PHLWINDOW  PWINDOWPREV     = pWindow->getGroupPrevious();
            const auto WINDOWISVISIBLE = pWindow->getGroupCurrent() == pWindow;

            if (WINDOWISVISIBLE)
                PWINDOWPREV->setGroupCurrent(pWindow->m_groupData.head ? pWindow->m_groupData.pNextWindow.lock() : PWINDOWPREV);

            PWINDOWPREV->m_groupData.pNextWindow = pWindow->m_groupData.pNextWindow;

            pWindow->m_groupData.pNextWindow.reset();

            if (pWindow->m_groupData.head) {
                std::swap(PWINDOWPREV->m_groupData.pNextWindow->m_groupData.head, pWindow->m_groupData.head);
                std::swap(PWINDOWPREV->m_groupData.pNextWindow->m_groupData.locked, pWindow->m_groupData.locked);
            }

            if (pWindow == m_lastTiledWindow)
                m_lastTiledWindow.reset();

            pWindow->setHidden(false);

            pWindow->updateWindowDecos();
            PWINDOWPREV->getGroupCurrent()->updateWindowDecos();
            g_pCompositor->updateWindowAnimatedDecorationValues(pWindow);

            return;
        }
    }

    if (pWindow->m_isFloating) {
        onWindowRemovedFloating(pWindow);
    } else {
        onWindowRemovedTiling(pWindow);
    }

    if (pWindow == m_lastTiledWindow)
        m_lastTiledWindow.reset();
}

void IHyprLayout::onWindowRemovedFloating(PHLWINDOW pWindow) {
    ; // no-op
}

void IHyprLayout::onWindowCreatedFloating(PHLWINDOW pWindow) {

    CBox       desiredGeometry = g_pXWaylandManager->getGeometryForWindow(pWindow);
    const auto PMONITOR        = pWindow->m_monitor.lock();

    if (pWindow->m_isX11) {
        Vector2D xy       = {desiredGeometry.x, desiredGeometry.y};
        xy                = g_pXWaylandManager->xwaylandToWaylandCoords(xy);
        desiredGeometry.x = xy.x;
        desiredGeometry.y = xy.y;
    }

    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    if (!PMONITOR) {
        Debug::log(ERR, "{:m} has an invalid monitor in onWindowCreatedFloating!!!", pWindow);
        return;
    }

    if (desiredGeometry.width <= 5 || desiredGeometry.height <= 5) {
        const auto PWINDOWSURFACE = pWindow->m_wlSurface->resource();
        *pWindow->m_realSize      = PWINDOWSURFACE->m_current.size;

        if ((desiredGeometry.width <= 1 || desiredGeometry.height <= 1) && pWindow->m_isX11 &&
            pWindow->isX11OverrideRedirect()) { // XDG windows should be fine. TODO: check for weird atoms?
            pWindow->setHidden(true);
            return;
        }

        // reject any windows with size <= 5x5
        if (pWindow->m_realSize->goal().x <= 5 || pWindow->m_realSize->goal().y <= 5)
            *pWindow->m_realSize = PMONITOR->m_size / 2.f;

        if (pWindow->m_isX11 && pWindow->isX11OverrideRedirect()) {

            if (pWindow->m_xwaylandSurface->m_geometry.x != 0 && pWindow->m_xwaylandSurface->m_geometry.y != 0)
                *pWindow->m_realPosition = g_pXWaylandManager->xwaylandToWaylandCoords(pWindow->m_xwaylandSurface->m_geometry.pos());
            else
                *pWindow->m_realPosition = Vector2D(PMONITOR->m_position.x + (PMONITOR->m_size.x - pWindow->m_realSize->goal().x) / 2.f,
                                                    PMONITOR->m_position.y + (PMONITOR->m_size.y - pWindow->m_realSize->goal().y) / 2.f);
        } else {
            *pWindow->m_realPosition = Vector2D(PMONITOR->m_position.x + (PMONITOR->m_size.x - pWindow->m_realSize->goal().x) / 2.f,
                                                PMONITOR->m_position.y + (PMONITOR->m_size.y - pWindow->m_realSize->goal().y) / 2.f);
        }
    } else {
        // we respect the size.
        *pWindow->m_realSize = Vector2D(desiredGeometry.width, desiredGeometry.height);

        // check if it's on the correct monitor!
        Vector2D middlePoint = Vector2D(desiredGeometry.x, desiredGeometry.y) + Vector2D(desiredGeometry.width, desiredGeometry.height) / 2.f;

        // check if it's visible on any monitor (only for XDG)
        bool visible = pWindow->m_isX11;

        if (!visible) {
            visible = g_pCompositor->isPointOnAnyMonitor(Vector2D(desiredGeometry.x, desiredGeometry.y)) &&
                g_pCompositor->isPointOnAnyMonitor(Vector2D(desiredGeometry.x + desiredGeometry.width, desiredGeometry.y)) &&
                g_pCompositor->isPointOnAnyMonitor(Vector2D(desiredGeometry.x, desiredGeometry.y + desiredGeometry.height)) &&
                g_pCompositor->isPointOnAnyMonitor(Vector2D(desiredGeometry.x + desiredGeometry.width, desiredGeometry.y + desiredGeometry.height));
        }

        // TODO: detect a popup in a more consistent way.
        bool centeredOnParent = false;
        if ((desiredGeometry.x == 0 && desiredGeometry.y == 0) || !visible || !pWindow->m_isX11) {
            // if the pos isn't set, fall back to the center placement if it's not a child
            auto pos = PMONITOR->m_position + PMONITOR->m_size / 2.F - desiredGeometry.size() / 2.F;

            // otherwise middle of parent if available
            if (!pWindow->m_isX11) {
                if (const auto PARENT = pWindow->parent(); PARENT) {
                    *pWindow->m_realPosition = PARENT->m_realPosition->goal() + PARENT->m_realSize->goal() / 2.F - desiredGeometry.size() / 2.F;
                    pWindow->m_workspace     = PARENT->m_workspace;
                    pWindow->m_monitor       = PARENT->m_monitor;
                    centeredOnParent         = true;
                }
            }
            if (!centeredOnParent)
                *pWindow->m_realPosition = pos;
        } else {
            // if it is, we respect where it wants to put itself, but apply monitor offset if outside
            // most of these are popups

            if (const auto POPENMON = g_pCompositor->getMonitorFromVector(middlePoint); POPENMON->m_id != PMONITOR->m_id)
                *pWindow->m_realPosition = Vector2D(desiredGeometry.x, desiredGeometry.y) - POPENMON->m_position + PMONITOR->m_position;
            else
                *pWindow->m_realPosition = Vector2D(desiredGeometry.x, desiredGeometry.y);
        }
    }

    if (*PXWLFORCESCALEZERO && pWindow->m_isX11)
        *pWindow->m_realSize = pWindow->m_realSize->goal() / PMONITOR->m_scale;

    if (pWindow->m_X11DoesntWantBorders || (pWindow->m_isX11 && pWindow->isX11OverrideRedirect())) {
        pWindow->m_realPosition->warp();
        pWindow->m_realSize->warp();
    }

    if (!pWindow->isX11OverrideRedirect())
        g_pCompositor->changeWindowZOrder(pWindow, true);
    else {
        pWindow->m_pendingReportedSize = pWindow->m_realSize->goal();
        pWindow->m_reportedSize        = pWindow->m_pendingReportedSize;
    }
}

bool IHyprLayout::onWindowCreatedAutoGroup(PHLWINDOW pWindow) {
    static auto     PAUTOGROUP       = CConfigValue<Hyprlang::INT>("group:auto_group");
    const PHLWINDOW OPENINGON        = g_pCompositor->m_lastWindow.lock() && g_pCompositor->m_lastWindow->m_workspace == pWindow->m_workspace ?
               g_pCompositor->m_lastWindow.lock() :
               (pWindow->m_workspace ? pWindow->m_workspace->getFirstWindow() : nullptr);
    const bool      FLOATEDINTOTILED = pWindow->m_isFloating && !OPENINGON->m_isFloating;
    const bool      SWALLOWING       = pWindow->m_swallowed || pWindow->m_groupSwallowed;

    if ((*PAUTOGROUP || SWALLOWING)                      // continue if auto_group is enabled or if dealing with window swallowing.
        && OPENINGON                                     // this shouldn't be 0, but honestly, better safe than sorry.
        && OPENINGON != pWindow                          // prevent freeze when the "group set" window rule makes the new window to be already a group.
        && OPENINGON->m_groupData.pNextWindow.lock()     // check if OPENINGON is a group.
        && pWindow->canBeGroupedInto(OPENINGON)          // check if the new window can be grouped into OPENINGON.
        && !g_pXWaylandManager->shouldBeFloated(pWindow) // don't group child windows. Fix for floated groups. Tiled groups don't need this because we check if !FLOATEDINTOTILED.
        && !FLOATEDINTOTILED) {                          // don't group a new floated window into a tiled group (for convenience).

        pWindow->m_isFloating = OPENINGON->m_isFloating; // match the floating state. Needed to autogroup a new tiled window into a floated group.

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

void IHyprLayout::onBeginDragWindow() {
    const auto  DRAGGINGWINDOW = g_pInputManager->m_currentlyDraggedWindow.lock();
    static auto PDRAGTHRESHOLD = CConfigValue<Hyprlang::INT>("binds:drag_threshold");

    m_mouseMoveEventCount = 1;
    m_beginDragSizeXY     = Vector2D();

    // Window will be floating. Let's check if it's valid. It should be, but I don't like crashing.
    if (!validMapped(DRAGGINGWINDOW)) {
        Debug::log(ERR, "Dragging attempted on an invalid window!");
        g_pKeybindManager->changeMouseBindMode(MBIND_INVALID);
        return;
    }

    // Try to pick up dragged window now if drag_threshold is disabled
    // or at least update dragging related variables for the cursors
    g_pInputManager->m_dragThresholdReached = *PDRAGTHRESHOLD <= 0;
    if (updateDragWindow())
        return;

    // get the grab corner
    static auto RESIZECORNER = CConfigValue<Hyprlang::INT>("general:resize_corner");
    if (*RESIZECORNER != 0 && *RESIZECORNER <= 4 && DRAGGINGWINDOW->m_isFloating) {
        switch (*RESIZECORNER) {
            case 1:
                m_grabbedCorner = CORNER_TOPLEFT;
                g_pInputManager->setCursorImageUntilUnset("nw-resize");
                break;
            case 2:
                m_grabbedCorner = CORNER_TOPRIGHT;
                g_pInputManager->setCursorImageUntilUnset("ne-resize");
                break;
            case 3:
                m_grabbedCorner = CORNER_BOTTOMRIGHT;
                g_pInputManager->setCursorImageUntilUnset("se-resize");
                break;
            case 4:
                m_grabbedCorner = CORNER_BOTTOMLEFT;
                g_pInputManager->setCursorImageUntilUnset("sw-resize");
                break;
        }
    } else if (m_beginDragXY.x < m_beginDragPositionXY.x + m_beginDragSizeXY.x / 2.0) {
        if (m_beginDragXY.y < m_beginDragPositionXY.y + m_beginDragSizeXY.y / 2.0) {
            m_grabbedCorner = CORNER_TOPLEFT;
            g_pInputManager->setCursorImageUntilUnset("nw-resize");
        } else {
            m_grabbedCorner = CORNER_BOTTOMLEFT;
            g_pInputManager->setCursorImageUntilUnset("sw-resize");
        }
    } else {
        if (m_beginDragXY.y < m_beginDragPositionXY.y + m_beginDragSizeXY.y / 2.0) {
            m_grabbedCorner = CORNER_TOPRIGHT;
            g_pInputManager->setCursorImageUntilUnset("ne-resize");
        } else {
            m_grabbedCorner = CORNER_BOTTOMRIGHT;
            g_pInputManager->setCursorImageUntilUnset("se-resize");
        }
    }

    if (g_pInputManager->m_dragMode != MBIND_RESIZE && g_pInputManager->m_dragMode != MBIND_RESIZE_FORCE_RATIO && g_pInputManager->m_dragMode != MBIND_RESIZE_BLOCK_RATIO)
        g_pInputManager->setCursorImageUntilUnset("grabbing");

    g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);

    g_pKeybindManager->shadowKeybinds();

    g_pCompositor->focusWindow(DRAGGINGWINDOW);
    g_pCompositor->changeWindowZOrder(DRAGGINGWINDOW, true);
}

void IHyprLayout::onEndDragWindow() {
    const auto DRAGGINGWINDOW = g_pInputManager->m_currentlyDraggedWindow.lock();

    m_mouseMoveEventCount = 1;

    if (!validMapped(DRAGGINGWINDOW)) {
        if (DRAGGINGWINDOW) {
            g_pInputManager->unsetCursorImage();
            g_pInputManager->m_currentlyDraggedWindow.reset();
        }
        return;
    }

    g_pInputManager->unsetCursorImage();
    g_pInputManager->m_currentlyDraggedWindow.reset();
    g_pInputManager->m_wasDraggingWindow = true;

    if (g_pInputManager->m_dragMode == MBIND_MOVE) {
        g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);
        const auto MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();
        PHLWINDOW  pWindow     = g_pCompositor->vectorToWindowUnified(MOUSECOORDS, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING, DRAGGINGWINDOW);

        if (pWindow) {
            if (pWindow->checkInputOnDecos(INPUT_TYPE_DRAG_END, MOUSECOORDS, DRAGGINGWINDOW))
                return;

            const bool  FLOATEDINTOTILED = !pWindow->m_isFloating && !DRAGGINGWINDOW->m_draggingTiled;
            static auto PDRAGINTOGROUP   = CConfigValue<Hyprlang::INT>("group:drag_into_group");

            if (pWindow->m_groupData.pNextWindow.lock() && DRAGGINGWINDOW->canBeGroupedInto(pWindow) && *PDRAGINTOGROUP == 1 && !FLOATEDINTOTILED) {

                if (DRAGGINGWINDOW->m_groupData.pNextWindow) {
                    PHLWINDOW next = DRAGGINGWINDOW->m_groupData.pNextWindow.lock();
                    while (next != DRAGGINGWINDOW) {
                        next->m_isFloating    = pWindow->m_isFloating;           // match the floating state of group members
                        *next->m_realSize     = pWindow->m_realSize->goal();     // match the size of group members
                        *next->m_realPosition = pWindow->m_realPosition->goal(); // match the position of group members
                        next                  = next->m_groupData.pNextWindow.lock();
                    }
                }

                DRAGGINGWINDOW->m_isFloating       = pWindow->m_isFloating; // match the floating state of the window
                DRAGGINGWINDOW->m_lastFloatingSize = m_draggingWindowOriginalFloatSize;
                DRAGGINGWINDOW->m_draggingTiled    = false;

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

    if (DRAGGINGWINDOW->m_draggingTiled) {
        static auto PPRECISEMOUSE    = CConfigValue<Hyprlang::INT>("dwindle:precise_mouse_move");
        DRAGGINGWINDOW->m_isFloating = false;
        g_pInputManager->refocus();

        if (*PPRECISEMOUSE) {
            eDirection      direction = DIRECTION_DEFAULT;

            const auto      MOUSECOORDS      = g_pInputManager->getMouseCoordsInternal();
            const PHLWINDOW pReferenceWindow = g_pCompositor->vectorToWindowUnified(MOUSECOORDS, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING, DRAGGINGWINDOW);

            if (pReferenceWindow && pReferenceWindow != DRAGGINGWINDOW) {
                const Vector2D draggedCenter   = DRAGGINGWINDOW->m_realPosition->goal() + DRAGGINGWINDOW->m_realSize->goal() / 2.f;
                const Vector2D referenceCenter = pReferenceWindow->m_realPosition->goal() + pReferenceWindow->m_realSize->goal() / 2.f;
                const float    xDiff           = draggedCenter.x - referenceCenter.x;
                const float    yDiff           = draggedCenter.y - referenceCenter.y;

                if (fabs(xDiff) > fabs(yDiff))
                    direction = xDiff < 0 ? DIRECTION_LEFT : DIRECTION_RIGHT;
                else
                    direction = yDiff < 0 ? DIRECTION_UP : DIRECTION_DOWN;
            }

            onWindowRemovedTiling(DRAGGINGWINDOW);
            onWindowCreatedTiling(DRAGGINGWINDOW, direction);
        } else
            changeWindowFloatingMode(DRAGGINGWINDOW);

        DRAGGINGWINDOW->m_lastFloatingSize = m_draggingWindowOriginalFloatSize;
    }

    g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);
    g_pCompositor->focusWindow(DRAGGINGWINDOW);

    g_pInputManager->m_wasDraggingWindow = false;
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

using SnapFn = std::function<void(double&, double&, const double)>;

static void performSnap(Vector2D& sourcePos, Vector2D& sourceSize, PHLWINDOW DRAGGINGWINDOW, const eMouseBindMode MODE, const int CORNER, const Vector2D& BEGINSIZE) {
    static auto  SNAPWINDOWGAP     = CConfigValue<Hyprlang::INT>("general:snap:window_gap");
    static auto  SNAPMONITORGAP    = CConfigValue<Hyprlang::INT>("general:snap:monitor_gap");
    static auto  SNAPBORDEROVERLAP = CConfigValue<Hyprlang::INT>("general:snap:border_overlap");
    static auto  SNAPRESPECTGAPS   = CConfigValue<Hyprlang::INT>("general:snap:respect_gaps");

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
        const bool   HASFULLSCREEN = DRAGGINGWINDOW->m_workspace && DRAGGINGWINDOW->m_workspace->m_hasFullscreenWindow;

        double       gapOffset = 0;
        if (*SNAPRESPECTGAPS) {
            static auto PGAPSINDATA = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_in");
            auto*       PGAPSINPTR  = (CCssGapData*)(PGAPSINDATA.ptr())->getData();
            gapOffset               = std::max({PGAPSINPTR->m_left, PGAPSINPTR->m_right, PGAPSINPTR->m_top, PGAPSINPTR->m_bottom});
        }

        for (auto& other : g_pCompositor->m_windows) {
            if ((HASFULLSCREEN && !other->m_createdOverFullscreen) || other == DRAGGINGWINDOW || other->workspaceID() != WSID || !other->m_isMapped || other->m_fadingOut ||
                other->isX11OverrideRedirect())
                continue;

            const int    OTHERBORDERSIZE = other->getRealBorderSize();
            const double BORDERSIZE      = OVERLAP ? std::max(DRAGGINGBORDERSIZE, OTHERBORDERSIZE) : (DRAGGINGBORDERSIZE + OTHERBORDERSIZE);

            const CBox   SURF   = other->getWindowMainSurfaceBox();
            const SRange SURFBX = {SURF.x - BORDERSIZE - gapOffset, SURF.x + SURF.w + BORDERSIZE + gapOffset};
            const SRange SURFBY = {SURF.y - BORDERSIZE - gapOffset, SURF.y + SURF.h + BORDERSIZE + gapOffset};

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
        const auto   MON        = DRAGGINGWINDOW->m_monitor.lock();
        double       gapOffset  = 0;
        if (*SNAPRESPECTGAPS) {
            static auto PGAPSOUTDATA = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_out");
            auto*       PGAPSOUTPTR  = (CCssGapData*)(PGAPSOUTDATA.ptr())->getData();
            gapOffset                = std::max({PGAPSOUTPTR->m_left, PGAPSOUTPTR->m_right, PGAPSOUTPTR->m_top, PGAPSOUTPTR->m_bottom});
        }

        if (CORNER & (CORNER_TOPLEFT | CORNER_BOTTOMLEFT) &&
            ((MON->m_reservedTopLeft.x > 0 && canSnap(sourceX.start, MON->m_position.x + MON->m_reservedTopLeft.x + DRAGGINGBORDERSIZE + gapOffset, GAPSIZE)) ||
             canSnap(sourceX.start, MON->m_position.x + MON->m_reservedTopLeft.x - BORDERDIFF + gapOffset, GAPSIZE))) {
            SNAP(sourceX.start, sourceX.end, MON->m_position.x + MON->m_reservedTopLeft.x + DRAGGINGBORDERSIZE + gapOffset);
            snaps |= SNAP_LEFT;
        }
        if (CORNER & (CORNER_TOPRIGHT | CORNER_BOTTOMRIGHT) &&
            ((MON->m_reservedBottomRight.x > 0 &&
              canSnap(sourceX.end, MON->m_position.x + MON->m_size.x - MON->m_reservedBottomRight.x - DRAGGINGBORDERSIZE - gapOffset, GAPSIZE)) ||
             canSnap(sourceX.end, MON->m_position.x + MON->m_size.x - MON->m_reservedBottomRight.x + BORDERDIFF - gapOffset, GAPSIZE))) {
            SNAP(sourceX.end, sourceX.start, MON->m_position.x + MON->m_size.x - MON->m_reservedBottomRight.x - DRAGGINGBORDERSIZE - gapOffset);
            snaps |= SNAP_RIGHT;
        }
        if (CORNER & (CORNER_TOPLEFT | CORNER_TOPRIGHT) &&
            ((MON->m_reservedTopLeft.y > 0 && canSnap(sourceY.start, MON->m_position.y + MON->m_reservedTopLeft.y + DRAGGINGBORDERSIZE + gapOffset, GAPSIZE)) ||
             canSnap(sourceY.start, MON->m_position.y + MON->m_reservedTopLeft.y - BORDERDIFF + gapOffset, GAPSIZE))) {
            SNAP(sourceY.start, sourceY.end, MON->m_position.y + MON->m_reservedTopLeft.y + DRAGGINGBORDERSIZE + gapOffset);
            snaps |= SNAP_UP;
        }
        if (CORNER & (CORNER_BOTTOMLEFT | CORNER_BOTTOMRIGHT) &&
            ((MON->m_reservedBottomRight.y > 0 &&
              canSnap(sourceY.end, MON->m_position.y + MON->m_size.y - MON->m_reservedBottomRight.y - DRAGGINGBORDERSIZE - gapOffset, GAPSIZE)) ||
             canSnap(sourceY.end, MON->m_position.y + MON->m_size.y - MON->m_reservedBottomRight.y + BORDERDIFF - gapOffset, GAPSIZE))) {
            SNAP(sourceY.end, sourceY.start, MON->m_position.y + MON->m_size.y - MON->m_reservedBottomRight.y - DRAGGINGBORDERSIZE - gapOffset);
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

void IHyprLayout::onMouseMove(const Vector2D& mousePos) {
    if (g_pInputManager->m_currentlyDraggedWindow.expired())
        return;

    const auto  DRAGGINGWINDOW = g_pInputManager->m_currentlyDraggedWindow.lock();
    static auto PDRAGTHRESHOLD = CConfigValue<Hyprlang::INT>("binds:drag_threshold");

    // Window invalid or drag begin size 0,0 meaning we rejected it.
    if ((!validMapped(DRAGGINGWINDOW) || m_beginDragSizeXY == Vector2D())) {
        g_pKeybindManager->changeMouseBindMode(MBIND_INVALID);
        return;
    }

    // Yoink dragged window here instead if using drag_threshold and it has been reached
    if (*PDRAGTHRESHOLD > 0 && !g_pInputManager->m_dragThresholdReached) {
        if ((m_beginDragXY.distanceSq(mousePos) <= std::pow(*PDRAGTHRESHOLD, 2) && m_beginDragXY == m_lastDragXY))
            return;
        g_pInputManager->m_dragThresholdReached = true;
        if (updateDragWindow())
            return;
    }

    static auto TIMER = std::chrono::high_resolution_clock::now(), MSTIMER = TIMER;

    const auto  SPECIAL = DRAGGINGWINDOW->onSpecialWorkspace();

    const auto  DELTA     = Vector2D(mousePos.x - m_beginDragXY.x, mousePos.y - m_beginDragXY.y);
    const auto  TICKDELTA = Vector2D(mousePos.x - m_lastDragXY.x, mousePos.y - m_lastDragXY.y);

    static auto PANIMATEMOUSE = CConfigValue<Hyprlang::INT>("misc:animate_mouse_windowdragging");
    static auto PANIMATE      = CConfigValue<Hyprlang::INT>("misc:animate_manual_resizes");

    static auto SNAPENABLED = CConfigValue<Hyprlang::INT>("general:snap:enabled");

    const auto  TIMERDELTA    = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - TIMER).count();
    const auto  MSDELTA       = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - MSTIMER).count();
    const auto  MSMONITOR     = 1000.0 / g_pHyprRenderer->m_mostHzMonitor->m_refreshRate;
    static int  totalMs       = 0;
    bool        canSkipUpdate = true;

    MSTIMER = std::chrono::high_resolution_clock::now();

    if (m_mouseMoveEventCount == 1)
        totalMs = 0;

    if (MSMONITOR > 16.0) {
        totalMs += MSDELTA < MSMONITOR ? MSDELTA : std::round(totalMs * 1.0 / m_mouseMoveEventCount);
        m_mouseMoveEventCount += 1;

        // check if time-window is enough to skip update on 60hz monitor
        canSkipUpdate = std::clamp(MSMONITOR - TIMERDELTA, 0.0, MSMONITOR) > totalMs * 1.0 / m_mouseMoveEventCount;
    }

    if ((abs(TICKDELTA.x) < 1.f && abs(TICKDELTA.y) < 1.f) || (TIMERDELTA < MSMONITOR && canSkipUpdate && (g_pInputManager->m_dragMode != MBIND_MOVE || *PANIMATEMOUSE)))
        return;

    TIMER = std::chrono::high_resolution_clock::now();

    m_lastDragXY = mousePos;

    g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);

    if (g_pInputManager->m_dragMode == MBIND_MOVE) {

        Vector2D newPos  = m_beginDragPositionXY + DELTA;
        Vector2D newSize = DRAGGINGWINDOW->m_realSize->goal();

        if (*SNAPENABLED && !DRAGGINGWINDOW->m_draggingTiled)
            performSnap(newPos, newSize, DRAGGINGWINDOW, MBIND_MOVE, -1, m_beginDragSizeXY);

        CBox wb = {newPos, newSize};
        wb.round();

        if (*PANIMATEMOUSE)
            *DRAGGINGWINDOW->m_realPosition = wb.pos();
        else {
            DRAGGINGWINDOW->m_realPosition->setValueAndWarp(wb.pos());
            DRAGGINGWINDOW->sendWindowSize();
        }

        DRAGGINGWINDOW->m_position = wb.pos();

    } else if (g_pInputManager->m_dragMode == MBIND_RESIZE || g_pInputManager->m_dragMode == MBIND_RESIZE_FORCE_RATIO || g_pInputManager->m_dragMode == MBIND_RESIZE_BLOCK_RATIO) {
        if (DRAGGINGWINDOW->m_isFloating) {

            Vector2D MINSIZE = DRAGGINGWINDOW->requestedMinSize().clamp(DRAGGINGWINDOW->m_windowData.minSize.valueOr(Vector2D(MIN_WINDOW_SIZE, MIN_WINDOW_SIZE)));
            Vector2D MAXSIZE;
            if (DRAGGINGWINDOW->m_windowData.maxSize.hasValue())
                MAXSIZE = DRAGGINGWINDOW->requestedMaxSize().clamp({}, DRAGGINGWINDOW->m_windowData.maxSize.value());
            else
                MAXSIZE = DRAGGINGWINDOW->requestedMaxSize().clamp({}, Vector2D(std::numeric_limits<double>::max(), std::numeric_limits<double>::max()));

            Vector2D newSize = m_beginDragSizeXY;
            Vector2D newPos  = m_beginDragPositionXY;

            if (m_grabbedCorner == CORNER_BOTTOMRIGHT)
                newSize = newSize + DELTA;
            else if (m_grabbedCorner == CORNER_TOPLEFT)
                newSize = newSize - DELTA;
            else if (m_grabbedCorner == CORNER_TOPRIGHT)
                newSize = newSize + Vector2D(DELTA.x, -DELTA.y);
            else if (m_grabbedCorner == CORNER_BOTTOMLEFT)
                newSize = newSize + Vector2D(-DELTA.x, DELTA.y);

            eMouseBindMode mode = g_pInputManager->m_dragMode;
            if (DRAGGINGWINDOW->m_windowData.keepAspectRatio.valueOrDefault() && mode != MBIND_RESIZE_BLOCK_RATIO)
                mode = MBIND_RESIZE_FORCE_RATIO;

            if (m_beginDragSizeXY.x >= 1 && m_beginDragSizeXY.y >= 1 && mode == MBIND_RESIZE_FORCE_RATIO) {

                const float RATIO = m_beginDragSizeXY.y / m_beginDragSizeXY.x;

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

            if (m_grabbedCorner == CORNER_TOPLEFT)
                newPos = newPos - newSize + m_beginDragSizeXY;
            else if (m_grabbedCorner == CORNER_TOPRIGHT)
                newPos = newPos + Vector2D(0.0, (m_beginDragSizeXY - newSize).y);
            else if (m_grabbedCorner == CORNER_BOTTOMLEFT)
                newPos = newPos + Vector2D((m_beginDragSizeXY - newSize).x, 0.0);

            if (*SNAPENABLED) {
                performSnap(newPos, newSize, DRAGGINGWINDOW, mode, m_grabbedCorner, m_beginDragSizeXY);
                newSize = newSize.clamp(MINSIZE, MAXSIZE);
            }

            CBox wb = {newPos, newSize};
            wb.round();

            if (*PANIMATE) {
                *DRAGGINGWINDOW->m_realSize     = wb.size();
                *DRAGGINGWINDOW->m_realPosition = wb.pos();
            } else {
                DRAGGINGWINDOW->m_realSize->setValueAndWarp(wb.size());
                DRAGGINGWINDOW->m_realPosition->setValueAndWarp(wb.pos());
                DRAGGINGWINDOW->sendWindowSize();
            }

            DRAGGINGWINDOW->m_position = wb.pos();
            DRAGGINGWINDOW->m_size     = wb.size();
        } else {
            resizeActiveWindow(TICKDELTA, m_grabbedCorner, DRAGGINGWINDOW);
        }
    }

    // get middle point
    Vector2D middle = DRAGGINGWINDOW->m_realPosition->value() + DRAGGINGWINDOW->m_realSize->value() / 2.f;

    // and check its monitor
    const auto PMONITOR = g_pCompositor->getMonitorFromVector(middle);

    if (PMONITOR && !SPECIAL) {
        DRAGGINGWINDOW->m_monitor = PMONITOR;
        DRAGGINGWINDOW->moveToWorkspace(PMONITOR->m_activeWorkspace);
        DRAGGINGWINDOW->updateGroupOutputs();

        DRAGGINGWINDOW->updateToplevel();
    }

    DRAGGINGWINDOW->updateWindowDecos();

    g_pHyprRenderer->damageWindow(DRAGGINGWINDOW);
}

void IHyprLayout::changeWindowFloatingMode(PHLWINDOW pWindow) {

    if (pWindow->isFullscreen()) {
        Debug::log(LOG, "changeWindowFloatingMode: fullscreen");
        g_pCompositor->setWindowFullscreenInternal(pWindow, FSMODE_NONE);
    }

    pWindow->m_pinned = false;

    g_pHyprRenderer->damageWindow(pWindow, true);

    const auto TILED = isWindowTiled(pWindow);

    // event
    g_pEventManager->postEvent(SHyprIPCEvent{"changefloatingmode", std::format("{:x},{}", (uintptr_t)pWindow.get(), (int)TILED)});
    EMIT_HOOK_EVENT("changeFloatingMode", pWindow);

    if (!TILED) {
        const auto PNEWMON = g_pCompositor->getMonitorFromVector(pWindow->m_realPosition->value() + pWindow->m_realSize->value() / 2.f);
        pWindow->m_monitor = PNEWMON;
        pWindow->moveToWorkspace(PNEWMON->m_activeSpecialWorkspace ? PNEWMON->m_activeSpecialWorkspace : PNEWMON->m_activeWorkspace);
        pWindow->updateGroupOutputs();

        const auto PWORKSPACE = PNEWMON->m_activeSpecialWorkspace ? PNEWMON->m_activeSpecialWorkspace : PNEWMON->m_activeWorkspace;

        if (PWORKSPACE->m_hasFullscreenWindow)
            g_pCompositor->setWindowFullscreenInternal(PWORKSPACE->getFullscreenWindow(), FSMODE_NONE);

        // save real pos cuz the func applies the default 5,5 mid
        const auto PSAVEDPOS  = pWindow->m_realPosition->goal();
        const auto PSAVEDSIZE = pWindow->m_realSize->goal();

        // if the window is pseudo, update its size
        if (!pWindow->m_draggingTiled)
            pWindow->m_pseudoSize = pWindow->m_realSize->goal();

        pWindow->m_lastFloatingSize = PSAVEDSIZE;

        // move to narnia because we don't wanna find our own node. onWindowCreatedTiling should apply the coords back.
        pWindow->m_position = Vector2D(-999999, -999999);

        onWindowCreatedTiling(pWindow);

        pWindow->m_realPosition->setValue(PSAVEDPOS);
        pWindow->m_realSize->setValue(PSAVEDSIZE);

        // fix pseudo leaving artifacts
        g_pHyprRenderer->damageMonitor(pWindow->m_monitor.lock());

        if (pWindow == g_pCompositor->m_lastWindow)
            m_lastTiledWindow = pWindow;
    } else {
        onWindowRemovedTiling(pWindow);

        g_pCompositor->changeWindowZOrder(pWindow, true);

        CBox wb = {pWindow->m_realPosition->goal() + (pWindow->m_realSize->goal() - pWindow->m_lastFloatingSize) / 2.f, pWindow->m_lastFloatingSize};
        wb.round();

        if (!(pWindow->m_isFloating && pWindow->m_isPseudotiled) && DELTALESSTHAN(pWindow->m_realSize->value().x, pWindow->m_lastFloatingSize.x, 10) &&
            DELTALESSTHAN(pWindow->m_realSize->value().y, pWindow->m_lastFloatingSize.y, 10)) {
            wb = {wb.pos() + Vector2D{10, 10}, wb.size() - Vector2D{20, 20}};
        }

        *pWindow->m_realPosition = wb.pos();
        *pWindow->m_realSize     = wb.size();

        pWindow->m_size     = wb.size();
        pWindow->m_position = wb.pos();

        g_pHyprRenderer->damageMonitor(pWindow->m_monitor.lock());

        pWindow->unsetWindowData(PRIORITY_LAYOUT);
        pWindow->updateWindowData();

        if (pWindow == m_lastTiledWindow)
            m_lastTiledWindow.reset();
    }

    g_pCompositor->updateWindowAnimatedDecorationValues(pWindow);
    pWindow->updateToplevel();
    g_pHyprRenderer->damageWindow(pWindow);
}

void IHyprLayout::moveActiveWindow(const Vector2D& delta, PHLWINDOW pWindow) {
    const auto PWINDOW = pWindow ? pWindow : g_pCompositor->m_lastWindow.lock();

    if (!validMapped(PWINDOW))
        return;

    if (!PWINDOW->m_isFloating) {
        Debug::log(LOG, "Dwindle cannot move a tiled window in moveActiveWindow!");
        return;
    }

    PWINDOW->setAnimationsToMove();

    PWINDOW->m_position += delta;
    *PWINDOW->m_realPosition = PWINDOW->m_realPosition->goal() + delta;

    g_pHyprRenderer->damageWindow(PWINDOW);
}

void IHyprLayout::onWindowFocusChange(PHLWINDOW pNewFocus) {
    m_lastTiledWindow = pNewFocus && !pNewFocus->m_isFloating ? pNewFocus : m_lastTiledWindow;
}

PHLWINDOW IHyprLayout::getNextWindowCandidate(PHLWINDOW pWindow) {
    // although we don't expect nullptrs here, let's verify jic
    if (!pWindow)
        return nullptr;

    const auto PWORKSPACE = pWindow->m_workspace;

    // first of all, if this is a fullscreen workspace,
    if (PWORKSPACE->m_hasFullscreenWindow)
        return PWORKSPACE->getFullscreenWindow();

    if (pWindow->m_isFloating) {

        // find whether there is a floating window below this one
        for (auto const& w : g_pCompositor->m_windows) {
            if (w->m_isMapped && !w->isHidden() && w->m_isFloating && !w->isX11OverrideRedirect() && w->m_workspace == pWindow->m_workspace && !w->m_X11ShouldntFocus &&
                !w->m_windowData.noFocus.valueOrDefault() && w != pWindow) {
                if (VECINRECT((pWindow->m_size / 2.f + pWindow->m_position), w->m_position.x, w->m_position.y, w->m_position.x + w->m_size.x, w->m_position.y + w->m_size.y)) {
                    return w;
                }
            }
        }

        // let's try the last tiled window.
        if (m_lastTiledWindow.lock() && m_lastTiledWindow->m_workspace == pWindow->m_workspace)
            return m_lastTiledWindow.lock();

        // if we don't, let's try to find any window that is in the middle
        if (const auto PWINDOWCANDIDATE = g_pCompositor->vectorToWindowUnified(pWindow->middle(), RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);
            PWINDOWCANDIDATE && PWINDOWCANDIDATE != pWindow)
            return PWINDOWCANDIDATE;

        // if not, floating window
        for (auto const& w : g_pCompositor->m_windows) {
            if (w->m_isMapped && !w->isHidden() && w->m_isFloating && !w->isX11OverrideRedirect() && w->m_workspace == pWindow->m_workspace && !w->m_X11ShouldntFocus &&
                !w->m_windowData.noFocus.valueOrDefault() && w != pWindow)
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

    if (!pWindowCandidate || pWindow == pWindowCandidate || !pWindowCandidate->m_isMapped || pWindowCandidate->isHidden() || pWindowCandidate->m_X11ShouldntFocus ||
        pWindowCandidate->isX11OverrideRedirect() || pWindowCandidate->m_monitor != g_pCompositor->m_lastMonitor)
        return nullptr;

    return pWindowCandidate;
}

bool IHyprLayout::isWindowReachable(PHLWINDOW pWindow) {
    return pWindow && (!pWindow->isHidden() || pWindow->m_groupData.pNextWindow);
}

void IHyprLayout::bringWindowToTop(PHLWINDOW pWindow) {
    if (pWindow == nullptr)
        return;

    if (pWindow->isHidden() && pWindow->m_groupData.pNextWindow) {
        // grouped, change the current to this window
        pWindow->setGroupCurrent(pWindow);
    }
}

void IHyprLayout::requestFocusForWindow(PHLWINDOW pWindow) {
    bringWindowToTop(pWindow);
    g_pCompositor->focusWindow(pWindow);
    g_pCompositor->warpCursorTo(pWindow->middle());
}

Vector2D IHyprLayout::predictSizeForNewWindowFloating(PHLWINDOW pWindow) { // get all rules, see if we have any size overrides.
    Vector2D sizeOverride = {};
    if (g_pCompositor->m_lastMonitor) {

        // If `persistentsize` is set, use the stored size if available.
        const bool HASPERSISTENTSIZE = std::ranges::any_of(pWindow->m_matchedRules, [](const auto& rule) { return rule->m_ruleType == CWindowRule::RULE_PERSISTENTSIZE; });

        const auto STOREDSIZE = HASPERSISTENTSIZE ? g_pConfigManager->getStoredFloatingSize(pWindow) : std::nullopt;

        if (STOREDSIZE.has_value()) {
            Debug::log(LOG, "using stored size {}x{} for new floating window {}::{}", STOREDSIZE->x, STOREDSIZE->y, pWindow->m_class, pWindow->m_title);
            return STOREDSIZE.value();
        }

        for (auto const& r : g_pConfigManager->getMatchingRules(pWindow, true, true)) {
            if (r->m_ruleType != CWindowRule::RULE_SIZE)
                continue;

            try {
                const auto  VALUE    = r->m_rule.substr(r->m_rule.find(' ') + 1);
                const auto  SIZEXSTR = VALUE.substr(0, VALUE.find(' '));
                const auto  SIZEYSTR = VALUE.substr(VALUE.find(' ') + 1);

                const auto  MAXSIZE = pWindow->requestedMaxSize();

                const float SIZEX = SIZEXSTR == "max" ? std::clamp(MAXSIZE.x, MIN_WINDOW_SIZE, g_pCompositor->m_lastMonitor->m_size.x) :
                                                        stringToPercentage(SIZEXSTR, g_pCompositor->m_lastMonitor->m_size.x);

                const float SIZEY = SIZEYSTR == "max" ? std::clamp(MAXSIZE.y, MIN_WINDOW_SIZE, g_pCompositor->m_lastMonitor->m_size.y) :
                                                        stringToPercentage(SIZEYSTR, g_pCompositor->m_lastMonitor->m_size.y);

                sizeOverride = {SIZEX, SIZEY};

            } catch (...) { Debug::log(LOG, "Rule size failed, rule: {} -> {}", r->m_rule, r->m_value); }
            break;
        }
    }

    return sizeOverride;
}

Vector2D IHyprLayout::predictSizeForNewWindow(PHLWINDOW pWindow) {
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

    Vector2D maxSize = pWindow->m_xdgSurface->m_toplevel->m_pending.maxSize;

    if ((maxSize.x > 0 && maxSize.x < sizePredicted.x) || (maxSize.y > 0 && maxSize.y < sizePredicted.y))
        sizePredicted = {};

    return sizePredicted;
}

bool IHyprLayout::updateDragWindow() {
    const auto DRAGGINGWINDOW = g_pInputManager->m_currentlyDraggedWindow.lock();
    const bool WAS_FULLSCREEN = DRAGGINGWINDOW->isFullscreen();

    if (g_pInputManager->m_dragThresholdReached) {
        if (WAS_FULLSCREEN) {
            Debug::log(LOG, "Dragging a fullscreen window");
            g_pCompositor->setWindowFullscreenInternal(DRAGGINGWINDOW, FSMODE_NONE);
        }

        const auto PWORKSPACE = DRAGGINGWINDOW->m_workspace;

        if (PWORKSPACE->m_hasFullscreenWindow && (!DRAGGINGWINDOW->m_isFloating || (!DRAGGINGWINDOW->m_createdOverFullscreen && !DRAGGINGWINDOW->m_pinned))) {
            Debug::log(LOG, "Rejecting drag on a fullscreen workspace. (window under fullscreen)");
            g_pKeybindManager->changeMouseBindMode(MBIND_INVALID);
            return true;
        }
    }

    DRAGGINGWINDOW->m_draggingTiled   = false;
    m_draggingWindowOriginalFloatSize = DRAGGINGWINDOW->m_lastFloatingSize;

    if (WAS_FULLSCREEN && DRAGGINGWINDOW->m_isFloating) {
        const auto MOUSECOORDS          = g_pInputManager->getMouseCoordsInternal();
        *DRAGGINGWINDOW->m_realPosition = MOUSECOORDS - DRAGGINGWINDOW->m_realSize->goal() / 2.f;
    } else if (!DRAGGINGWINDOW->m_isFloating && g_pInputManager->m_dragMode == MBIND_MOVE) {
        Vector2D MINSIZE                   = DRAGGINGWINDOW->requestedMinSize().clamp(DRAGGINGWINDOW->m_windowData.minSize.valueOr(Vector2D(MIN_WINDOW_SIZE, MIN_WINDOW_SIZE)));
        DRAGGINGWINDOW->m_lastFloatingSize = (DRAGGINGWINDOW->m_realSize->goal() * 0.8489).clamp(MINSIZE, Vector2D{}).floor();
        *DRAGGINGWINDOW->m_realPosition    = g_pInputManager->getMouseCoordsInternal() - DRAGGINGWINDOW->m_realSize->goal() / 2.f;
        if (g_pInputManager->m_dragThresholdReached) {
            changeWindowFloatingMode(DRAGGINGWINDOW);
            DRAGGINGWINDOW->m_isFloating    = true;
            DRAGGINGWINDOW->m_draggingTiled = true;
        }
    }

    m_beginDragXY         = g_pInputManager->getMouseCoordsInternal();
    m_beginDragPositionXY = DRAGGINGWINDOW->m_realPosition->goal();
    m_beginDragSizeXY     = DRAGGINGWINDOW->m_realSize->goal();
    m_lastDragXY          = m_beginDragXY;

    return false;
}
