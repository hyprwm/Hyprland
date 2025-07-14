#include "IHyprLayout.hpp"
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
#include "../managers/EventManager.hpp"
#include "../managers/HookSystemManager.hpp"
#include "config/ConfigDataValues.hpp"
#include <cmath>
#include <hyprlang.hpp>
#include <hyprutils/math/Box.hpp>

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
    const auto window     = g_pInputManager->m_currentlyDraggedWindow.lock();
    m_mouseMoveEventCount = 1;

    if (!validMapped(window)) {
        if (window) {
            g_pInputManager->unsetCursorImage();
            g_pInputManager->m_currentlyDraggedWindow.reset();
        }
        return;
    }

    g_pInputManager->unsetCursorImage();
    g_pInputManager->m_currentlyDraggedWindow.reset();
    g_pInputManager->m_wasDraggingWindow = true;

    if (g_pInputManager->m_dragMode == MBIND_MOVE) {
        g_pHyprRenderer->damageWindow(window);

        const auto MOUSECOORDS  = g_pInputManager->getMouseCoordsInternal();
        PHLWINDOW  targetWindow = g_pCompositor->vectorToWindowUnified(MOUSECOORDS, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING, window);

        if (targetWindow) {
            if (targetWindow->checkInputOnDecos(INPUT_TYPE_DRAG_END, MOUSECOORDS, window))
                return;

            const bool  floatedIntoTiled = !targetWindow->m_isFloating && !window->m_draggingTiled;
            static auto PDRAGINTOGROUP   = CConfigValue<Hyprlang::INT>("group:drag_into_group");

            if (*PDRAGINTOGROUP == 1 && targetWindow->m_groupData.pNextWindow.lock() && window->canBeGroupedInto(targetWindow) && !floatedIntoTiled) {

                if (window->m_groupData.pNextWindow) {
                    PHLWINDOW next = window->m_groupData.pNextWindow.lock();
                    while (next && next != window) {
                        next->m_isFloating    = targetWindow->m_isFloating;           // match the floating state of group members
                        *next->m_realSize     = targetWindow->m_realSize->goal();     // match the size of group members
                        *next->m_realPosition = targetWindow->m_realPosition->goal(); // match the position of group members
                        next                  = next->m_groupData.pNextWindow.lock();
                    }
                }

                window->m_isFloating       = targetWindow->m_isFloating; // match the floating state of the window
                window->m_lastFloatingSize = m_draggingWindowOriginalFloatSize;
                window->m_draggingTiled    = false;

                static auto USECURRPOS        = CConfigValue<Hyprlang::INT>("group:insert_after_current");
                PHLWINDOW   groupInsertTarget = *USECURRPOS ? targetWindow : targetWindow->getGroupTail();

                groupInsertTarget->insertWindowToGroup(window);
                targetWindow->setGroupCurrent(window);
                window->applyGroupRules();
                window->updateWindowDecos();

                if (!window->getDecorationByType(DECORATION_GROUPBAR))
                    window->addWindowDeco(makeUnique<CHyprGroupBarDecoration>(window));
            }
        }
    }

    if (window->m_draggingTiled) {
        static auto PPRECISEMOUSE = CConfigValue<Hyprlang::INT>("dwindle:precise_mouse_move");
        window->m_isFloating      = false;
        g_pInputManager->refocus();

        if (*PPRECISEMOUSE) {
            eDirection      insertDir = DIRECTION_DEFAULT;

            const auto      MOUSECOORDS      = g_pInputManager->getMouseCoordsInternal();
            const PHLWINDOW pReferenceWindow = g_pCompositor->vectorToWindowUnified(MOUSECOORDS, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING, window);

            if (pReferenceWindow && pReferenceWindow != window) {
                const Vector2D draggedCenter   = window->m_realPosition->goal() + window->m_realSize->goal() / 2.f;
                const Vector2D referenceCenter = pReferenceWindow->m_realPosition->goal() + pReferenceWindow->m_realSize->goal() / 2.f;

                const float    xDiff = draggedCenter.x - referenceCenter.x;
                const float    yDiff = draggedCenter.y - referenceCenter.y;

                insertDir = (std::abs(xDiff) > std::abs(yDiff)) ? (xDiff < 0 ? DIRECTION_LEFT : DIRECTION_RIGHT) : (yDiff < 0 ? DIRECTION_UP : DIRECTION_DOWN);
            }

            onWindowRemovedTiling(window);
            onWindowCreatedTiling(window, insertDir);
        } else
            changeWindowFloatingMode(window);

        window->m_lastFloatingSize = m_draggingWindowOriginalFloatSize;
    }

    g_pHyprRenderer->damageWindow(window);
    g_pCompositor->focusWindow(window);

    g_pInputManager->m_wasDraggingWindow = false;
}

static inline bool canSnap(const double a, const double b, const double gap) {
    return std::abs(a - b) < gap;
}

static void snapMove(double& start, double& end, const double target) {
    end   = target + (end - start);
    start = target;
}

static void snapResize(double& start, double& end, const double target) {
    start = target;
}

using SnapFn = std::function<void(double&, double&, double)>;

struct SRange {
    double start, end;
};

static inline SRange getRange(double pos, double size) {
    return {pos, pos + size};
}

static double getGapOffset(bool respectGaps, const char* gapsKeys) {
    if (!respectGaps)
        return 0;
    static auto gapConfig = CConfigValue<Hyprlang::CUSTOMTYPE>(gapsKeys);
    auto*       gaps      = (CCssGapData*)(gapConfig.ptr())->getData();
    return std::max({gaps->m_left, gaps->m_right, gaps->m_top, gaps->m_bottom});
}

static bool isVerticallyAligned(const SRange& a, const SRange& b) {
    return a.start <= b.end && b.start <= a.end;
}

static void snapToOtherWindows(SRange& xRange, SRange& yRange, SnapFn snapFn, PHLWINDOW window, int cornerMask, int& snappedDirs) {
    const auto   WSID           = window->workspaceID();
    const bool   respectGaps    = *CConfigValue<Hyprlang::INT>("general:snap:respect_gaps");
    const double gap            = *CConfigValue<Hyprlang::INT>("general:snap:window_gap");
    const bool   overlapBorders = *CConfigValue<Hyprlang::INT>("general:snap:border_overlap");

    if (!gap)
        return;

    const int  borderSize    = window->getRealBorderSize();
    const bool hasFullScreen = window->m_workspace && window->m_workspace->m_hasFullscreenWindow;

    for (auto& otherWindow : g_pCompositor->m_windows) {
        if (otherWindow == window || !otherWindow->m_isMapped || otherWindow->workspaceID() != WSID || otherWindow->m_fadingOut || otherWindow->isX11OverrideRedirect())
            continue;
        if (hasFullScreen && !otherWindow->m_createdOverFullscreen)
            continue;

        const int    otherBorder = otherWindow->getRealBorderSize();
        const double snapBorder  = overlapBorders ? std::max(borderSize, otherBorder) : (borderSize + otherBorder);
        const double gapOffset   = getGapOffset(respectGaps, "general:gaps_in");

        CBox         otherBox = window->getRealBorderSize();
        SRange       otherX   = {otherBox.x - snapBorder - gapOffset, otherBox.x + otherBox.w + snapBorder + gapOffset};
        SRange       otherY   = {otherBox.y - snapBorder - gapOffset, otherBox.y + otherBox.h + snapBorder + gapOffset};

        // Snap X based on Y overlap
        if (isVerticallyAligned(yRange, otherY)) {
            if ((cornerMask & (CORNER_TOPLEFT | CORNER_BOTTOMLEFT)) && canSnap(xRange.start, otherX.end, gap)) {
                snapFn(xRange.start, xRange.end, otherX.end);
                snappedDirs |= SNAP_LEFT;
            } else if ((cornerMask & (CORNER_TOPRIGHT | CORNER_BOTTOMRIGHT)) && canSnap(xRange.end, otherX.start, gap)) {
                snapFn(xRange.end, xRange.start, otherX.start);
                snappedDirs |= SNAP_RIGHT;
            }
        }

        // Snap Y based on X overlap
        if (isVerticallyAligned(xRange, otherX)) {
            if ((cornerMask & (CORNER_TOPLEFT | CORNER_TOPRIGHT)) && canSnap(yRange.start, otherY.end, gap)) {
                snapFn(yRange.start, yRange.end, otherY.end);
                snappedDirs |= SNAP_UP;
            } else if ((cornerMask & (CORNER_BOTTOMLEFT | CORNER_BOTTOMRIGHT)) && canSnap(yRange.end, otherY.start, gap)) {
                snapFn(yRange.end, yRange.start, otherY.start);
                snappedDirs |= SNAP_DOWN;
            }
        }

        // Extra snapping at corners
        const double borderDelta = otherBorder - borderSize;
        if (xRange.start == otherX.end || otherX.start == xRange.end) {
            SRange yAdj = {otherBox.y - borderDelta, otherBox.y + otherBox.h + borderDelta};
            if ((cornerMask & (CORNER_TOPLEFT | CORNER_TOPRIGHT)) && !(snappedDirs & SNAP_UP) && canSnap(yRange.start, yAdj.start, gap)) {
                snapFn(yRange.start, yRange.end, yAdj.start);
                snappedDirs |= SNAP_UP;
            } else if ((cornerMask & (CORNER_BOTTOMLEFT | CORNER_BOTTOMRIGHT)) && !(snappedDirs & SNAP_DOWN) && canSnap(yRange.end, yAdj.end, gap)) {
                snapFn(yRange.end, yRange.start, yAdj.end);
                snappedDirs |= SNAP_DOWN;
            }
        }

        if (yRange.start == otherY.end || otherY.start == yRange.end) {
            SRange xAdj = {otherBox.x - borderDelta, otherBox.x + otherBox.w + borderDelta};
            if ((cornerMask & (CORNER_TOPLEFT | CORNER_BOTTOMLEFT)) && !(snappedDirs & SNAP_LEFT) && canSnap(xRange.start, xAdj.start, gap)) {
                snapFn(xRange.start, xRange.end, xAdj.start);
                snappedDirs |= SNAP_LEFT;
            } else if ((cornerMask & (CORNER_TOPRIGHT | CORNER_BOTTOMRIGHT)) && !(snappedDirs & SNAP_RIGHT) && canSnap(xRange.end, xAdj.end, gap)) {
                snapFn(xRange.end, xRange.start, xAdj.end);
                snappedDirs |= SNAP_RIGHT;
            }
        }
    }
}

static void snapToMonitorEdges(SRange& xRange, SRange& yRange, SnapFn snapFn, PHLWINDOW window, int cornerMask, int& snappedDirs) {
    const double gap = *CConfigValue<Hyprlang::INT>("general:snap:monitor_gap");
    if (!gap)
        return;

    const bool   respectGaps    = *CConfigValue<Hyprlang::INT>("general:snap:respect_gaps");
    const bool   overlapBorders = *CConfigValue<Hyprlang::INT>("general:snap:border_overlap");
    const double border         = overlapBorders ? window->getRealBorderSize() : 0;
    const double gapOffset      = getGapOffset(respectGaps, "general:gaps_out");

    auto         mon = window->m_monitor.lock();
    if (!mon)
        return;

    const auto& pos  = mon->m_position;
    const auto& size = mon->m_size;
    const auto& tl   = mon->m_reservedTopLeft;
    const auto& br   = mon->m_reservedBottomRight;

    auto        trySnap = [&](double& src, double& opp, double target, int dir) {
        if (canSnap(src, target, gap)) {
            snapFn(src, opp, target);
            snappedDirs |= dir;
        }
    };

    if (cornerMask & (CORNER_TOPLEFT | CORNER_BOTTOMLEFT))
        trySnap(xRange.start, xRange.end, pos.x + tl.x + border + gapOffset, SNAP_LEFT);

    if (cornerMask & (CORNER_TOPRIGHT | CORNER_BOTTOMRIGHT))
        trySnap(xRange.end, xRange.start, pos.x + size.x - br.x - border - gapOffset, SNAP_RIGHT);

    if (cornerMask & (CORNER_TOPLEFT | CORNER_TOPRIGHT))
        trySnap(yRange.start, yRange.end, pos.y + tl.y + border + gapOffset, SNAP_UP);

    if (cornerMask & (CORNER_BOTTOMLEFT | CORNER_BOTTOMRIGHT))
        trySnap(yRange.end, yRange.start, pos.y + size.y - br.y - border - gapOffset, SNAP_DOWN);
}

static void maintainAspectRatio(SRange& xRange, SRange& yRange, int snappedDirs, int cornerMask, const Vector2D& beginSize) {
    if (!(snappedDirs & (SNAP_LEFT | SNAP_RIGHT | SNAP_UP | SNAP_DOWN)))
        return;

    const float ratio = beginSize.y / beginSize.x;

    if ((cornerMask & (CORNER_TOPLEFT | CORNER_BOTTOMLEFT) && (snappedDirs & SNAP_LEFT)) || (cornerMask & (CORNER_TOPRIGHT | CORNER_BOTTOMRIGHT) && (snappedDirs & SNAP_RIGHT))) {
        const double newHeight = (xRange.end - xRange.start) * ratio;
        if (cornerMask & (CORNER_TOPLEFT | CORNER_TOPRIGHT))
            yRange.start = yRange.end - newHeight;
        else
            yRange.end = yRange.start + newHeight;
    } else if ((cornerMask & (CORNER_TOPLEFT | CORNER_TOPRIGHT) && (snappedDirs & SNAP_UP)) ||
               (cornerMask & (CORNER_BOTTOMLEFT | CORNER_BOTTOMRIGHT) && (snappedDirs & SNAP_DOWN))) {
        const double newWidth = (yRange.end - yRange.start) / ratio;
        if (cornerMask & (CORNER_TOPLEFT | CORNER_BOTTOMLEFT))
            xRange.start = xRange.end - newWidth;
        else
            xRange.end = xRange.start + newWidth;
    }
}

static void performSnap(Vector2D& pos, Vector2D& size, PHLWINDOW window, const eMouseBindMode mode, const int cornerMask, const Vector2D& beginSize) {
    const SnapFn snapFn = (mode == MBIND_MOVE) ? snapMove : snapResize;

    SRange       xRange      = getRange(pos.x, size.x);
    SRange       yRange      = getRange(pos.y, size.y);
    int          snappedDirs = 0;

    snapToOtherWindows(xRange, yRange, snapFn, window, cornerMask, snappedDirs);
    snapToMonitorEdges(xRange, yRange, snapFn, window, cornerMask, snappedDirs);

    if (mode == MBIND_RESIZE_FORCE_RATIO) {
        maintainAspectRatio(xRange, yRange, snappedDirs, cornerMask, beginSize);
    }

    pos  = {xRange.start, yRange.start};
    size = {xRange.end - xRange.start, yRange.end - yRange.start};
}

void IHyprLayout::updateWindowMonitorAndDecos(const SP<CWindow>& win) {
    Vector2D middle  = win->m_realPosition->value() + win->m_realSize->value() / 2.f;
    auto     monitor = g_pCompositor->getMonitorFromVector(middle);
    if (monitor && !win->onSpecialWorkspace()) {
        win->m_monitor = monitor;
        win->moveToWorkspace(monitor->m_activeWorkspace);
        win->updateGroupOutputs();
        win->updateToplevel();
    }
    win->updateWindowDecos();
    g_pHyprRenderer->damageWindow(win);
}

Vector2D IHyprLayout::ratioAdjust(const Vector2D& in, float ratio, bool expand) {
    if (expand) {
        if (in.x * ratio > in.y)
            return Vector2D(in.x, in.x * ratio);
        else
            return Vector2D(in.y / ratio, in.y);
    } else {
        if (in.x * ratio < in.y)
            return Vector2D(in.x, in.x * ratio);
        else
            return Vector2D(in.y / ratio, in.y);
    }
}

void IHyprLayout::enforceAspectRatio(Vector2D& size, Vector2D& minSize, Vector2D& maxSize, const SP<CWindow>& win) {
    if (!win->m_windowData.keepAspectRatio.valueOr(false) || g_pInputManager->m_dragMode == MBIND_RESIZE_BLOCK_RATIO)
        return;

    if (m_beginDragSizeXY.x < 1 || m_beginDragSizeXY.y < 1)
        return;

    const float ratio = m_beginDragSizeXY.y / m_beginDragSizeXY.x;

    minSize = ratioAdjust(minSize, ratio, true);
    maxSize = ratioAdjust(maxSize, ratio, false);
    size    = ratioAdjust(size, ratio, false);
}

Vector2D IHyprLayout::computeNewResizePos(const Vector2D& newSize) {
    Vector2D pos = m_beginDragPositionXY;
    switch (m_grabbedCorner) {
        case CORNER_TOPLEFT: return pos - newSize + m_beginDragSizeXY;
        case CORNER_TOPRIGHT: return pos + Vector2D(0.0, (m_beginDragSizeXY - newSize).y);
        case CORNER_BOTTOMLEFT: return pos + Vector2D((m_beginDragSizeXY - newSize).x, 0.0);
        default: return pos;
    }
}

Vector2D IHyprLayout::computeNewResizeSize(const Vector2D& delta) {
    Vector2D size = m_beginDragSizeXY;
    switch (m_grabbedCorner) {
        case CORNER_BOTTOMRIGHT: return size + delta;
        case CORNER_TOPLEFT: return size - delta;
        case CORNER_TOPRIGHT: return size + Vector2D(delta.x, -delta.y);
        case CORNER_BOTTOMLEFT: return size + Vector2D(-delta.x, delta.y);
        case CORNER_NONE: return size;
    }
    return size;
}

void IHyprLayout::handleFloatingResize(const Vector2D& mousePos, const SP<CWindow>& win) {
    static auto snampEnabled = CConfigValue<Hyprlang::INT>("general:snap:enabled");
    static auto pAnimate     = CConfigValue<Hyprlang::INT>("misc:animate_manual_resizes");

    Vector2D    delta   = mousePos - m_beginDragXY;
    Vector2D    minSize = win->requestedMinSize().clamp(win->m_windowData.minSize.valueOr(Vector2D(MIN_WINDOW_SIZE, MIN_WINDOW_SIZE)));
    Vector2D    maxSize = win->m_windowData.maxSize.hasValue() ? win->requestedMaxSize().clamp({}, win->m_windowData.maxSize.value()) :
                                                                 win->requestedMaxSize().clamp({}, Vector2D(std::numeric_limits<double>::max(), std::numeric_limits<double>::max()));

    Vector2D    newSize = computeNewResizeSize(delta);
    Vector2D    newPos  = computeNewResizePos(newSize);

    enforceAspectRatio(newSize, minSize, maxSize, win);
    newSize = newSize.clamp(minSize, maxSize);

    if (*snampEnabled) {
        performSnap(newPos, newSize, win, g_pInputManager->m_dragMode, m_grabbedCorner, m_beginDragSizeXY);
        newSize = newSize.clamp(minSize, maxSize);
    }

    CBox box = {newPos.round(), newSize};
    if (*pAnimate) {
        *win->m_realSize     = box.size();
        *win->m_realPosition = box.pos();
    } else {
        win->m_realSize->setValueAndWarp(box.size());
        win->m_realPosition->setValueAndWarp(box.pos());
        win->sendWindowSize();
    }

    win->m_position = box.pos();
    win->m_size     = box.size();
}

void IHyprLayout::updateWindowResize(const Vector2D& mousePos, const SP<CWindow>& win) {
    if (win->m_isFloating)
        handleFloatingResize(mousePos, win);
    else
        resizeActiveWindow(mousePos - m_lastDragXY, m_grabbedCorner, win);
}

void IHyprLayout::updateWindowPosition(const Vector2D& mousePos, const SP<CWindow>& win) {
    static auto snapEnabled   = CConfigValue<Hyprlang::INT>("general:snap:enabled");
    static auto pAnimateMouse = CConfigValue<Hyprlang::INT>("misc:animate_mouse_windowdragging");

    Vector2D    delta   = mousePos - m_beginDragXY;
    Vector2D    newPos  = m_beginDragPositionXY + delta;
    Vector2D    newSize = win->m_realSize->goal();

    if (*snapEnabled && !win->m_draggingTiled)
        performSnap(newPos, newSize, win, MBIND_MOVE, -1, m_beginDragSizeXY);

    CBox box = {newPos.round(), newSize};
    if (*pAnimateMouse)
        *win->m_realPosition = box.pos();
    else {
        win->m_realPosition->setValueAndWarp(box.pos());
        win->sendWindowSize();
    }
    win->m_position = box.pos();
}

bool IHyprLayout::shouldUpdateMouseMove(const Vector2D& mousePos) {
    static auto pAnimateMouse = CConfigValue<Hyprlang::INT>("misc:animate_mouse_windowdragging");

    const auto  now      = std::chrono::high_resolution_clock::now();
    static auto lastTime = now, msTimer = now;

    Vector2D    tickDelta    = mousePos - m_lastDragXY;
    int         timerDeltaMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count();
    int         msDelta      = std::chrono::duration_cast<std::chrono::milliseconds>(now - msTimer).count();
    msTimer                  = now;

    const float frameDuration = 1000.0f / g_pHyprRenderer->m_mostHzMonitor->m_refreshRate;
    static int  accumulatedMs = 0;
    bool        canSkipUpdate = true;

    if (m_mouseMoveEventCount == 1)
        accumulatedMs = 0;

    if (frameDuration > 16.0f) {
        accumulatedMs += msDelta;
        float avg = accumulatedMs * 1.0f / m_mouseMoveEventCount;
        m_mouseMoveEventCount++;
        float delta   = static_cast<float>(frameDuration - timerDeltaMs);
        canSkipUpdate = std::clamp(delta, 0.0f, frameDuration) > avg;

        if ((std::abs(tickDelta.x) < 1.f && std::abs(tickDelta.y) < 1.f) ||
            (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count() < frameDuration && canSkipUpdate &&
             (g_pInputManager->m_dragMode != MBIND_MOVE || *pAnimateMouse)))
            return false;
    }

    lastTime     = now;
    m_lastDragXY = mousePos;
    return true;
}

void IHyprLayout::handleDragThreshold(const Vector2D& mousePos, const SP<CWindow>& window) {
    static auto dragThreshold = CConfigValue<Hyprlang::INT>("binds:drag_threshold");
    if (*dragThreshold > 0 && !g_pInputManager->m_dragThresholdReached) {
        if ((m_beginDragXY.distanceSq(mousePos) <= std::pow(*dragThreshold, 2)) && m_beginDragXY == m_lastDragXY)
            return;
        g_pInputManager->m_dragThresholdReached = true;
        if (updateDragWindow())
            return;
    }
}

void IHyprLayout::onMouseMove(const Vector2D& mousePos) {
    if (g_pInputManager->m_currentlyDraggedWindow.expired())
        return;

    const auto window = g_pInputManager->m_currentlyDraggedWindow.lock();

    // Window invalid or drag begin size 0,0 meaning we rejected it.
    if ((!validMapped(window) || m_beginDragSizeXY == Vector2D())) {
        g_pKeybindManager->changeMouseBindMode(MBIND_INVALID);
        return;
    }

    handleDragThreshold(mousePos, window);
    if (!shouldUpdateMouseMove(mousePos))
        return;

    g_pHyprRenderer->damageWindow(window);

    if (g_pInputManager->m_dragMode == MBIND_MOVE)
        updateWindowPosition(mousePos, window);
    else
        updateWindowResize(mousePos, window);

    updateWindowMonitorAndDecos(window);
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
