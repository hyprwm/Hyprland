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
        if (pWindow->m_groupData.pNextWindow.lock() == pWindow) {
            pWindow->m_groupData.pNextWindow.reset();
            pWindow->updateWindowDecos();
        } else {
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
    const bool      FLOATEDINTOTILED = pWindow->m_isFloating && OPENINGON && !OPENINGON->m_isFloating;
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

        return true;
    }

    return false;
}

void IHyprLayout::onBeginDragWindow(PHLWINDOW) {
    ;
}

void IHyprLayout::onEndDragWindow() {
    ;
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

void IHyprLayout::performSnap(Vector2D& sourcePos, Vector2D& sourceSize, PHLWINDOW DRAGGINGWINDOW, const Interactive::eWindowDragMode MODE, const int CORNER,
                              const Vector2D& BEGINSIZE) {
    static auto  SNAPWINDOWGAP     = CConfigValue<Hyprlang::INT>("general:snap:window_gap");
    static auto  SNAPMONITORGAP    = CConfigValue<Hyprlang::INT>("general:snap:monitor_gap");
    static auto  SNAPBORDEROVERLAP = CConfigValue<Hyprlang::INT>("general:snap:border_overlap");
    static auto  SNAPRESPECTGAPS   = CConfigValue<Hyprlang::INT>("general:snap:respect_gaps");

    static auto  PGAPSIN  = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_in");
    static auto  PGAPSOUT = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_out");
    const auto   GAPSNONE = CCssGapData{0, 0, 0, 0};

    const SnapFn SNAP  = (MODE == Interactive::WINDOW_DRAG_MOVE) ? snapMove : snapResize;
    int          snaps = 0;

    struct SRange {
        double start = 0;
        double end   = 0;
    };
    const auto EXTENTS = DRAGGINGWINDOW->getWindowExtentsUnified(RESERVED_EXTENTS | INPUT_EXTENTS);
    SRange     sourceX = {sourcePos.x - EXTENTS.topLeft.x, sourcePos.x + sourceSize.x + EXTENTS.bottomRight.x};
    SRange     sourceY = {sourcePos.y - EXTENTS.topLeft.y, sourcePos.y + sourceSize.y + EXTENTS.bottomRight.y};

    if (*SNAPWINDOWGAP) {
        const double GAPSIZE       = *SNAPWINDOWGAP;
        const auto   WSID          = DRAGGINGWINDOW->workspaceID();
        const bool   HASFULLSCREEN = DRAGGINGWINDOW->m_workspace && DRAGGINGWINDOW->m_workspace->m_hasFullscreenWindow;

        const auto*  GAPSIN = *SNAPRESPECTGAPS ? sc<CCssGapData*>(PGAPSIN.ptr()->getData()) : &GAPSNONE;
        const double GAPSX  = GAPSIN->m_left + GAPSIN->m_right;
        const double GAPSY  = GAPSIN->m_top + GAPSIN->m_bottom;

        for (auto& other : g_pCompositor->m_windows) {
            if ((HASFULLSCREEN && !other->m_createdOverFullscreen) || other == DRAGGINGWINDOW || other->workspaceID() != WSID || !other->m_isMapped || other->m_fadingOut ||
                other->isX11OverrideRedirect())
                continue;

            const CBox   SURF   = other->getWindowBoxUnified(RESERVED_EXTENTS);
            const SRange SURFBX = {SURF.x - GAPSX, SURF.x + SURF.w + GAPSX};
            const SRange SURFBY = {SURF.y - GAPSY, SURF.y + SURF.h + GAPSY};

            // only snap windows if their ranges overlap in the opposite axis
            if (sourceY.start <= SURFBY.end && SURFBY.start <= sourceY.end) {
                if (CORNER & (Interactive::CORNER_TOPLEFT | Interactive::CORNER_BOTTOMLEFT) && canSnap(sourceX.start, SURFBX.end, GAPSIZE)) {
                    SNAP(sourceX.start, sourceX.end, SURFBX.end);
                    snaps |= SNAP_LEFT;
                } else if (CORNER & (Interactive::CORNER_TOPRIGHT | Interactive::CORNER_BOTTOMRIGHT) && canSnap(sourceX.end, SURFBX.start, GAPSIZE)) {
                    SNAP(sourceX.end, sourceX.start, SURFBX.start);
                    snaps |= SNAP_RIGHT;
                }
            }
            if (sourceX.start <= SURFBX.end && SURFBX.start <= sourceX.end) {
                if (CORNER & (Interactive::CORNER_TOPLEFT | Interactive::CORNER_TOPRIGHT) && canSnap(sourceY.start, SURFBY.end, GAPSIZE)) {
                    SNAP(sourceY.start, sourceY.end, SURFBY.end);
                    snaps |= SNAP_UP;
                } else if (CORNER & (Interactive::CORNER_BOTTOMLEFT | Interactive::CORNER_BOTTOMRIGHT) && canSnap(sourceY.end, SURFBY.start, GAPSIZE)) {
                    SNAP(sourceY.end, sourceY.start, SURFBY.start);
                    snaps |= SNAP_DOWN;
                }
            }

            // corner snapping
            if (sourceX.start == SURFBX.end || SURFBX.start == sourceX.end) {
                const SRange SURFY = {SURFBY.start + GAPSY, SURFBY.end - GAPSY};
                if (CORNER & (Interactive::CORNER_TOPLEFT | Interactive::CORNER_TOPRIGHT) && !(snaps & SNAP_UP) && canSnap(sourceY.start, SURFY.start, GAPSIZE)) {
                    SNAP(sourceY.start, sourceY.end, SURFY.start);
                    snaps |= SNAP_UP;
                } else if (CORNER & (Interactive::CORNER_BOTTOMLEFT | Interactive::CORNER_BOTTOMRIGHT) && !(snaps & SNAP_DOWN) && canSnap(sourceY.end, SURFY.end, GAPSIZE)) {
                    SNAP(sourceY.end, sourceY.start, SURFY.end);
                    snaps |= SNAP_DOWN;
                }
            }
            if (sourceY.start == SURFBY.end || SURFBY.start == sourceY.end) {
                const SRange SURFX = {SURFBX.start + GAPSX, SURFBX.end - GAPSX};
                if (CORNER & (Interactive::CORNER_TOPLEFT | Interactive::CORNER_BOTTOMLEFT) && !(snaps & SNAP_LEFT) && canSnap(sourceX.start, SURFX.start, GAPSIZE)) {
                    SNAP(sourceX.start, sourceX.end, SURFX.start);
                    snaps |= SNAP_LEFT;
                } else if (CORNER & (Interactive::CORNER_TOPRIGHT | Interactive::CORNER_BOTTOMRIGHT) && !(snaps & SNAP_RIGHT) && canSnap(sourceX.end, SURFX.end, GAPSIZE)) {
                    SNAP(sourceX.end, sourceX.start, SURFX.end);
                    snaps |= SNAP_RIGHT;
                }
            }
        }
    }

    if (*SNAPMONITORGAP) {
        const double GAPSIZE    = *SNAPMONITORGAP;
        const auto   EXTENTNONE = SBoxExtents{{0, 0}, {0, 0}};
        const auto*  EXTENTDIFF = *SNAPBORDEROVERLAP ? &EXTENTS : &EXTENTNONE;
        const auto   MON        = DRAGGINGWINDOW->m_monitor.lock();
        const auto*  GAPSOUT    = *SNAPRESPECTGAPS ? sc<CCssGapData*>(PGAPSOUT.ptr()->getData()) : &GAPSNONE;

        SRange       monX = {MON->m_position.x + MON->m_reservedTopLeft.x + GAPSOUT->m_left, MON->m_position.x + MON->m_size.x - MON->m_reservedBottomRight.x - GAPSOUT->m_right};
        SRange       monY = {MON->m_position.y + MON->m_reservedTopLeft.y + GAPSOUT->m_top, MON->m_position.y + MON->m_size.y - MON->m_reservedBottomRight.y - GAPSOUT->m_bottom};

        if (CORNER & (Interactive::CORNER_TOPLEFT | Interactive::CORNER_BOTTOMLEFT) &&
            ((MON->m_reservedTopLeft.x > 0 && canSnap(sourceX.start, monX.start, GAPSIZE)) ||
             canSnap(sourceX.start, (monX.start -= MON->m_reservedTopLeft.x + EXTENTDIFF->topLeft.x), GAPSIZE))) {
            SNAP(sourceX.start, sourceX.end, monX.start);
            snaps |= SNAP_LEFT;
        }
        if (CORNER & (Interactive::CORNER_TOPRIGHT | Interactive::CORNER_BOTTOMRIGHT) &&
            ((MON->m_reservedBottomRight.x > 0 && canSnap(sourceX.end, monX.end, GAPSIZE)) ||
             canSnap(sourceX.end, (monX.end += MON->m_reservedBottomRight.x + EXTENTDIFF->bottomRight.x), GAPSIZE))) {
            SNAP(sourceX.end, sourceX.start, monX.end);
            snaps |= SNAP_RIGHT;
        }
        if (CORNER & (Interactive::CORNER_TOPLEFT | Interactive::CORNER_TOPRIGHT) &&
            ((MON->m_reservedTopLeft.y > 0 && canSnap(sourceY.start, monY.start, GAPSIZE)) ||
             canSnap(sourceY.start, (monY.start -= MON->m_reservedTopLeft.y + EXTENTDIFF->topLeft.y), GAPSIZE))) {
            SNAP(sourceY.start, sourceY.end, monY.start);
            snaps |= SNAP_UP;
        }
        if (CORNER & (Interactive::CORNER_BOTTOMLEFT | Interactive::CORNER_BOTTOMRIGHT) &&
            ((MON->m_reservedBottomRight.y > 0 && canSnap(sourceY.end, monY.end, GAPSIZE)) ||
             canSnap(sourceY.end, (monY.end += MON->m_reservedBottomRight.y + EXTENTDIFF->bottomRight.y), GAPSIZE))) {
            SNAP(sourceY.end, sourceY.start, monY.end);
            snaps |= SNAP_DOWN;
        }
    }

    // remove extents from main surface
    sourceX = {sourceX.start + EXTENTS.topLeft.x, sourceX.end - EXTENTS.bottomRight.x};
    sourceY = {sourceY.start + EXTENTS.topLeft.y, sourceY.end - EXTENTS.bottomRight.y};

    if (MODE == Interactive::WINDOW_DRAG_RESIZE_FORCE_RATIO) {
        if ((CORNER & (Interactive::CORNER_TOPLEFT | Interactive::CORNER_BOTTOMLEFT) && snaps & SNAP_LEFT) ||
            (CORNER & (Interactive::CORNER_TOPRIGHT | Interactive::CORNER_BOTTOMRIGHT) && snaps & SNAP_RIGHT)) {
            const double SIZEY = (sourceX.end - sourceX.start) * (BEGINSIZE.y / BEGINSIZE.x);
            if (CORNER & (Interactive::CORNER_TOPLEFT | Interactive::CORNER_TOPRIGHT))
                sourceY.start = sourceY.end - SIZEY;
            else
                sourceY.end = sourceY.start + SIZEY;
        } else if ((CORNER & (Interactive::CORNER_TOPLEFT | Interactive::CORNER_TOPRIGHT) && snaps & SNAP_UP) ||
                   (CORNER & (Interactive::CORNER_BOTTOMLEFT | Interactive::CORNER_BOTTOMRIGHT) && snaps & SNAP_DOWN)) {
            const double SIZEX = (sourceY.end - sourceY.start) * (BEGINSIZE.x / BEGINSIZE.y);
            if (CORNER & (Interactive::CORNER_TOPLEFT | Interactive::CORNER_BOTTOMLEFT))
                sourceX.start = sourceX.end - SIZEX;
            else
                sourceX.end = sourceX.start + SIZEX;
        }
    }

    sourcePos  = {sourceX.start, sourceY.start};
    sourceSize = {sourceX.end - sourceX.start, sourceY.end - sourceY.start};
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
    g_pEventManager->postEvent(SHyprIPCEvent{"changefloatingmode", std::format("{:x},{}", rc<uintptr_t>(pWindow.get()), sc<int>(TILED))});
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
