#include "InputManager.hpp"
#include "../../Compositor.hpp"

void CInputManager::onTouchDown(wlr_touch_down_event* e) {

    wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, g_pCompositor->m_pLastMonitor->vecPosition.x + e->x * g_pCompositor->m_pLastMonitor->vecSize.x, g_pCompositor->m_pLastMonitor->vecPosition.y + e->y * g_pCompositor->m_pLastMonitor->vecSize.y);

    refocus();

    m_sTouchData.touchFocusWindow = nullptr;

    if (g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow)) {
        wlr_seat_touch_notify_down(g_pCompositor->m_sSeat.seat, g_pCompositor->m_pLastFocus, e->time_msec, e->touch_id,
            e->x * g_pCompositor->m_pLastMonitor->vecSize.x + g_pCompositor->m_pLastMonitor->vecPosition.x - g_pCompositor->m_pLastWindow->m_vRealPosition.vec().x,
            e->y * g_pCompositor->m_pLastMonitor->vecSize.y + g_pCompositor->m_pLastMonitor->vecPosition.y - g_pCompositor->m_pLastWindow->m_vRealPosition.vec().y);

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

        wlr_seat_touch_notify_motion(g_pCompositor->m_sSeat.seat, e->time_msec, e->touch_id,
            e->x * PMONITOR->vecSize.x + PMONITOR->vecPosition.x - m_sTouchData.touchFocusWindow->m_vRealPosition.vec().x,
            e->y * PMONITOR->vecSize.y + PMONITOR->vecPosition.y - m_sTouchData.touchFocusWindow->m_vRealPosition.vec().y);

        wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, PMONITOR->vecPosition.x + e->x * PMONITOR->vecSize.x, PMONITOR->vecPosition.y + e->y * PMONITOR->vecSize.y);
    }
}