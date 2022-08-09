#include "InputManager.hpp"
#include "../../Compositor.hpp"

void CInputManager::onSwipeBegin(wlr_pointer_swipe_begin_event* e) {

    static auto *const PSWIPE = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe")->intValue;
    static auto *const PSWIPEFINGERS = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_fingers")->intValue;

    if (e->fingers != *PSWIPEFINGERS|| *PSWIPE == 0)
        return;

    int onMonitor = 0;
    for (auto& w : g_pCompositor->m_vWorkspaces) {
        if (w->m_iMonitorID == g_pCompositor->m_pLastMonitor->ID) {
            onMonitor++;
        }
    }

    if (onMonitor < 2)
        return; // disallow swiping when there's 1 workspace on a monitor

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(g_pCompositor->m_pLastMonitor->activeWorkspace);

    Debug::log(LOG, "Starting a swipe from %s", PWORKSPACE->m_szName.c_str());

    m_sActiveSwipe.pWorkspaceBegin = PWORKSPACE;
    m_sActiveSwipe.delta = 0;
    m_sActiveSwipe.pMonitor = g_pCompositor->m_pLastMonitor;
    m_sActiveSwipe.avgSpeed = 0;
    m_sActiveSwipe.speedPoints = 0;
}

void CInputManager::onSwipeEnd(wlr_pointer_swipe_end_event* e) {

    if (!m_sActiveSwipe.pWorkspaceBegin)
        return; // no valid swipe

    static auto *const PSWIPEPERC = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_cancel_ratio")->floatValue;
    static auto *const PSWIPEDIST = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_distance")->intValue;
    static auto *const PSWIPEFORC = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_min_speed_to_force")->intValue;

    // commit
    std::string wsname = "";
    auto workspaceIDLeft = getWorkspaceIDFromString("m-1", wsname);
    auto workspaceIDRight = getWorkspaceIDFromString("m+1", wsname);

    const auto PWORKSPACER = g_pCompositor->getWorkspaceByID(workspaceIDRight);
    const auto PWORKSPACEL = g_pCompositor->getWorkspaceByID(workspaceIDLeft);

    const auto RENDEROFFSETMIDDLE = m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.vec();

    if ((abs(m_sActiveSwipe.delta) < *PSWIPEDIST * *PSWIPEPERC && (*PSWIPEFORC == 0 || (*PSWIPEFORC != 0 && m_sActiveSwipe.avgSpeed < *PSWIPEFORC))) || abs(m_sActiveSwipe.delta) < 2) {
        // revert
        if (abs(m_sActiveSwipe.delta) < 2) {
            PWORKSPACEL->m_vRenderOffset.setValueAndWarp(Vector2D(0,0));
            PWORKSPACER->m_vRenderOffset.setValueAndWarp(Vector2D(0,0));
            m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.setValueAndWarp(Vector2D(0,0));
        } else {
            if (m_sActiveSwipe.delta < 0) {
                // to left
                PWORKSPACEL->m_vRenderOffset = Vector2D({-m_sActiveSwipe.pMonitor->vecSize.x, 0});
            } else {
                // to right
                PWORKSPACER->m_vRenderOffset = Vector2D({m_sActiveSwipe.pMonitor->vecSize.x, 0});
            }

            m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset = Vector2D();
        }
    } else if (m_sActiveSwipe.delta < 0) {
        // switch to left
        const auto RENDEROFFSET = PWORKSPACEL->m_vRenderOffset.vec();

        g_pKeybindManager->m_mDispatchers["workspace"]("[internal]" + std::to_string(workspaceIDLeft));

        PWORKSPACEL->m_vRenderOffset.setValue(RENDEROFFSET);
        PWORKSPACEL->m_fAlpha.setValueAndWarp(255.f);

        m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.setValue(RENDEROFFSETMIDDLE);
        m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset = Vector2D(m_sActiveSwipe.pMonitor->vecSize.x, 0);
        m_sActiveSwipe.pWorkspaceBegin->m_fAlpha.setValueAndWarp(255.f);

        g_pInputManager->unconstrainMouse();

        Debug::log(LOG, "Ended swipe to the left");
    } else {
        // switch to right
        const auto RENDEROFFSET = PWORKSPACER->m_vRenderOffset.vec();

        g_pKeybindManager->m_mDispatchers["workspace"]("[internal]" + std::to_string(workspaceIDRight));

        PWORKSPACER->m_vRenderOffset.setValue(RENDEROFFSET);
        PWORKSPACER->m_fAlpha.setValueAndWarp(255.f);

        m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.setValue(RENDEROFFSETMIDDLE);
        m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset = Vector2D(-m_sActiveSwipe.pMonitor->vecSize.x, 0);
        m_sActiveSwipe.pWorkspaceBegin->m_fAlpha.setValueAndWarp(255.f);

        g_pInputManager->unconstrainMouse();

        Debug::log(LOG, "Ended swipe to the right");
    }

    PWORKSPACEL->m_bForceRendering = false;
    PWORKSPACER->m_bForceRendering = false;
    m_sActiveSwipe.pWorkspaceBegin->m_bForceRendering = false;

    m_sActiveSwipe.pWorkspaceBegin = nullptr;

    g_pInputManager->refocus();
}

void CInputManager::onSwipeUpdate(wlr_pointer_swipe_update_event* e) {
    if (!m_sActiveSwipe.pWorkspaceBegin)
        return;

    static auto *const PSWIPEDIST = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_distance")->intValue;
    static auto *const PSWIPEINVR = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_invert")->intValue;

    m_sActiveSwipe.delta += *PSWIPEINVR ? -e->dx : e->dx;

    m_sActiveSwipe.avgSpeed = (m_sActiveSwipe.avgSpeed * m_sActiveSwipe.speedPoints + abs(e->dx)) / (m_sActiveSwipe.speedPoints + 1);
    m_sActiveSwipe.speedPoints++;

    std::string wsname = "";
    auto workspaceIDLeft = getWorkspaceIDFromString("m-1", wsname);
    auto workspaceIDRight = getWorkspaceIDFromString("m+1", wsname);

    if (workspaceIDLeft == INT_MAX || workspaceIDRight == INT_MAX || workspaceIDLeft == m_sActiveSwipe.pWorkspaceBegin->m_iID)
        return;

    m_sActiveSwipe.pWorkspaceBegin->m_bForceRendering = true;

    m_sActiveSwipe.delta = std::clamp(m_sActiveSwipe.delta, (double)-*PSWIPEDIST, (double)*PSWIPEDIST);

    if (m_sActiveSwipe.delta < 0) {
        if (workspaceIDLeft > m_sActiveSwipe.pWorkspaceBegin->m_iID){
            m_sActiveSwipe.delta = 0;
            return;
        }

        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(workspaceIDLeft);

        PWORKSPACE->m_bForceRendering = true;

        if (workspaceIDLeft != workspaceIDRight) {
            const auto PWORKSPACER = g_pCompositor->getWorkspaceByID(workspaceIDRight);

            PWORKSPACER->m_bForceRendering = false;
        }

        PWORKSPACE->m_vRenderOffset.setValueAndWarp(Vector2D(((- m_sActiveSwipe.delta) / *PSWIPEDIST) * m_sActiveSwipe.pMonitor->vecSize.x - m_sActiveSwipe.pMonitor->vecSize.x, 0));
        m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.setValueAndWarp(Vector2D(((- m_sActiveSwipe.delta) / *PSWIPEDIST) * m_sActiveSwipe.pMonitor->vecSize.x, 0));

        g_pCompositor->updateWorkspaceWindowDecos(workspaceIDLeft);
    } else {
        if (workspaceIDRight < m_sActiveSwipe.pWorkspaceBegin->m_iID){
            m_sActiveSwipe.delta = 0;
            return;
        }

        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(workspaceIDRight);

        PWORKSPACE->m_bForceRendering = true;

        if (workspaceIDLeft != workspaceIDRight) {
            const auto PWORKSPACEL = g_pCompositor->getWorkspaceByID(workspaceIDLeft);

            PWORKSPACEL->m_bForceRendering = false;
        }

        PWORKSPACE->m_vRenderOffset.setValueAndWarp(Vector2D(((- m_sActiveSwipe.delta) / *PSWIPEDIST) * m_sActiveSwipe.pMonitor->vecSize.x + m_sActiveSwipe.pMonitor->vecSize.x, 0));
        m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.setValueAndWarp(Vector2D(((- m_sActiveSwipe.delta) / *PSWIPEDIST) * m_sActiveSwipe.pMonitor->vecSize.x, 0));

        g_pCompositor->updateWorkspaceWindowDecos(workspaceIDRight);
    }

    g_pHyprRenderer->damageMonitor(m_sActiveSwipe.pMonitor);

    g_pCompositor->updateWorkspaceWindowDecos(m_sActiveSwipe.pWorkspaceBegin->m_iID);
}