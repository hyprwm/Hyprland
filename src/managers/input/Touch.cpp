#include "InputManager.hpp"
#include "../../Compositor.hpp"
#include "../../config/ConfigValue.hpp"

void CInputManager::onTouchDown(wlr_touch_down_event* e) {
    static auto PSWIPETOUCH  = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_touch");
    static auto PGAPSOUTDATA = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_out");
    auto* const PGAPSOUT     = (CCssGapData*)(PGAPSOUTDATA.ptr())->getData();
    // TODO: WORKSPACERULE.gapsOut.value_or()
    auto        gapsOut     = *PGAPSOUT;
    static auto PBORDERSIZE = CConfigValue<Hyprlang::INT>("general:border_size");
    static auto PSWIPEINVR  = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_invert");
    EMIT_HOOK_EVENT_CANCELLABLE("touchDown", e);

    auto       PMONITOR = g_pCompositor->getMonitorFromName(e->touch->output_name ? e->touch->output_name : "");

    const auto PDEVIT = std::find_if(m_lTouchDevices.begin(), m_lTouchDevices.end(), [&](const STouchDevice& other) { return other.pWlrDevice == &e->touch->base; });

    if (PDEVIT != m_lTouchDevices.end() && !PDEVIT->boundOutput.empty())
        PMONITOR = g_pCompositor->getMonitorFromName(PDEVIT->boundOutput);

    PMONITOR = PMONITOR ? PMONITOR : g_pCompositor->m_pLastMonitor;

    wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, PMONITOR->vecPosition.x + e->x * PMONITOR->vecSize.x, PMONITOR->vecPosition.y + e->y * PMONITOR->vecSize.y);

    refocus();

    if (m_ecbClickBehavior == CLICKMODE_KILL) {
        wlr_pointer_button_event e;
        e.state = WL_POINTER_BUTTON_STATE_PRESSED;
        g_pInputManager->processMouseDownKill(&e);
        return;
    }

    // Don't propagate new touches when a workspace swipe is in progress.
    if (m_sActiveSwipe.pWorkspaceBegin) {
        return;
        // TODO: Don't swipe if you touched a floating window.
    } else if (*PSWIPETOUCH && (!m_pFoundLSToFocus || m_pFoundLSToFocus->layer <= ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM)) {
        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);
        const bool VERTANIMS  = PWORKSPACE->m_vRenderOffset.getConfig()->pValues->internalStyle == "slidevert" ||
            PWORKSPACE->m_vRenderOffset.getConfig()->pValues->internalStyle.starts_with("slidefadevert");
        // TODO: support no_gaps_when_only?
        const double TARGETLEFT  = ((VERTANIMS ? gapsOut.top : gapsOut.left) + *PBORDERSIZE) / (VERTANIMS ? PMONITOR->vecSize.y : PMONITOR->vecSize.x);
        const double TARGETRIGHT = 1 - (((VERTANIMS ? gapsOut.bottom : gapsOut.right) + *PBORDERSIZE) / (VERTANIMS ? PMONITOR->vecSize.y : PMONITOR->vecSize.x));
        const double POSITION    = (VERTANIMS ? e->y : e->x);
        if (POSITION < TARGETLEFT || POSITION > TARGETRIGHT) {
            beginWorkspaceSwipe();
            m_sActiveSwipe.touch_id = e->touch_id;
            // Set the initial direction based on which edge you started from
            if (POSITION > 0.5)
                m_sActiveSwipe.initialDirection = *PSWIPEINVR ? -1 : 1;
            else
                m_sActiveSwipe.initialDirection = *PSWIPEINVR ? 1 : -1;
            return;
        }
    }

    m_bLastInputTouch = true;

    m_sTouchData.touchFocusWindow  = m_pFoundWindowToFocus;
    m_sTouchData.touchFocusSurface = m_pFoundSurfaceToFocus;
    m_sTouchData.touchFocusLS      = m_pFoundLSToFocus;

    Vector2D local;

    if (m_sTouchData.touchFocusWindow) {
        if (m_sTouchData.touchFocusWindow->m_bIsX11) {
            local = (g_pInputManager->getMouseCoordsInternal() - m_sTouchData.touchFocusWindow->m_vRealPosition.goal()) * m_sTouchData.touchFocusWindow->m_fX11SurfaceScaledBy;
            m_sTouchData.touchSurfaceOrigin = m_sTouchData.touchFocusWindow->m_vRealPosition.goal();
        } else {
            g_pCompositor->vectorWindowToSurface(g_pInputManager->getMouseCoordsInternal(), m_sTouchData.touchFocusWindow, local);
            m_sTouchData.touchSurfaceOrigin = g_pInputManager->getMouseCoordsInternal() - local;
        }
    } else if (m_sTouchData.touchFocusLS) {
        local = g_pInputManager->getMouseCoordsInternal() - Vector2D(m_sTouchData.touchFocusLS->geometry.x, m_sTouchData.touchFocusLS->geometry.y);

        m_sTouchData.touchSurfaceOrigin = g_pInputManager->getMouseCoordsInternal() - local;
    } else {
        return; // oops, nothing found.
    }

    wlr_seat_touch_notify_down(g_pCompositor->m_sSeat.seat, m_sTouchData.touchFocusSurface, e->time_msec, e->touch_id, local.x, local.y);

    g_pCompositor->notifyIdleActivity();
}

void CInputManager::onTouchUp(wlr_touch_up_event* e) {
    EMIT_HOOK_EVENT_CANCELLABLE("touchUp", e);
    if (m_sActiveSwipe.pWorkspaceBegin) {
        // If there was a swipe from this finger, end it.
        if (e->touch_id == m_sActiveSwipe.touch_id)
            endWorkspaceSwipe();
        return;
    }

    if (m_sTouchData.touchFocusSurface) {
        wlr_seat_touch_notify_up(g_pCompositor->m_sSeat.seat, e->time_msec, e->touch_id);
    }
}

void CInputManager::onTouchMove(wlr_touch_motion_event* e) {
    EMIT_HOOK_EVENT_CANCELLABLE("touchMove", e);
    if (m_sActiveSwipe.pWorkspaceBegin) {
        // Do nothing if this is using a different finger.
        if (e->touch_id != m_sActiveSwipe.touch_id)
            return;
        const bool VERTANIMS = m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.getConfig()->pValues->internalStyle == "slidevert" ||
            m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.getConfig()->pValues->internalStyle.starts_with("slidefadevert");
        static auto PSWIPEINVR = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_invert");
        static auto PSWIPEDIST = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_distance");
        // Handle the workspace swipe if there is one
        if (m_sActiveSwipe.initialDirection == -1) {
            if (*PSWIPEINVR)
                // go from 0 to -PSWIPEDIST
                updateWorkspaceSwipe(*PSWIPEDIST * ((VERTANIMS ? e->y : e->x) - 1));
            else
                // go from 0 to -PSWIPEDIST
                updateWorkspaceSwipe(*PSWIPEDIST * (-1 * (VERTANIMS ? e->y : e->x)));
        } else if (*PSWIPEINVR)
            // go from 0 to PSWIPEDIST
            updateWorkspaceSwipe(*PSWIPEDIST * (VERTANIMS ? e->y : e->x));
        else
            // go from 0 to PSWIPEDIST
            updateWorkspaceSwipe(*PSWIPEDIST * (1 - (VERTANIMS ? e->y : e->x)));
        return;
    }
    if (m_sTouchData.touchFocusWindow && g_pCompositor->windowValidMapped(m_sTouchData.touchFocusWindow)) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(m_sTouchData.touchFocusWindow->m_iMonitorID);

        wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, PMONITOR->vecPosition.x + e->x * PMONITOR->vecSize.x, PMONITOR->vecPosition.y + e->y * PMONITOR->vecSize.y);

        auto local = g_pInputManager->getMouseCoordsInternal() - m_sTouchData.touchSurfaceOrigin;
        if (m_sTouchData.touchFocusWindow->m_bIsX11)
            local = local * m_sTouchData.touchFocusWindow->m_fX11SurfaceScaledBy;

        wlr_seat_touch_notify_motion(g_pCompositor->m_sSeat.seat, e->time_msec, e->touch_id, local.x, local.y);
        // wlr_seat_pointer_notify_motion(g_pCompositor->m_sSeat.seat, e->time_msec, local.x, local.y);
    } else if (m_sTouchData.touchFocusLS) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(m_sTouchData.touchFocusLS->monitorID);

        wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, PMONITOR->vecPosition.x + e->x * PMONITOR->vecSize.x, PMONITOR->vecPosition.y + e->y * PMONITOR->vecSize.y);

        const auto local = g_pInputManager->getMouseCoordsInternal() - m_sTouchData.touchSurfaceOrigin;

        wlr_seat_touch_notify_motion(g_pCompositor->m_sSeat.seat, e->time_msec, e->touch_id, local.x, local.y);
        // wlr_seat_pointer_notify_motion(g_pCompositor->m_sSeat.seat, e->time_msec, local.x, local.y);
    }
}

void CInputManager::onPointerHoldBegin(wlr_pointer_hold_begin_event* e) {
    wlr_pointer_gestures_v1_send_hold_begin(g_pCompositor->m_sWLRPointerGestures, g_pCompositor->m_sSeat.seat, e->time_msec, e->fingers);
}

void CInputManager::onPointerHoldEnd(wlr_pointer_hold_end_event* e) {
    wlr_pointer_gestures_v1_send_hold_end(g_pCompositor->m_sWLRPointerGestures, g_pCompositor->m_sSeat.seat, e->time_msec, e->cancelled);
}
