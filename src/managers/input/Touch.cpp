#include "InputManager.hpp"
#include "../../Compositor.hpp"

void CInputManager::onTouchDown(wlr_touch_down_event* e) {
    auto PMONITOR = g_pCompositor->getMonitorFromName(e->touch->output_name ? e->touch->output_name : "");
    PMONITOR = PMONITOR ? PMONITOR : g_pCompositor->m_pLastMonitor;

    wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, PMONITOR->vecPosition.x + e->x * PMONITOR->vecSize.x, PMONITOR->vecPosition.y + e->y * PMONITOR->vecSize.y);

    refocus();

    m_sTouchData.touchFocusWindow = nullptr;

    if (g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow)) {
        Vector2D local;
        if (g_pCompositor->m_pLastWindow->m_bIsX11) {
            local = g_pInputManager->getMouseCoordsInternal() - g_pCompositor->m_pLastWindow->m_vRealPosition.goalv();
        } else {
            g_pCompositor->vectorWindowToSurface(g_pInputManager->getMouseCoordsInternal(), g_pCompositor->m_pLastWindow, local);
        }

        m_sTouchData.touchSurfaceOrigin = g_pInputManager->getMouseCoordsInternal() - local;

        wlr_seat_touch_notify_down(g_pCompositor->m_sSeat.seat, g_pCompositor->m_pLastFocus, e->time_msec, e->touch_id, local.x, local.y);

        m_sTouchData.touchFocusWindow = g_pCompositor->m_pLastWindow;
    }
}

void CInputManager::onTouchUp(wlr_touch_up_event* e){
    if (m_sTouchData.touchFocusWindow) {
        wlr_seat_touch_notify_up(g_pCompositor->m_sSeat.seat, e->time_msec, e->touch_id);
    }
}

void CInputManager::onTouchMove(wlr_touch_motion_event* e){
    if (g_pCompositor->windowValidMapped(m_sTouchData.touchFocusWindow)) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(m_sTouchData.touchFocusWindow->m_iMonitorID);

        wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, PMONITOR->vecPosition.x + e->x * PMONITOR->vecSize.x, PMONITOR->vecPosition.y + e->y * PMONITOR->vecSize.y);

        const auto local = g_pInputManager->getMouseCoordsInternal() - m_sTouchData.touchSurfaceOrigin;

        wlr_seat_touch_notify_motion(g_pCompositor->m_sSeat.seat, e->time_msec, e->touch_id, local.x, local.y);
    }
}

void CInputManager::onPointerHoldBegin(wlr_pointer_hold_begin_event* e) {
    wlr_pointer_gestures_v1_send_hold_begin(g_pCompositor->m_sWLRPointerGestures, g_pCompositor->m_sSeat.seat, e->time_msec, e->fingers);
}

void CInputManager::onPointerHoldEnd(wlr_pointer_hold_end_event* e) {
    wlr_pointer_gestures_v1_send_hold_end(g_pCompositor->m_sWLRPointerGestures, g_pCompositor->m_sSeat.seat, e->time_msec, e->cancelled);
}
