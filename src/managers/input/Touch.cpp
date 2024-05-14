#include "InputManager.hpp"
#include "../../Compositor.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../protocols/IdleNotify.hpp"
#include "../../devices/ITouch.hpp"
#include "../SeatManager.hpp"

void CInputManager::onTouchDown(ITouch::SDownEvent e) {
    static auto PSWIPETOUCH  = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_touch");
    static auto PGAPSOUTDATA = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_out");
    auto* const PGAPSOUT     = (CCssGapData*)(PGAPSOUTDATA.ptr())->getData();
    // TODO: WORKSPACERULE.gapsOut.value_or()
    auto        gapsOut     = *PGAPSOUT;
    static auto PBORDERSIZE = CConfigValue<Hyprlang::INT>("general:border_size");
    static auto PSWIPEINVR  = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_invert");
    EMIT_HOOK_EVENT_CANCELLABLE("touchDown", e);

    auto PMONITOR = g_pCompositor->getMonitorFromName(!e.device->boundOutput.empty() ? e.device->boundOutput : "");

    PMONITOR = PMONITOR ? PMONITOR : g_pCompositor->m_pLastMonitor.get();

    g_pCompositor->warpCursorTo({PMONITOR->vecPosition.x + e.pos.x * PMONITOR->vecSize.x, PMONITOR->vecPosition.y + e.pos.y * PMONITOR->vecSize.y}, true);

    refocus();

    if (m_ecbClickBehavior == CLICKMODE_KILL) {
        IPointer::SButtonEvent e;
        e.state = WL_POINTER_BUTTON_STATE_PRESSED;
        g_pInputManager->processMouseDownKill(e);
        return;
    }

    // Don't propagate new touches when a workspace swipe is in progress.
    if (m_sActiveSwipe.pWorkspaceBegin) {
        return;
        // TODO: Don't swipe if you touched a floating window.
    } else if (*PSWIPETOUCH && (m_pFoundLSToFocus.expired() || m_pFoundLSToFocus->layer <= 1)) {
        const auto PWORKSPACE = PMONITOR->activeWorkspace;
        const bool VERTANIMS  = PWORKSPACE->m_vRenderOffset.getConfig()->pValues->internalStyle == "slidevert" ||
            PWORKSPACE->m_vRenderOffset.getConfig()->pValues->internalStyle.starts_with("slidefadevert");
        // TODO: support no_gaps_when_only?
        const double TARGETLEFT  = ((VERTANIMS ? gapsOut.top : gapsOut.left) + *PBORDERSIZE) / (VERTANIMS ? PMONITOR->vecSize.y : PMONITOR->vecSize.x);
        const double TARGETRIGHT = 1 - (((VERTANIMS ? gapsOut.bottom : gapsOut.right) + *PBORDERSIZE) / (VERTANIMS ? PMONITOR->vecSize.y : PMONITOR->vecSize.x));
        const double POSITION    = (VERTANIMS ? e.pos.y : e.pos.x);
        if (POSITION < TARGETLEFT || POSITION > TARGETRIGHT) {
            beginWorkspaceSwipe();
            m_sActiveSwipe.touch_id = e.touchID;
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

    if (!m_sTouchData.touchFocusWindow.expired()) {
        if (m_sTouchData.touchFocusWindow->m_bIsX11) {
            local = (g_pInputManager->getMouseCoordsInternal() - m_sTouchData.touchFocusWindow->m_vRealPosition.goal()) * m_sTouchData.touchFocusWindow->m_fX11SurfaceScaledBy;
            m_sTouchData.touchSurfaceOrigin = m_sTouchData.touchFocusWindow->m_vRealPosition.goal();
        } else {
            g_pCompositor->vectorWindowToSurface(g_pInputManager->getMouseCoordsInternal(), m_sTouchData.touchFocusWindow.lock(), local);
            m_sTouchData.touchSurfaceOrigin = g_pInputManager->getMouseCoordsInternal() - local;
        }
    } else if (!m_sTouchData.touchFocusLS.expired()) {
        local = g_pInputManager->getMouseCoordsInternal() - m_sTouchData.touchFocusLS->geometry.pos();

        m_sTouchData.touchSurfaceOrigin = g_pInputManager->getMouseCoordsInternal() - local;
    } else
        return; // oops, nothing found.

    g_pSeatManager->sendTouchDown(m_sTouchData.touchFocusSurface, e.timeMs, e.touchID, local);

    PROTO::idle->onActivity();
}

void CInputManager::onTouchUp(ITouch::SUpEvent e) {
    EMIT_HOOK_EVENT_CANCELLABLE("touchUp", e);
    if (m_sActiveSwipe.pWorkspaceBegin) {
        // If there was a swipe from this finger, end it.
        if (e.touchID == m_sActiveSwipe.touch_id)
            endWorkspaceSwipe();
        return;
    }

    if (m_sTouchData.touchFocusSurface)
        g_pSeatManager->sendTouchUp(e.timeMs, e.touchID);
}

void CInputManager::onTouchMove(ITouch::SMotionEvent e) {
    EMIT_HOOK_EVENT_CANCELLABLE("touchMove", e);
    if (m_sActiveSwipe.pWorkspaceBegin) {
        // Do nothing if this is using a different finger.
        if (e.touchID != m_sActiveSwipe.touch_id)
            return;
        const bool VERTANIMS = m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.getConfig()->pValues->internalStyle == "slidevert" ||
            m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.getConfig()->pValues->internalStyle.starts_with("slidefadevert");
        static auto PSWIPEINVR    = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_invert");
        static auto PSWIPEDIST    = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_distance");
        const auto  SWIPEDISTANCE = std::clamp(*PSWIPEDIST, (int64_t)1LL, (int64_t)UINT32_MAX);
        // Handle the workspace swipe if there is one
        if (m_sActiveSwipe.initialDirection == -1) {
            if (*PSWIPEINVR)
                // go from 0 to -SWIPEDISTANCE
                updateWorkspaceSwipe(SWIPEDISTANCE * ((VERTANIMS ? e.pos.y : e.pos.x) - 1));
            else
                // go from 0 to -SWIPEDISTANCE
                updateWorkspaceSwipe(SWIPEDISTANCE * (-1 * (VERTANIMS ? e.pos.y : e.pos.x)));
        } else if (*PSWIPEINVR)
            // go from 0 to SWIPEDISTANCE
            updateWorkspaceSwipe(SWIPEDISTANCE * (VERTANIMS ? e.pos.y : e.pos.x));
        else
            // go from 0 to SWIPEDISTANCE
            updateWorkspaceSwipe(SWIPEDISTANCE * (1 - (VERTANIMS ? e.pos.y : e.pos.x)));
        return;
    }
    if (validMapped(m_sTouchData.touchFocusWindow)) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(m_sTouchData.touchFocusWindow->m_iMonitorID);

        g_pCompositor->warpCursorTo({PMONITOR->vecPosition.x + e.pos.x * PMONITOR->vecSize.x, PMONITOR->vecPosition.y + e.pos.y * PMONITOR->vecSize.y}, true);

        auto local = g_pInputManager->getMouseCoordsInternal() - m_sTouchData.touchSurfaceOrigin;
        if (m_sTouchData.touchFocusWindow->m_bIsX11)
            local = local * m_sTouchData.touchFocusWindow->m_fX11SurfaceScaledBy;

        g_pSeatManager->sendTouchMotion(e.timeMs, e.touchID, local);
    } else if (!m_sTouchData.touchFocusLS.expired()) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(m_sTouchData.touchFocusLS->monitorID);

        g_pCompositor->warpCursorTo({PMONITOR->vecPosition.x + e.pos.x * PMONITOR->vecSize.x, PMONITOR->vecPosition.y + e.pos.y * PMONITOR->vecSize.y}, true);

        const auto local = g_pInputManager->getMouseCoordsInternal() - m_sTouchData.touchSurfaceOrigin;

        g_pSeatManager->sendTouchMotion(e.timeMs, e.touchID, local);
    }
}
