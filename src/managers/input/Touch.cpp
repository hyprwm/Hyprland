#include "InputManager.hpp"
#include "../SessionLockManager.hpp"
#include "../../protocols/SessionLock.hpp"
#include "../../Compositor.hpp"
#include "../../desktop/view/LayerSurface.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../output/Monitor.hpp"
#include "../../state/MonitorState.hpp"
#include "../../devices/ITouch.hpp"
#include "../../event/EventBus.hpp"
#include "../SeatManager.hpp"
#include "../../protocols/core/DataDevice.hpp"
#include "debug/log/Logger.hpp"
#include "UnifiedWorkspaceSwipeGesture.hpp"

void CInputManager::onTouchDown(ITouch::SDownEvent e) {
    m_lastInputTouch = true;

    static auto PSWIPETOUCH  = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_touch");
    static auto PGAPSOUTDATA = CConfigValue<Config::IComplexConfigValue>("general:gaps_out");
    auto* const PGAPSOUT     = sc<Config::CCssGapData*>((PGAPSOUTDATA.ptr()));
    // TODO: WORKSPACERULE.gapsOut.value_or()
    auto                 gapsOut     = *PGAPSOUT;
    static auto          PBORDERSIZE = CConfigValue<Config::INTEGER>("general:border_size");
    static auto          PSWIPEINVR  = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_touch_invert");

    Event::SCallbackInfo info;
    Event::bus()->m_events.input.touch.down.emit(e, info);
    if (info.cancelled)
        return;

    if (!e.device)
        return;

    std::erase_if(m_touchData.consumedTouches, [](const auto& touch) { return touch.device.expired(); });

    // Keep every suppressed finger consumed through its up or hardware cancel, even if the swipe is cancelled internally.
    if (m_touchData.workspaceSwipe || !m_touchData.consumedTouches.empty() || g_pUnifiedWorkspaceSwipe->isGestureInProgress()) {
        if (std::ranges::none_of(m_touchData.consumedTouches, [&e](const auto& touch) { return touch.device == e.device && touch.id == e.touchID; }))
            m_touchData.consumedTouches.emplace_back(STouchData::SConsumedTouch{
                .device = e.device,
                .id     = e.touchID,
            });
        return;
    }

    auto PMONITOR = State::monitorState()->query().name(!e.device->m_boundOutput.empty() ? e.device->m_boundOutput : "").run();

    PMONITOR = PMONITOR ? PMONITOR : Desktop::focusState()->monitor();

    if (!PMONITOR || !PMONITOR->m_activeWorkspace || PMONITOR->m_size.x <= 0 || PMONITOR->m_size.y <= 0)
        return;

    if (PMONITOR != Desktop::focusState()->monitor())
        Desktop::focusState()->rawMonitorFocus(PMONITOR);

    const auto TOUCH_COORDS = PMONITOR->m_position + (e.pos * PMONITOR->m_size);

    m_touchData.lastTouchPos = TOUCH_COORDS;

    refocus(TOUCH_COORDS);

    if (m_clickBehavior == CLICKMODE_KILL) {
        IPointer::SButtonEvent e;
        e.state = WL_POINTER_BUTTON_STATE_PRESSED;
        g_pInputManager->processMouseDownKill(e);
        return;
    }

    // TODO: Don't swipe if you touched a floating window.
    if (*PSWIPETOUCH && (m_foundLSToFocus.expired() || m_foundLSToFocus->m_layer <= 1) && !g_pSessionLockManager->isSessionLocked()) {
        const auto   PWORKSPACE  = PMONITOR->m_activeWorkspace;
        const auto   STYLE       = PWORKSPACE->m_renderOffset->getStyle();
        const bool   VERTANIMS   = STYLE == "slidevert" || STYLE.starts_with("slidefadevert");
        const double TARGETLEFT  = ((VERTANIMS ? gapsOut.m_top : gapsOut.m_left) + *PBORDERSIZE) / (VERTANIMS ? PMONITOR->m_size.y : PMONITOR->m_size.x);
        const double TARGETRIGHT = 1 - (((VERTANIMS ? gapsOut.m_bottom : gapsOut.m_right) + *PBORDERSIZE) / (VERTANIMS ? PMONITOR->m_size.y : PMONITOR->m_size.x));
        const double POSITION    = (VERTANIMS ? e.pos.y : e.pos.x);
        if ((POSITION < TARGETLEFT || POSITION > TARGETRIGHT) && g_pUnifiedWorkspaceSwipe->begin(PMONITOR)) {
            auto& swipe         = m_touchData.workspaceSwipe.emplace();
            swipe.device        = e.device;
            swipe.id            = e.touchID;
            swipe.sessionID     = g_pUnifiedWorkspaceSwipe->sessionID();
            swipe.monitor       = PMONITOR;
            swipe.fromEnd       = POSITION > 0.5;
            swipe.deviceDestroy = e.device->m_events.destroy.listen([this, device = WP<ITouch>(e.device)] {
                std::erase_if(m_touchData.consumedTouches, [&device](const auto& touch) { return touch.device == device; });
                if (!m_touchData.workspaceSwipe || m_touchData.workspaceSwipe->device != device)
                    return;

                const auto SESSION = m_touchData.workspaceSwipe->sessionID;
                m_touchData.workspaceSwipe.reset();
                if (g_pUnifiedWorkspaceSwipe && SESSION == g_pUnifiedWorkspaceSwipe->sessionID())
                    g_pUnifiedWorkspaceSwipe->cancel();
            });
            m_touchData.consumedTouches.emplace_back(STouchData::SConsumedTouch{
                .device = e.device,
                .id     = e.touchID,
            });
            // Direction locking belongs to the unified gesture; the physical edge survives forever restarts.
            g_pUnifiedWorkspaceSwipe->m_initialDirection = (swipe.fromEnd ? 1 : -1) * (*PSWIPEINVR ? -1 : 1);
            return;
        }
    }

    // could have abovelock surface, thus only use lock if no ls found
    if (g_pSessionLockManager->isSessionLocked() && m_foundLSToFocus.expired()) {
        m_touchData.touchFocusLockSurface = g_pSessionLockManager->getSessionLockSurfaceForMonitor(PMONITOR->m_id);
        if (!m_touchData.touchFocusLockSurface)
            LOG(Log::WARN, "The session is locked but can't find a lock surface");
        else
            m_touchData.touchFocusSurface = m_touchData.touchFocusLockSurface->surface->surface();
    } else {
        m_touchData.touchFocusLockSurface.reset();
        m_touchData.touchFocusWindow  = m_foundWindowToFocus;
        m_touchData.touchFocusSurface = m_foundSurfaceToFocus;
        m_touchData.touchFocusLS      = m_foundLSToFocus;
    }

    Vector2D local;

    if (m_touchData.touchFocusLockSurface) {
        local                          = TOUCH_COORDS - PMONITOR->m_position;
        m_touchData.touchSurfaceOrigin = TOUCH_COORDS - local;
    } else if (!m_touchData.touchFocusWindow.expired()) {
        if (m_touchData.touchFocusWindow->backend().isX11()) {
            local = m_touchData.touchFocusWindow->backend().surfaceLocalToBuffer(TOUCH_COORDS - m_touchData.touchFocusWindow->position(Desktop::View::IGeometric::GEOMETRIC_GOAL));
            m_touchData.touchSurfaceOrigin = m_touchData.touchFocusWindow->position(Desktop::View::IGeometric::GEOMETRIC_GOAL);
        } else {
            Desktop::viewState()->hitTest().windowSurfaceAt(TOUCH_COORDS, m_touchData.touchFocusWindow.lock(), local);
            m_touchData.touchSurfaceOrigin = TOUCH_COORDS - local;
        }
    } else if (!m_touchData.touchFocusLS.expired()) {
        PHLLS    foundSurf;
        Vector2D foundCoords;
        auto     surf = Desktop::viewState()->hitTest().layerPopupSurfaceAt(TOUCH_COORDS, PMONITOR, &foundCoords, &foundSurf);
        if (surf) {
            local                         = foundCoords;
            m_touchData.touchFocusSurface = surf;
        } else
            local = TOUCH_COORDS - m_touchData.touchFocusLS->m_geometry.pos();

        m_touchData.touchSurfaceOrigin = TOUCH_COORDS - local;
    } else
        return; // oops, nothing found.

    g_pSeatManager->sendTouchDown(m_touchData.touchFocusSurface.lock(), e.timeMs, e.touchID, local);
}

void CInputManager::onTouchUp(ITouch::SUpEvent e, SP<ITouch> device) {
    m_lastInputTouch = true;

    Event::SCallbackInfo info;
    Event::bus()->m_events.input.touch.up.emit(e, info);
    const auto CONSUMED = std::erase_if(m_touchData.consumedTouches, [&e, &device](const auto& touch) { return touch.device == device && touch.id == e.touchID; });
    if (CONSUMED) {
        if (m_touchData.workspaceSwipe && m_touchData.workspaceSwipe->device == device && m_touchData.workspaceSwipe->id == e.touchID) {
            const auto SESSION = m_touchData.workspaceSwipe->sessionID;
            m_touchData.workspaceSwipe.reset();
            if (SESSION == g_pUnifiedWorkspaceSwipe->sessionID()) {
                if (info.cancelled)
                    g_pUnifiedWorkspaceSwipe->cancel();
                else
                    g_pUnifiedWorkspaceSwipe->end();
            }
        }
        return;
    }

    if (info.cancelled)
        return;

    if (m_touchData.touchFocusSurface)
        g_pSeatManager->sendTouchUp(e.timeMs, e.touchID);
}

void CInputManager::onTouchCancel(ITouch::SCancelEvent e, SP<ITouch> device) {
    if (!device)
        return;

    // Aquamarine forwards libinput's seat slot for cancel, so this terminates one contact, just like up.
    std::erase_if(m_touchData.consumedTouches, [&e, &device](const auto& touch) { return touch.device == device && touch.id == e.touchID; });
    if (!m_touchData.workspaceSwipe || m_touchData.workspaceSwipe->device != device || m_touchData.workspaceSwipe->id != e.touchID)
        return;

    const auto SESSION = m_touchData.workspaceSwipe->sessionID;
    m_touchData.workspaceSwipe.reset();
    if (SESSION && g_pUnifiedWorkspaceSwipe && SESSION == g_pUnifiedWorkspaceSwipe->sessionID())
        g_pUnifiedWorkspaceSwipe->cancel();
}

void CInputManager::onTouchMove(ITouch::SMotionEvent e, SP<ITouch> device) {
    m_lastInputTouch = true;

    m_lastCursorMovement.reset();

    // Cache the global touch position so listeners (in particular the dnd
    // touchMove listener emitted just below) and renderers can resolve where
    // the finger currently is in layout coordinates.
    const bool SWIPE_FINGER = m_touchData.workspaceSwipe && m_touchData.workspaceSwipe->device == device && m_touchData.workspaceSwipe->id == e.touchID;
    const auto PMONITOR     = SWIPE_FINGER ? m_touchData.workspaceSwipe->monitor.lock() : Desktop::focusState()->monitor();
    if (PMONITOR)
        m_touchData.lastTouchPos = PMONITOR->m_position + (e.pos * PMONITOR->m_size);

    Event::SCallbackInfo info;
    Event::bus()->m_events.input.touch.motion.emit(e, info);
    if (info.cancelled)
        return;

    if (std::ranges::any_of(m_touchData.consumedTouches, [&e, &device](const auto& touch) { return touch.device == device && touch.id == e.touchID; })) {
        if (!m_touchData.workspaceSwipe || m_touchData.workspaceSwipe->device != device || m_touchData.workspaceSwipe->id != e.touchID ||
            m_touchData.workspaceSwipe->sessionID != g_pUnifiedWorkspaceSwipe->sessionID())
            return;

        const auto   ANIMSTYLE     = g_pUnifiedWorkspaceSwipe->m_workspaceBegin->m_renderOffset->getStyle();
        const bool   VERTANIMS     = ANIMSTYLE == "slidevert" || ANIMSTYLE.starts_with("slidefadevert");
        static auto  PSWIPEINVR    = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_touch_invert");
        static auto  PSWIPEDIST    = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_distance");
        const auto   SWIPEDISTANCE = std::clamp(*PSWIPEDIST, sc<int64_t>(1LL), sc<int64_t>(UINT32_MAX));
        const double POSITION      = VERTANIMS ? e.pos.y : e.pos.x;
        const double DELTA         = m_touchData.workspaceSwipe->fromEnd ? 1 - POSITION : -POSITION;
        g_pUnifiedWorkspaceSwipe->update(SWIPEDISTANCE * DELTA * (*PSWIPEINVR ? -1 : 1));
        return;
    }
    // During a drag-and-drop session, repick the surface under the finger so
    // wl_data_device enter/leave/offer follow the touch point, the same way
    // cursor motion drives pointer focus during mouse drags. Touch events are
    // not delivered to surfaces during the drag (mouse drags work likewise).
    if (PROTO::data->dndActive()) {
        refocus(m_touchData.lastTouchPos);
        return;
    }
    if (m_touchData.touchFocusLockSurface) {
        const auto PMONITOR     = State::monitorState()->query().id(m_touchData.touchFocusLockSurface->iMonitorID).run();
        const auto TOUCH_COORDS = PMONITOR->m_position + (e.pos * PMONITOR->m_size);
        const auto LOCAL        = TOUCH_COORDS - PMONITOR->m_position;
        g_pSeatManager->sendTouchMotion(e.timeMs, e.touchID, LOCAL);
    } else if (validMapped(m_touchData.touchFocusWindow)) {
        const auto PMONITOR     = m_touchData.touchFocusWindow->m_monitor.lock();
        const auto TOUCH_COORDS = PMONITOR->m_position + (e.pos * PMONITOR->m_size);
        auto       local        = TOUCH_COORDS - m_touchData.touchSurfaceOrigin;
        if (m_touchData.touchFocusWindow->backend().isX11())
            local = m_touchData.touchFocusWindow->backend().surfaceLocalToBuffer(local);

        g_pSeatManager->sendTouchMotion(e.timeMs, e.touchID, local);
    } else if (validMapped(m_touchData.touchFocusLS)) {
        const auto PMONITOR     = m_touchData.touchFocusLS->m_monitor.lock();
        const auto TOUCH_COORDS = PMONITOR->m_position + (e.pos * PMONITOR->m_size);
        const auto LOCAL        = TOUCH_COORDS - m_touchData.touchSurfaceOrigin;

        g_pSeatManager->sendTouchMotion(e.timeMs, e.touchID, LOCAL);
    }
}
