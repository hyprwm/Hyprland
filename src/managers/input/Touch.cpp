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

    auto PMONITOR = State::monitorState()->query().name(!e.device->m_boundOutput.empty() ? e.device->m_boundOutput : "").run();

    PMONITOR = PMONITOR ? PMONITOR : Desktop::focusState()->monitor();

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

    // Don't propagate new touches when a workspace swipe is in progress.
    if (g_pUnifiedWorkspaceSwipe->isGestureInProgress()) {
        return;
        // TODO: Don't swipe if you touched a floating window.
    } else if (*PSWIPETOUCH && (m_foundLSToFocus.expired() || m_foundLSToFocus->m_layer <= 1) && !g_pSessionLockManager->isSessionLocked()) {
        const auto   PWORKSPACE  = PMONITOR->m_activeWorkspace;
        const auto   STYLE       = PWORKSPACE->m_renderOffset->getStyle();
        const bool   VERTANIMS   = STYLE == "slidevert" || STYLE.starts_with("slidefadevert");
        const double TARGETLEFT  = ((VERTANIMS ? gapsOut.m_top : gapsOut.m_left) + *PBORDERSIZE) / (VERTANIMS ? PMONITOR->m_size.y : PMONITOR->m_size.x);
        const double TARGETRIGHT = 1 - (((VERTANIMS ? gapsOut.m_bottom : gapsOut.m_right) + *PBORDERSIZE) / (VERTANIMS ? PMONITOR->m_size.y : PMONITOR->m_size.x));
        const double POSITION    = (VERTANIMS ? e.pos.y : e.pos.x);
        if (POSITION < TARGETLEFT || POSITION > TARGETRIGHT) {
            g_pUnifiedWorkspaceSwipe->begin();
            g_pUnifiedWorkspaceSwipe->m_touchID = e.touchID;
            // Set the initial direction based on which edge you started from
            if (POSITION > 0.5)
                g_pUnifiedWorkspaceSwipe->m_initialDirection = *PSWIPEINVR ? -1 : 1;
            else
                g_pUnifiedWorkspaceSwipe->m_initialDirection = *PSWIPEINVR ? 1 : -1;
            return;
        }
    }

    // could have abovelock surface, thus only use lock if no ls found
    if (g_pSessionLockManager->isSessionLocked() && m_foundLSToFocus.expired()) {
        m_touchData.touchFocusLockSurface = g_pSessionLockManager->getSessionLockSurfaceForMonitor(PMONITOR->m_id);
        if (!m_touchData.touchFocusLockSurface)
            Log::logger->log(Log::WARN, "The session is locked but can't find a lock surface");
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
        if (m_touchData.touchFocusWindow->m_isX11) {
            local                          = (TOUCH_COORDS - m_touchData.touchFocusWindow->m_realPosition->goal()) * m_touchData.touchFocusWindow->m_X11SurfaceScaledBy;
            m_touchData.touchSurfaceOrigin = m_touchData.touchFocusWindow->m_realPosition->goal();
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

void CInputManager::onTouchUp(ITouch::SUpEvent e) {
    m_lastInputTouch = true;

    Event::SCallbackInfo info;
    Event::bus()->m_events.input.touch.up.emit(e, info);
    if (info.cancelled)
        return;

    if (g_pUnifiedWorkspaceSwipe->isGestureInProgress()) {
        // If there was a swipe from this finger, end it.
        if (e.touchID == g_pUnifiedWorkspaceSwipe->m_touchID)
            g_pUnifiedWorkspaceSwipe->end();
        return;
    }

    if (m_touchData.touchFocusSurface)
        g_pSeatManager->sendTouchUp(e.timeMs, e.touchID);
}

void CInputManager::onTouchMove(ITouch::SMotionEvent e) {
    m_lastInputTouch = true;

    m_lastCursorMovement.reset();

    // Cache the global touch position so listeners (in particular the dnd
    // touchMove listener emitted just below) and renderers can resolve where
    // the finger currently is in layout coordinates.
    if (const auto PMONITOR = Desktop::focusState()->monitor(); PMONITOR)
        m_touchData.lastTouchPos = PMONITOR->m_position + (e.pos * PMONITOR->m_size);

    Event::SCallbackInfo info;
    Event::bus()->m_events.input.touch.motion.emit(e, info);
    if (info.cancelled)
        return;

    if (g_pUnifiedWorkspaceSwipe->isGestureInProgress()) {
        // Do nothing if this is using a different finger.
        if (e.touchID != g_pUnifiedWorkspaceSwipe->m_touchID)
            return;

        const auto  ANIMSTYLE     = g_pUnifiedWorkspaceSwipe->m_workspaceBegin->m_renderOffset->getStyle();
        const bool  VERTANIMS     = ANIMSTYLE == "slidevert" || ANIMSTYLE.starts_with("slidefadevert");
        static auto PSWIPEINVR    = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_touch_invert");
        static auto PSWIPEDIST    = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_distance");
        const auto  SWIPEDISTANCE = std::clamp(*PSWIPEDIST, sc<int64_t>(1LL), sc<int64_t>(UINT32_MAX));
        // Handle the workspace swipe if there is one
        if (g_pUnifiedWorkspaceSwipe->m_initialDirection == -1) {
            if (*PSWIPEINVR)
                // go from 0 to -SWIPEDISTANCE
                g_pUnifiedWorkspaceSwipe->update(SWIPEDISTANCE * ((VERTANIMS ? e.pos.y : e.pos.x) - 1));
            else
                // go from 0 to -SWIPEDISTANCE
                g_pUnifiedWorkspaceSwipe->update(SWIPEDISTANCE * (-1 * (VERTANIMS ? e.pos.y : e.pos.x)));
        } else if (*PSWIPEINVR)
            // go from 0 to SWIPEDISTANCE
            g_pUnifiedWorkspaceSwipe->update(SWIPEDISTANCE * (VERTANIMS ? e.pos.y : e.pos.x));
        else
            // go from 0 to SWIPEDISTANCE
            g_pUnifiedWorkspaceSwipe->update(SWIPEDISTANCE * (1 - (VERTANIMS ? e.pos.y : e.pos.x)));
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
        if (m_touchData.touchFocusWindow->m_isX11)
            local = local * m_touchData.touchFocusWindow->m_X11SurfaceScaledBy;

        g_pSeatManager->sendTouchMotion(e.timeMs, e.touchID, local);
    } else if (validMapped(m_touchData.touchFocusLS)) {
        const auto PMONITOR     = m_touchData.touchFocusLS->m_monitor.lock();
        const auto TOUCH_COORDS = PMONITOR->m_position + (e.pos * PMONITOR->m_size);
        const auto LOCAL        = TOUCH_COORDS - m_touchData.touchSurfaceOrigin;

        g_pSeatManager->sendTouchMotion(e.timeMs, e.touchID, LOCAL);
    }
}
