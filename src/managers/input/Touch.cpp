#include "InputManager.hpp"
#include "../SessionLockManager.hpp"
#include "../../protocols/SessionLock.hpp"
#include "../../Compositor.hpp"
#include "../../desktop/LayerSurface.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../helpers/Monitor.hpp"
#include "../../devices/ITouch.hpp"
#include "../SeatManager.hpp"
#include "managers/animation/AnimationManager.hpp"
#include "../HookSystemManager.hpp"
#include "debug/Log.hpp"
#include "UnifiedWorkspaceSwipeGesture.hpp"

void CInputManager::onTouchDown(ITouch::SDownEvent e) {
    m_lastInputTouch = true;

    static auto PSWIPETOUCH  = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_touch");
    static auto PGAPSOUTDATA = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_out");
    auto* const PGAPSOUT     = sc<CCssGapData*>((PGAPSOUTDATA.ptr())->getData());
    // TODO: WORKSPACERULE.gapsOut.value_or()
    auto        gapsOut     = *PGAPSOUT;
    static auto PBORDERSIZE = CConfigValue<Hyprlang::INT>("general:border_size");
    static auto PSWIPEINVR  = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_touch_invert");
    EMIT_HOOK_EVENT_CANCELLABLE("touchDown", e);

    auto PMONITOR = g_pCompositor->getMonitorFromName(!e.device->m_boundOutput.empty() ? e.device->m_boundOutput : "");

    PMONITOR = PMONITOR ? PMONITOR : g_pCompositor->m_lastMonitor.lock();

    const auto TOUCH_COORDS = PMONITOR->m_position + (e.pos * PMONITOR->m_size);

    const auto allowKeyboardRefocus = e.device->m_changeFocus;
    mouseMoveUnified(0, true, false, TOUCH_COORDS, allowKeyboardRefocus);

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
            Debug::log(WARN, "The session is locked but can't find a lock surface");
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
            g_pCompositor->vectorWindowToSurface(TOUCH_COORDS, m_touchData.touchFocusWindow.lock(), local);
            m_touchData.touchSurfaceOrigin = TOUCH_COORDS - local;
        }
    } else if (!m_touchData.touchFocusLS.expired()) {
        PHLLS    foundSurf;
        Vector2D foundCoords;
        auto     surf = g_pCompositor->vectorToLayerPopupSurface(TOUCH_COORDS, PMONITOR, &foundCoords, &foundSurf);
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

    EMIT_HOOK_EVENT_CANCELLABLE("touchUp", e);
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

    EMIT_HOOK_EVENT_CANCELLABLE("touchMove", e);
    if (g_pUnifiedWorkspaceSwipe->isGestureInProgress()) {
        // Do nothing if this is using a different finger.
        if (e.touchID != g_pUnifiedWorkspaceSwipe->m_touchID)
            return;

        const auto  ANIMSTYLE     = g_pUnifiedWorkspaceSwipe->m_workspaceBegin->m_renderOffset->getStyle();
        const bool  VERTANIMS     = ANIMSTYLE == "slidevert" || ANIMSTYLE.starts_with("slidefadevert");
        static auto PSWIPEINVR    = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_touch_invert");
        static auto PSWIPEDIST    = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_distance");
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
    if (m_touchData.touchFocusLockSurface) {
        const auto PMONITOR     = g_pCompositor->getMonitorFromID(m_touchData.touchFocusLockSurface->iMonitorID);
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
