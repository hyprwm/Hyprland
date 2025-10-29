#include "Drag.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../managers/input/InputManager.hpp"
#include "../../managers/KeybindManager.hpp"
#include "../../managers/LayoutManager.hpp"
#include "../../managers/HookSystemManager.hpp"
#include "../../Compositor.hpp"
#include "../../render/Renderer.hpp"
#include "../Window.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Interactive;
using namespace Hyprutils::Utils;

static UP<CDrag> g_drag;

CDrag::CDrag(PHLWINDOW w, eWindowDragMode mode, eDragInputType inType) : m_window(w), m_dragMode(mode) {
    onBeginDrag();

    // register listeners for our in type
    if (inType == DRAG_INPUT_TYPE_MOUSE) {
        m_mouseMove = g_pHookSystem->hookDynamic("mouseMove", [this](void* self, SCallbackInfo& info, std::any e) { onMouseMoved(); });

        m_mouseButton = g_pHookSystem->hookDynamic("mouseButton", [](void* self, SCallbackInfo& info, std::any e) {
            auto E = std::any_cast<IPointer::SButtonEvent>(e);
            if (E.state != WL_POINTER_BUTTON_STATE_RELEASED)
                return;

            Debug::log(LOG, "CDrag: dropping drag on mouse up");
            end();
        });
    } else if (inType == DRAG_INPUT_TYPE_TOUCH) {
        m_touchMove = g_pHookSystem->hookDynamic("touchMove", [this](void* self, SCallbackInfo& info, std::any e) { onMouseMoved(); });

        m_touchUp = g_pHookSystem->hookDynamic("touchUp", [](void* self, SCallbackInfo& info, std::any e) {
            Debug::log(LOG, "CDrag: dropping drag on touch up");
            end();
        });
    } else
        Debug::log(ERR, "CDrag: BUG THIS: invalid drag input type");
}

CDrag::~CDrag() {
    if (m_window)
        end();
}

bool CDrag::end() {
    if (!g_drag)
        return false;

    if (!validMapped(g_drag->m_window)) {
        g_drag.reset();
        return false;
    }

    g_drag->onEndDrag();
    g_drag->m_window.reset();
    g_drag.reset();
    return true;
}

bool CDrag::active() {
    return g_drag && validMapped(g_drag->m_window);
}

bool CDrag::start(PHLWINDOW w, eWindowDragMode mode, eDragInputType inType) {
    if (active())
        return false;

    if (!validMapped(w))
        return false;

    const auto MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();

    if (!w->isFullscreen() && mode == WINDOW_DRAG_MOVE)
        w->checkInputOnDecos(INPUT_TYPE_DRAG_START, MOUSECOORDS);

    g_drag = UP<CDrag>(new CDrag(w, mode, inType));
    return true;
}

void CDrag::clearDragThreshold() {
    if (!active())
        return;
    g_drag->m_dragThresholdReached = false;
}

bool CDrag::dragThresholdReached() {
    if (!active())
        return false;
    return g_drag->m_dragThresholdReached;
}

eWindowDragMode CDrag::getDragMode() {
    if (!active())
        return WINDOW_DRAG_INVALID;
    return g_drag->m_dragMode;
}

PHLWINDOW CDrag::getDragWindow() {
    if (!active())
        return nullptr;
    return g_drag->m_window.lock();
}

void CDrag::onBeginDrag() {
    static auto PDRAGTHRESHOLD = CConfigValue<Hyprlang::INT>("binds:drag_threshold");

    m_mouseMoveEventCount = 1;
    m_beginDragSizeXY     = Vector2D();

    // Window will be floating. Let's check if it's valid. It should be, but I don't like crashing.
    if (!validMapped(m_window)) {
        Debug::log(ERR, "Dragging attempted on an invalid window!");
        end();
        return;
    }

    // Try to pick up dragged window now if drag_threshold is disabled
    // or at least update dragging related variables for the cursors
    m_dragThresholdReached = *PDRAGTHRESHOLD <= 0;
    if (updateDragWindow())
        return;

    g_pLayoutManager->getCurrentLayout()->onBeginDragWindow(m_window.lock());

    // get the grab corner
    static auto RESIZECORNER = CConfigValue<Hyprlang::INT>("general:resize_corner");
    if (*RESIZECORNER != 0 && *RESIZECORNER <= 4 && m_window->m_isFloating) {
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
            default:
                Debug::log(TRACE, "CDrag: unknown corner in grab. Will fall back");
                m_grabbedCorner = CORNER_BOTTOMRIGHT;
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

    if (m_dragMode != WINDOW_DRAG_RESIZE && m_dragMode != WINDOW_DRAG_RESIZE_BLOCK_RATIO && m_dragMode != WINDOW_DRAG_RESIZE_FORCE_RATIO)
        g_pInputManager->setCursorImageUntilUnset("grabbing");

    g_pHyprRenderer->damageWindow(m_window.lock());

    g_pKeybindManager->shadowKeybinds();

    g_pCompositor->focusWindow(m_window.lock());
    g_pCompositor->changeWindowZOrder(m_window.lock(), true);
}

bool CDrag::updateDragWindow() {
    if (!m_window)
        return false;

    const bool WAS_FULLSCREEN = m_window->isFullscreen();

    if (m_dragThresholdReached) {
        if (WAS_FULLSCREEN) {
            Debug::log(LOG, "Dragging a fullscreen window");
            g_pCompositor->setWindowFullscreenInternal(m_window.lock(), FSMODE_NONE);
        }

        const auto PWORKSPACE = m_window->m_workspace;

        if (PWORKSPACE->m_hasFullscreenWindow && (!m_window->m_isFloating || (!m_window->m_createdOverFullscreen && !m_window->m_pinned))) {
            Debug::log(LOG, "Rejecting drag on a fullscreen workspace. (window under fullscreen)");
            end();
            return true;
        }
    }

    m_window->m_draggingTiled         = false;
    m_draggingWindowOriginalFloatSize = m_window->m_lastFloatingSize;

    if (WAS_FULLSCREEN && m_window->m_isFloating) {
        const auto MOUSECOORDS    = g_pInputManager->getMouseCoordsInternal();
        *m_window->m_realPosition = MOUSECOORDS - m_window->m_realSize->goal() / 2.f;
    } else if (!m_window->m_isFloating && m_dragMode == WINDOW_DRAG_MOVE) {
        Vector2D MINSIZE             = m_window->requestedMinSize().clamp(m_window->m_windowData.minSize.valueOr(Vector2D(MIN_WINDOW_SIZE, MIN_WINDOW_SIZE)));
        m_window->m_lastFloatingSize = (m_window->m_realSize->goal() * 0.8489).clamp(MINSIZE, Vector2D{}).floor();
        *m_window->m_realPosition    = g_pInputManager->getMouseCoordsInternal() - m_window->m_realSize->goal() / 2.f;
        if (m_dragThresholdReached) {
            g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(m_window.lock());
            m_window->m_isFloating    = true;
            m_window->m_draggingTiled = true;
        }
    }

    m_beginDragXY         = g_pInputManager->getMouseCoordsInternal();
    m_beginDragPositionXY = m_window->m_realPosition->goal();
    m_beginDragSizeXY     = m_window->m_realSize->goal();
    m_lastDragXY          = m_beginDragXY;

    return false;
}

void CDrag::onMouseMoved() {
    static auto PDRAGTHRESHOLD = CConfigValue<Hyprlang::INT>("binds:drag_threshold");

    // Window invalid or drag begin size 0,0 meaning we rejected it.
    if ((!validMapped(m_window) || m_beginDragSizeXY == Vector2D())) {
        end();
        return;
    }

    const auto MOUSE_POS = g_pInputManager->getMouseCoordsInternal();

    // Yoink dragged window here instead if using drag_threshold and it has been reached
    if (*PDRAGTHRESHOLD > 0 && !m_dragThresholdReached) {
        if ((m_beginDragXY.distanceSq(MOUSE_POS) <= std::pow(*PDRAGTHRESHOLD, 2) && m_beginDragXY == m_lastDragXY))
            return;
        m_dragThresholdReached = true;
        if (updateDragWindow())
            return;
    }

    static auto TIMER = std::chrono::high_resolution_clock::now(), MSTIMER = TIMER;

    const auto  SPECIAL = m_window->onSpecialWorkspace();

    const auto  DELTA     = Vector2D(MOUSE_POS.x - m_beginDragXY.x, MOUSE_POS.y - m_beginDragXY.y);
    const auto  TICKDELTA = Vector2D(MOUSE_POS.x - m_lastDragXY.x, MOUSE_POS.y - m_lastDragXY.y);

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

    if ((abs(TICKDELTA.x) < 1.f && abs(TICKDELTA.y) < 1.f) || (TIMERDELTA < MSMONITOR && canSkipUpdate && (m_dragMode != WINDOW_DRAG_MOVE || *PANIMATEMOUSE)))
        return;

    TIMER = std::chrono::high_resolution_clock::now();

    m_lastDragXY = MOUSE_POS;

    g_pHyprRenderer->damageWindow(m_window.lock());

    if (m_dragMode == WINDOW_DRAG_MOVE) {

        Vector2D newPos  = m_beginDragPositionXY + DELTA;
        Vector2D newSize = m_window->m_realSize->goal();

        if (*SNAPENABLED && !m_window->m_draggingTiled)
            g_pLayoutManager->getCurrentLayout()->performSnap(newPos, newSize, m_window.lock(), WINDOW_DRAG_MOVE, -1, m_beginDragSizeXY);

        newPos = newPos.round();

        if (*PANIMATEMOUSE)
            *m_window->m_realPosition = newPos;
        else {
            m_window->m_realPosition->setValueAndWarp(newPos);
            m_window->sendWindowSize();
        }

        m_window->m_position = newPos;

    } else if (m_dragMode == WINDOW_DRAG_RESIZE || m_dragMode == WINDOW_DRAG_RESIZE_FORCE_RATIO || m_dragMode == WINDOW_DRAG_RESIZE_BLOCK_RATIO) {
        if (m_window->m_isFloating) {

            Vector2D MINSIZE = m_window->requestedMinSize().clamp(m_window->m_windowData.minSize.valueOr(Vector2D(MIN_WINDOW_SIZE, MIN_WINDOW_SIZE)));
            Vector2D MAXSIZE;
            if (m_window->m_windowData.maxSize.hasValue())
                MAXSIZE = m_window->requestedMaxSize().clamp({}, m_window->m_windowData.maxSize.value());
            else
                MAXSIZE = m_window->requestedMaxSize().clamp({}, Vector2D(std::numeric_limits<double>::max(), std::numeric_limits<double>::max()));

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

            eWindowDragMode mode = m_dragMode;
            if (m_window->m_windowData.keepAspectRatio.valueOrDefault() && mode != WINDOW_DRAG_RESIZE_BLOCK_RATIO)
                mode = WINDOW_DRAG_RESIZE_FORCE_RATIO;

            if (m_beginDragSizeXY.x >= 1 && m_beginDragSizeXY.y >= 1 && mode == WINDOW_DRAG_RESIZE_FORCE_RATIO) {

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
                g_pLayoutManager->getCurrentLayout()->performSnap(newPos, newSize, m_window.lock(), mode, m_grabbedCorner, m_beginDragSizeXY);
                newSize = newSize.clamp(MINSIZE, MAXSIZE);
            }

            CBox wb = {newPos, newSize};
            wb.round();

            if (*PANIMATE) {
                *m_window->m_realSize     = wb.size();
                *m_window->m_realPosition = wb.pos();
            } else {
                m_window->m_realSize->setValueAndWarp(wb.size());
                m_window->m_realPosition->setValueAndWarp(wb.pos());
                m_window->sendWindowSize();
            }

            m_window->m_position = wb.pos();
            m_window->m_size     = wb.size();
        } else
            g_pLayoutManager->getCurrentLayout()->resizeActiveWindow(TICKDELTA, m_grabbedCorner, m_window.lock());
    }

    // get middle point
    Vector2D middle = m_window->m_realPosition->value() + m_window->m_realSize->value() / 2.f;

    // and check its monitor
    const auto PMONITOR = g_pCompositor->getMonitorFromVector(middle);

    if (PMONITOR && !SPECIAL) {
        m_window->m_monitor = PMONITOR;
        m_window->moveToWorkspace(PMONITOR->m_activeWorkspace);
        m_window->updateGroupOutputs();

        m_window->updateToplevel();
    }

    m_window->updateWindowDecos();

    g_pHyprRenderer->damageWindow(m_window.lock());
}

void CDrag::onEndDrag() {
    CScopeGuard x([] { g_pLayoutManager->getCurrentLayout()->onEndDragWindow(); });

    m_mouseMoveEventCount = 1;

    g_pInputManager->unsetCursorImage();

    if (!validMapped(m_window))
        return;

    g_pInputManager->unsetCursorImage();

    // try to move into group
    if (m_dragMode == WINDOW_DRAG_MOVE) {
        g_pHyprRenderer->damageWindow(m_window.lock());
        const auto MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();
        PHLWINDOW  pWindow     = g_pCompositor->vectorToWindowUnified(MOUSECOORDS, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING, m_window.lock());

        if (pWindow) {
            if (pWindow->checkInputOnDecos(INPUT_TYPE_DRAG_END, MOUSECOORDS, m_window))
                return;

            const bool  FLOATEDINTOTILED = !pWindow->m_isFloating && !m_window->m_draggingTiled;
            static auto PDRAGINTOGROUP   = CConfigValue<Hyprlang::INT>("group:drag_into_group");

            if (pWindow->m_groupData.pNextWindow.lock() && m_window->canBeGroupedInto(pWindow) && *PDRAGINTOGROUP == 1 && !FLOATEDINTOTILED) {

                if (m_window->m_groupData.pNextWindow) {
                    PHLWINDOW next = m_window->m_groupData.pNextWindow.lock();
                    while (next != m_window) {
                        next->m_isFloating    = pWindow->m_isFloating;           // match the floating state of group members
                        *next->m_realSize     = pWindow->m_realSize->goal();     // match the size of group members
                        *next->m_realPosition = pWindow->m_realPosition->goal(); // match the position of group members
                        next                  = next->m_groupData.pNextWindow.lock();
                    }
                }

                m_window->m_isFloating       = pWindow->m_isFloating; // match the floating state of the window
                m_window->m_lastFloatingSize = m_draggingWindowOriginalFloatSize;
                m_window->m_draggingTiled    = false;

                static auto USECURRPOS = CConfigValue<Hyprlang::INT>("group:insert_after_current");
                (*USECURRPOS ? pWindow : pWindow->getGroupTail())->insertWindowToGroup(m_window.lock());
                pWindow->setGroupCurrent(m_window.lock());
                m_window->applyGroupRules();
                m_window->updateWindowDecos();
            }
        }
    }

    if (m_window->m_draggingTiled) {
        static auto PPRECISEMOUSE = CConfigValue<Hyprlang::INT>("dwindle:precise_mouse_move");
        m_window->m_isFloating    = false;
        g_pInputManager->refocus();

        if (*PPRECISEMOUSE) {
            eDirection      direction = DIRECTION_DEFAULT;

            const auto      MOUSECOORDS      = g_pInputManager->getMouseCoordsInternal();
            const PHLWINDOW pReferenceWindow = g_pCompositor->vectorToWindowUnified(MOUSECOORDS, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING, m_window.lock());

            if (pReferenceWindow && pReferenceWindow != m_window) {
                const Vector2D draggedCenter   = m_window->m_realPosition->goal() + m_window->m_realSize->goal() / 2.f;
                const Vector2D referenceCenter = pReferenceWindow->m_realPosition->goal() + pReferenceWindow->m_realSize->goal() / 2.f;
                const float    xDiff           = draggedCenter.x - referenceCenter.x;
                const float    yDiff           = draggedCenter.y - referenceCenter.y;

                if (fabs(xDiff) > fabs(yDiff))
                    direction = xDiff < 0 ? DIRECTION_LEFT : DIRECTION_RIGHT;
                else
                    direction = yDiff < 0 ? DIRECTION_UP : DIRECTION_DOWN;
            }

            g_pLayoutManager->getCurrentLayout()->onWindowRemovedTiling(m_window.lock());
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedTiling(m_window.lock(), direction);
        } else
            g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(m_window.lock());

        m_window->m_lastFloatingSize = m_draggingWindowOriginalFloatSize;
    }

    g_pHyprRenderer->damageWindow(m_window.lock());
    g_pCompositor->focusWindow(m_window.lock());
}
