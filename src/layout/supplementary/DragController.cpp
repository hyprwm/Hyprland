#include "DragController.hpp"

#include "../space/Space.hpp"

#include "../../Compositor.hpp"
#include "../../managers/cursor/CursorShapeOverrideController.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../desktop/view/Group.hpp"
#include "../../render/Renderer.hpp"

using namespace Layout;
using namespace Layout::Supplementary;

SP<ITarget> CDragStateController::target() const {
    return m_target.lock();
}

eMouseBindMode CDragStateController::mode() const {
    return m_dragMode;
}

bool CDragStateController::wasDraggingWindow() const {
    return m_wasDraggingWindow;
}

bool CDragStateController::dragThresholdReached() const {
    return m_dragThresholdReached;
}

void CDragStateController::resetDragThresholdReached() {
    m_dragThresholdReached = false;
}

bool CDragStateController::draggingTiled() const {
    return m_draggingTiled;
}

bool CDragStateController::updateDragWindow() {
    const auto DRAGGINGTARGET = m_target.lock();
    const bool WAS_FULLSCREEN = DRAGGINGTARGET->fullscreenMode() != FSMODE_NONE;

    if (m_dragThresholdReached) {
        if (WAS_FULLSCREEN) {
            Log::logger->log(Log::DEBUG, "Dragging a fullscreen window");
            g_pCompositor->setWindowFullscreenInternal(DRAGGINGTARGET->window(), FSMODE_NONE);
        }

        const auto PWORKSPACE     = DRAGGINGTARGET->workspace();
        const auto DRAGGINGWINDOW = DRAGGINGTARGET->window();

        if (PWORKSPACE->m_hasFullscreenWindow && (!DRAGGINGTARGET->floating() || (!DRAGGINGWINDOW->m_createdOverFullscreen && !DRAGGINGWINDOW->m_pinned))) {
            Log::logger->log(Log::DEBUG, "Rejecting drag on a fullscreen workspace. (window under fullscreen)");
            CKeybindManager::changeMouseBindMode(MBIND_INVALID);
            return true;
        }
    }

    m_draggingTiled                   = false;
    m_draggingWindowOriginalFloatSize = DRAGGINGTARGET->lastFloatingSize();

    if (WAS_FULLSCREEN && DRAGGINGTARGET->floating()) {
        const auto MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();
        DRAGGINGTARGET->setPositionGlobal(CBox{MOUSECOORDS - DRAGGINGTARGET->position().size() / 2.F, DRAGGINGTARGET->position().size()});
    } else if (!DRAGGINGTARGET->floating() && m_dragMode == MBIND_MOVE) {
        Vector2D MINSIZE = DRAGGINGTARGET->minSize().value_or(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE});
        DRAGGINGTARGET->rememberFloatingSize((DRAGGINGTARGET->position().size() * 0.8489).clamp(MINSIZE, Vector2D{}).floor());
        DRAGGINGTARGET->setPositionGlobal(CBox{g_pInputManager->getMouseCoordsInternal() - DRAGGINGTARGET->position().size() / 2.F, DRAGGINGTARGET->position().size()});

        if (m_dragThresholdReached) {
            g_layoutManager->changeFloatingMode(DRAGGINGTARGET);
            m_draggingTiled = true;
        }
    }

    const auto DRAG_ORIGINAL_BOX = DRAGGINGTARGET->position();

    m_beginDragXY         = g_pInputManager->getMouseCoordsInternal();
    m_beginDragPositionXY = DRAG_ORIGINAL_BOX.pos();
    m_beginDragSizeXY     = DRAG_ORIGINAL_BOX.size();
    m_lastDragXY          = m_beginDragXY;

    return false;
}

void CDragStateController::dragBegin(SP<ITarget> target, eMouseBindMode mode) {
    m_target   = target;
    m_dragMode = mode;

    const auto  DRAGGINGTARGET = m_target.lock();
    static auto PDRAGTHRESHOLD = CConfigValue<Hyprlang::INT>("binds:drag_threshold");

    m_mouseMoveEventCount = 1;
    m_beginDragSizeXY     = Vector2D();

    // Window will be floating. Let's check if it's valid. It should be, but I don't like crashing.
    if (!validMapped(DRAGGINGTARGET->window())) {
        Log::logger->log(Log::ERR, "Dragging attempted on an invalid window (not mapped)");
        CKeybindManager::changeMouseBindMode(MBIND_INVALID);
        return;
    }

    if (!DRAGGINGTARGET->workspace()) {
        Log::logger->log(Log::ERR, "Dragging attempted on an invalid window (no workspace)");
        CKeybindManager::changeMouseBindMode(MBIND_INVALID);
        return;
    }

    // Try to pick up dragged window now if drag_threshold is disabled
    // or at least update dragging related variables for the cursors
    m_dragThresholdReached = *PDRAGTHRESHOLD <= 0;
    if (updateDragWindow())
        return;

    // get the grab corner
    static auto RESIZECORNER = CConfigValue<Hyprlang::INT>("general:resize_corner");
    if (*RESIZECORNER != 0 && *RESIZECORNER <= 4 && DRAGGINGTARGET->floating()) {
        switch (*RESIZECORNER) {
            case 1:
                m_grabbedCorner = CORNER_TOPLEFT;
                Cursor::overrideController->setOverride("nw-resize", Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
                break;
            case 2:
                m_grabbedCorner = CORNER_TOPRIGHT;
                Cursor::overrideController->setOverride("ne-resize", Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
                break;
            case 3:
                m_grabbedCorner = CORNER_BOTTOMRIGHT;
                Cursor::overrideController->setOverride("se-resize", Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
                break;
            case 4:
                m_grabbedCorner = CORNER_BOTTOMLEFT;
                Cursor::overrideController->setOverride("sw-resize", Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
                break;
        }
    } else if (m_beginDragXY.x < m_beginDragPositionXY.x + m_beginDragSizeXY.x / 2.F) {
        if (m_beginDragXY.y < m_beginDragPositionXY.y + m_beginDragSizeXY.y / 2.F) {
            m_grabbedCorner = CORNER_TOPLEFT;
            Cursor::overrideController->setOverride("nw-resize", Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
        } else {
            m_grabbedCorner = CORNER_BOTTOMLEFT;
            Cursor::overrideController->setOverride("sw-resize", Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
        }
    } else {
        if (m_beginDragXY.y < m_beginDragPositionXY.y + m_beginDragSizeXY.y / 2.F) {
            m_grabbedCorner = CORNER_TOPRIGHT;
            Cursor::overrideController->setOverride("ne-resize", Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
        } else {
            m_grabbedCorner = CORNER_BOTTOMRIGHT;
            Cursor::overrideController->setOverride("se-resize", Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
        }
    }

    if (m_dragMode != MBIND_RESIZE && m_dragMode != MBIND_RESIZE_FORCE_RATIO && m_dragMode != MBIND_RESIZE_BLOCK_RATIO)
        Cursor::overrideController->setOverride("grabbing", Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);

    DRAGGINGTARGET->damageEntire();

    g_pKeybindManager->shadowKeybinds();

    if (DRAGGINGTARGET->window()) {
        Desktop::focusState()->rawWindowFocus(DRAGGINGTARGET->window(), Desktop::FOCUS_REASON_DESKTOP_STATE_CHANGE);
        g_pCompositor->changeWindowZOrder(DRAGGINGTARGET->window(), true);
    }
}
void CDragStateController::dragEnd() {
    auto draggingTarget = m_target.lock();

    m_mouseMoveEventCount = 1;

    if (!validMapped(draggingTarget->window())) {
        if (draggingTarget->window()) {
            Cursor::overrideController->unsetOverride(Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
            m_target.reset();
        }
        return;
    }

    Cursor::overrideController->unsetOverride(Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
    m_target.reset();
    m_wasDraggingWindow = true;

    if (m_dragMode == MBIND_MOVE && draggingTarget->window()) {
        draggingTarget->damageEntire();

        const auto DRAGGING_WINDOW = draggingTarget->window();

        const auto MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();
        PHLWINDOW  pWindow =
            g_pCompositor->vectorToWindowUnified(MOUSECOORDS, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING, DRAGGING_WINDOW);

        if (pWindow) {
            if (pWindow->checkInputOnDecos(INPUT_TYPE_DRAG_END, MOUSECOORDS, DRAGGING_WINDOW))
                return;

            const bool  FLOATEDINTOTILED = !pWindow->m_isFloating && !m_draggingTiled;
            static auto PDRAGINTOGROUP   = CConfigValue<Hyprlang::INT>("group:drag_into_group");

            if (pWindow->m_group && DRAGGING_WINDOW->canBeGroupedInto(pWindow->m_group) && *PDRAGINTOGROUP == 1 && !FLOATEDINTOTILED) {
                pWindow->m_group->add(DRAGGING_WINDOW);
                // fix the draggingTarget, now it's DRAGGING_WINDOW
                draggingTarget = DRAGGING_WINDOW->m_target;
            }
        }
    }

    if (m_draggingTiled) {
        // static auto PPRECISEMOUSE = CConfigValue<Hyprlang::INT>("dwindle:precise_mouse_move");

        // FIXME: remove or rethink
        // if (*PPRECISEMOUSE) {
        //     eDirection      direction = DIRECTION_DEFAULT;

        //     const auto      MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();
        //     const PHLWINDOW pReferenceWindow =
        //         g_pCompositor->vectorToWindowUnified(MOUSECOORDS, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING, DRAGGINGWINDOW);

        //     if (pReferenceWindow && pReferenceWindow != DRAGGINGWINDOW) {
        //         const Vector2D draggedCenter   = DRAGGINGWINDOW->m_realPosition->goal() + DRAGGINGWINDOW->m_realSize->goal() / 2.f;
        //         const Vector2D referenceCenter = pReferenceWindow->m_realPosition->goal() + pReferenceWindow->m_realSize->goal() / 2.f;
        //         const float    xDiff           = draggedCenter.x - referenceCenter.x;
        //         const float    yDiff           = draggedCenter.y - referenceCenter.y;

        //         if (fabs(xDiff) > fabs(yDiff))
        //             direction = xDiff < 0 ? DIRECTION_LEFT : DIRECTION_RIGHT;
        //         else
        //             direction = yDiff < 0 ? DIRECTION_UP : DIRECTION_DOWN;
        //     }

        //     onWindowRemovedTiling(DRAGGINGWINDOW);
        //     onWindowCreatedTiling(DRAGGINGWINDOW, direction);
        // } else

        // make sure to check if we are floating because drag into group could make us tiled already
        if (draggingTarget->floating())
            g_layoutManager->changeFloatingMode(draggingTarget);

        draggingTarget->rememberFloatingSize(m_draggingWindowOriginalFloatSize);
    }

    draggingTarget->damageEntire();

    Desktop::focusState()->fullWindowFocus(draggingTarget->window(), Desktop::FOCUS_REASON_DESKTOP_STATE_CHANGE);

    m_wasDraggingWindow = false;
    m_dragMode          = MBIND_INVALID;
}

void CDragStateController::mouseMove(const Vector2D& mousePos) {
    if (m_target.expired())
        return;

    const auto  DRAGGINGTARGET = m_target.lock();
    static auto PDRAGTHRESHOLD = CConfigValue<Hyprlang::INT>("binds:drag_threshold");

    // Window invalid or drag begin size 0,0 meaning we rejected it.
    if ((!validMapped(DRAGGINGTARGET->window()) || m_beginDragSizeXY == Vector2D())) {
        CKeybindManager::changeMouseBindMode(MBIND_INVALID);
        return;
    }

    // Yoink dragged window here instead if using drag_threshold and it has been reached
    if (*PDRAGTHRESHOLD > 0 && !m_dragThresholdReached) {
        if ((m_beginDragXY.distanceSq(mousePos) <= std::pow(*PDRAGTHRESHOLD, 2) && m_beginDragXY == m_lastDragXY))
            return;
        m_dragThresholdReached = true;
        if (updateDragWindow())
            return;
    }

    static auto TIMER = std::chrono::high_resolution_clock::now(), MSTIMER = TIMER;

    const auto  DELTA     = Vector2D(mousePos.x - m_beginDragXY.x, mousePos.y - m_beginDragXY.y);
    const auto  TICKDELTA = Vector2D(mousePos.x - m_lastDragXY.x, mousePos.y - m_lastDragXY.y);

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

    if ((abs(TICKDELTA.x) < 1.f && abs(TICKDELTA.y) < 1.f) || (TIMERDELTA < MSMONITOR && canSkipUpdate && (m_dragMode != MBIND_MOVE)))
        return;

    TIMER = std::chrono::high_resolution_clock::now();

    m_lastDragXY = mousePos;

    DRAGGINGTARGET->damageEntire();

    if (m_dragMode == MBIND_MOVE) {

        Vector2D newPos  = m_beginDragPositionXY + DELTA;
        Vector2D newSize = DRAGGINGTARGET->position().size();

        if (*SNAPENABLED && !m_draggingTiled)
            g_layoutManager->performSnap(newPos, newSize, DRAGGINGTARGET, MBIND_MOVE, -1, m_beginDragSizeXY);

        newPos = newPos.round();

        DRAGGINGTARGET->setPositionGlobal({newPos, newSize});
        DRAGGINGTARGET->warpPositionSize();
    } else if (m_dragMode == MBIND_RESIZE || m_dragMode == MBIND_RESIZE_FORCE_RATIO || m_dragMode == MBIND_RESIZE_BLOCK_RATIO) {
        if (DRAGGINGTARGET->floating()) {

            Vector2D MINSIZE = DRAGGINGTARGET->minSize().value_or(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE});
            Vector2D MAXSIZE = DRAGGINGTARGET->maxSize().value_or(Math::VECTOR2D_MAX);

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

            eMouseBindMode mode = m_dragMode;
            if (DRAGGINGTARGET->window() && DRAGGINGTARGET->window()->m_ruleApplicator->keepAspectRatio().valueOrDefault() && mode != MBIND_RESIZE_BLOCK_RATIO)
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
                g_layoutManager->performSnap(newPos, newSize, DRAGGINGTARGET, mode, m_grabbedCorner, m_beginDragSizeXY);
                newSize = newSize.clamp(MINSIZE, MAXSIZE);
            }

            CBox wb = {newPos, newSize};
            wb.round();

            DRAGGINGTARGET->setPositionGlobal(wb);
            DRAGGINGTARGET->warpPositionSize();
        } else {
            g_layoutManager->resizeTarget(TICKDELTA, DRAGGINGTARGET, m_grabbedCorner);
            DRAGGINGTARGET->warpPositionSize();
        }
    }

    // get middle point
    Vector2D middle = DRAGGINGTARGET->position().middle();

    // and check its monitor
    const auto PMONITOR = g_pCompositor->getMonitorFromVector(middle);

    if (PMONITOR && PMONITOR->m_activeWorkspace) {
        const auto WS = PMONITOR->m_activeSpecialWorkspace ? PMONITOR->m_activeSpecialWorkspace : PMONITOR->m_activeWorkspace;
        DRAGGINGTARGET->assignToSpace(WS->m_space);
    }

    DRAGGINGTARGET->damageEntire();
}
