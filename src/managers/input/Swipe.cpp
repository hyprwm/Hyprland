#include "InputManager.hpp"
#include "../../Compositor.hpp"

void CInputManager::onSwipeBegin(wlr_pointer_swipe_begin_event* e) {
    static auto* const PSWIPE        = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe")->intValue;
    static auto* const PSWIPEFINGERS = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_fingers")->intValue;
    static auto* const PSWIPENEW     = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_create_new")->intValue;

    if (e->fingers != *PSWIPEFINGERS || *PSWIPE == 0 || g_pSessionLockManager->isSessionLocked())
        return;

    int onMonitor = 0;
    for (auto& w : g_pCompositor->m_vWorkspaces) {
        if (w->m_iMonitorID == g_pCompositor->m_pLastMonitor->ID && !g_pCompositor->isWorkspaceSpecial(w->m_iID)) {
            onMonitor++;
        }
    }

    if (onMonitor < 2 && !*PSWIPENEW)
        return; // disallow swiping when there's 1 workspace on a monitor

    beginWorkspaceSwipe();
}

void CInputManager::beginWorkspaceSwipe() {
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(g_pCompositor->m_pLastMonitor->activeWorkspace);

    Debug::log(LOG, "Starting a swipe from %s", PWORKSPACE->m_szName.c_str());

    m_sActiveSwipe.pWorkspaceBegin = PWORKSPACE;
    m_sActiveSwipe.delta           = 0;
    m_sActiveSwipe.pMonitor        = g_pCompositor->m_pLastMonitor;
    m_sActiveSwipe.avgSpeed        = 0;
    m_sActiveSwipe.speedPoints     = 0;

    if (PWORKSPACE->m_bHasFullscreenWindow) {
        for (auto& ls : g_pCompositor->m_pLastMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
            ls->alpha = 1.f;
        }
    }
}

void CInputManager::onSwipeEnd(wlr_pointer_swipe_end_event* e) {
    if (!m_sActiveSwipe.pWorkspaceBegin)
        return; // no valid swipe

    static auto* const PSWIPEPERC   = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_cancel_ratio")->floatValue;
    static auto* const PSWIPEDIST   = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_distance")->intValue;
    static auto* const PSWIPEFORC   = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_min_speed_to_force")->intValue;
    static auto* const PSWIPENEW    = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_create_new")->intValue;
    static auto* const PSWIPENUMBER = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_numbered")->intValue;
    const bool         VERTANIMS    = m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.getConfig()->pValues->internalStyle == "slidevert";

    // commit
    std::string wsname           = "";
    auto        workspaceIDLeft  = getWorkspaceIDFromString(*PSWIPENUMBER ? "-1" : "m-1", wsname);
    auto        workspaceIDRight = getWorkspaceIDFromString(*PSWIPENUMBER ? "+1" : "m+1", wsname);

    // If we've been swiping off the right end with PSWIPENEW enabled, there is
    // no workspace there yet, and we need to choose an ID for a new one now.
    // With multiple monitors, it might not be appropriate to choose one more
    // than the ID of the workspace we're swiping from, because that ID might
    // just be on another monitor.  It's also not just the smallest unused ID,
    // because that could be a gap in the existing workspace numbers, and it'd
    // be counterintuitive to swipe rightwards onto a new workspace and end up
    // left of where we started.  Instead, it's one more than the greatest
    // workspace ID that currently exists.
    if (workspaceIDRight <= m_sActiveSwipe.pWorkspaceBegin->m_iID && *PSWIPENEW) {
        int maxWorkspace = 0;
        for (const auto& ws : g_pCompositor->m_vWorkspaces) {
            maxWorkspace = std::max(maxWorkspace, ws->m_iID);
        }
        workspaceIDRight = maxWorkspace + 1;
    }

    auto        PWORKSPACER = g_pCompositor->getWorkspaceByID(workspaceIDRight); // not guaranteed if PSWIPENEW || PSWIPENUMBER
    auto        PWORKSPACEL = g_pCompositor->getWorkspaceByID(workspaceIDLeft);  // not guaranteed if PSWIPENUMBER

    const auto  RENDEROFFSETMIDDLE = m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.vec();

    CWorkspace* pSwitchedTo = nullptr;

    if ((abs(m_sActiveSwipe.delta) < *PSWIPEDIST * *PSWIPEPERC && (*PSWIPEFORC == 0 || (*PSWIPEFORC != 0 && m_sActiveSwipe.avgSpeed < *PSWIPEFORC))) ||
        abs(m_sActiveSwipe.delta) < 2) {
        // revert
        if (abs(m_sActiveSwipe.delta) < 2) {
            if (PWORKSPACEL)
                PWORKSPACEL->m_vRenderOffset.setValueAndWarp(Vector2D(0, 0));
            if (PWORKSPACER)
                PWORKSPACER->m_vRenderOffset.setValueAndWarp(Vector2D(0, 0));
            m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.setValueAndWarp(Vector2D(0, 0));
        } else {
            if (m_sActiveSwipe.delta < 0) {
                // to left
                if (PWORKSPACEL) {
                    if (VERTANIMS)
                        PWORKSPACEL->m_vRenderOffset = Vector2D({0, -m_sActiveSwipe.pMonitor->vecSize.y});
                    else
                        PWORKSPACEL->m_vRenderOffset = Vector2D({-m_sActiveSwipe.pMonitor->vecSize.x, 0});
                }
            } else if (PWORKSPACER) {
                // to right
                if (VERTANIMS)
                    PWORKSPACER->m_vRenderOffset = Vector2D({0, m_sActiveSwipe.pMonitor->vecSize.y});
                else
                    PWORKSPACER->m_vRenderOffset = Vector2D({m_sActiveSwipe.pMonitor->vecSize.x, 0});
            }

            m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset = Vector2D();
        }

        pSwitchedTo = m_sActiveSwipe.pWorkspaceBegin;
    } else if (m_sActiveSwipe.delta < 0) {
        // switch to left
        const auto RENDEROFFSET = PWORKSPACEL ? PWORKSPACEL->m_vRenderOffset.vec() : Vector2D();

        if (PWORKSPACEL)
            g_pKeybindManager->m_mDispatchers["workspace"]("[internal]" + std::to_string(workspaceIDLeft));
        else {
            g_pKeybindManager->m_mDispatchers["workspace"](std::to_string(workspaceIDLeft)); // so that the ID is created properly
            PWORKSPACEL = g_pCompositor->getWorkspaceByID(workspaceIDLeft);
        }

        PWORKSPACEL->m_vRenderOffset.setValue(RENDEROFFSET);
        PWORKSPACEL->m_fAlpha.setValueAndWarp(1.f);

        m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.setValue(RENDEROFFSETMIDDLE);
        if (VERTANIMS)
            m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset = Vector2D(0, m_sActiveSwipe.pMonitor->vecSize.y);
        else
            m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset = Vector2D(m_sActiveSwipe.pMonitor->vecSize.x, 0);
        m_sActiveSwipe.pWorkspaceBegin->m_fAlpha.setValueAndWarp(1.f);

        g_pInputManager->unconstrainMouse();

        Debug::log(LOG, "Ended swipe to the left");

        pSwitchedTo = PWORKSPACEL;
    } else {
        // switch to right
        const auto RENDEROFFSET = PWORKSPACER ? PWORKSPACER->m_vRenderOffset.vec() : Vector2D();

        if (PWORKSPACER)
            g_pKeybindManager->m_mDispatchers["workspace"]("[internal]" + std::to_string(workspaceIDRight));
        else {
            g_pKeybindManager->m_mDispatchers["workspace"](std::to_string(workspaceIDRight)); // so that the ID is created properly
            PWORKSPACER = g_pCompositor->getWorkspaceByID(workspaceIDRight);
        }

        PWORKSPACER->m_vRenderOffset.setValue(RENDEROFFSET);
        PWORKSPACER->m_fAlpha.setValueAndWarp(1.f);

        m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.setValue(RENDEROFFSETMIDDLE);
        if (VERTANIMS)
            m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset = Vector2D(0, -m_sActiveSwipe.pMonitor->vecSize.y);
        else
            m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset = Vector2D(-m_sActiveSwipe.pMonitor->vecSize.x, 0);
        m_sActiveSwipe.pWorkspaceBegin->m_fAlpha.setValueAndWarp(1.f);

        g_pInputManager->unconstrainMouse();

        Debug::log(LOG, "Ended swipe to the right");

        pSwitchedTo = PWORKSPACER;
    }

    g_pHyprRenderer->damageMonitor(m_sActiveSwipe.pMonitor);

    if (PWORKSPACEL)
        PWORKSPACEL->m_bForceRendering = false;
    if (PWORKSPACER)
        PWORKSPACER->m_bForceRendering = false;
    m_sActiveSwipe.pWorkspaceBegin->m_bForceRendering = false;

    m_sActiveSwipe.pWorkspaceBegin = nullptr;

    g_pInputManager->refocus();

    // apply alpha
    for (auto& ls : g_pCompositor->m_pLastMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        ls->alpha = pSwitchedTo->m_bHasFullscreenWindow && pSwitchedTo->m_efFullscreenMode == FULLSCREEN_FULL ? 0.f : 1.f;
    }
}

void CInputManager::onSwipeUpdate(wlr_pointer_swipe_update_event* e) {
    if (!m_sActiveSwipe.pWorkspaceBegin)
        return;

    static auto* const PSWIPEDIST    = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_distance")->intValue;
    static auto* const PSWIPEINVR    = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_invert")->intValue;
    static auto* const PSWIPENEW     = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_create_new")->intValue;
    static auto* const PSWIPEFOREVER = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_forever")->intValue;
    static auto* const PSWIPENUMBER  = &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_numbered")->intValue;

    const bool         VERTANIMS = m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.getConfig()->pValues->internalStyle == "slidevert";

    m_sActiveSwipe.delta += VERTANIMS ? (*PSWIPEINVR ? -e->dy : e->dy) : (*PSWIPEINVR ? -e->dx : e->dx);

    m_sActiveSwipe.avgSpeed = (m_sActiveSwipe.avgSpeed * m_sActiveSwipe.speedPoints + abs(e->dx)) / (m_sActiveSwipe.speedPoints + 1);
    m_sActiveSwipe.speedPoints++;

    std::string wsname           = "";
    auto        workspaceIDLeft  = getWorkspaceIDFromString(*PSWIPENUMBER ? "-1" : "m-1", wsname);
    auto        workspaceIDRight = getWorkspaceIDFromString(*PSWIPENUMBER ? "+1" : "m+1", wsname);

    if ((workspaceIDLeft == INT_MAX || workspaceIDRight == INT_MAX || workspaceIDLeft == m_sActiveSwipe.pWorkspaceBegin->m_iID) && !*PSWIPENEW) {
        m_sActiveSwipe.pWorkspaceBegin = nullptr; // invalidate the swipe
        return;
    }

    m_sActiveSwipe.pWorkspaceBegin->m_bForceRendering = true;

    m_sActiveSwipe.delta = std::clamp(m_sActiveSwipe.delta, (double)-*PSWIPEDIST, (double)*PSWIPEDIST);

    if ((m_sActiveSwipe.pWorkspaceBegin->m_iID == workspaceIDLeft && *PSWIPENEW && (m_sActiveSwipe.delta < 0)) ||
        (m_sActiveSwipe.delta > 0 && g_pCompositor->getWindowsOnWorkspace(m_sActiveSwipe.pWorkspaceBegin->m_iID) == 0 &&
         workspaceIDRight <= m_sActiveSwipe.pWorkspaceBegin->m_iID) ||
        (m_sActiveSwipe.delta < 0 && m_sActiveSwipe.pWorkspaceBegin->m_iID <= workspaceIDLeft)) {

        m_sActiveSwipe.delta = 0;
        return;
    }

    if (m_sActiveSwipe.delta < 0) {
        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(workspaceIDLeft);

        if (workspaceIDLeft > m_sActiveSwipe.pWorkspaceBegin->m_iID || !PWORKSPACE) {
            if (*PSWIPENEW || *PSWIPENUMBER) {
                g_pHyprRenderer->damageMonitor(m_sActiveSwipe.pMonitor);

                if (VERTANIMS)
                    m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.setValueAndWarp(Vector2D(0, ((-m_sActiveSwipe.delta) / *PSWIPEDIST) * m_sActiveSwipe.pMonitor->vecSize.y));
                else
                    m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.setValueAndWarp(Vector2D(((-m_sActiveSwipe.delta) / *PSWIPEDIST) * m_sActiveSwipe.pMonitor->vecSize.x, 0));

                g_pCompositor->updateWorkspaceWindowDecos(m_sActiveSwipe.pWorkspaceBegin->m_iID);
                return;
            }
            m_sActiveSwipe.delta = 0;
            return;
        }

        PWORKSPACE->m_bForceRendering = true;
        PWORKSPACE->m_fAlpha.setValueAndWarp(1.f);

        if (workspaceIDLeft != workspaceIDRight) {
            const auto PWORKSPACER = g_pCompositor->getWorkspaceByID(workspaceIDRight);

            if (PWORKSPACER) {
                PWORKSPACER->m_bForceRendering = false;
                PWORKSPACER->m_fAlpha.setValueAndWarp(0.f);
            }
        }

        if (VERTANIMS) {
            PWORKSPACE->m_vRenderOffset.setValueAndWarp(
                Vector2D(0, ((-m_sActiveSwipe.delta) / *PSWIPEDIST) * m_sActiveSwipe.pMonitor->vecSize.y - m_sActiveSwipe.pMonitor->vecSize.y));
            m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.setValueAndWarp(Vector2D(0, ((-m_sActiveSwipe.delta) / *PSWIPEDIST) * m_sActiveSwipe.pMonitor->vecSize.y));
        } else {
            PWORKSPACE->m_vRenderOffset.setValueAndWarp(
                Vector2D(((-m_sActiveSwipe.delta) / *PSWIPEDIST) * m_sActiveSwipe.pMonitor->vecSize.x - m_sActiveSwipe.pMonitor->vecSize.x, 0));
            m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.setValueAndWarp(Vector2D(((-m_sActiveSwipe.delta) / *PSWIPEDIST) * m_sActiveSwipe.pMonitor->vecSize.x, 0));
        }

        g_pCompositor->updateWorkspaceWindowDecos(workspaceIDLeft);
    } else {
        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(workspaceIDRight);

        if (workspaceIDRight < m_sActiveSwipe.pWorkspaceBegin->m_iID || !PWORKSPACE) {
            if (*PSWIPENEW || *PSWIPENUMBER) {
                g_pHyprRenderer->damageMonitor(m_sActiveSwipe.pMonitor);

                if (VERTANIMS)
                    m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.setValueAndWarp(Vector2D(0, ((-m_sActiveSwipe.delta) / *PSWIPEDIST) * m_sActiveSwipe.pMonitor->vecSize.y));
                else
                    m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.setValueAndWarp(Vector2D(((-m_sActiveSwipe.delta) / *PSWIPEDIST) * m_sActiveSwipe.pMonitor->vecSize.x, 0));

                g_pCompositor->updateWorkspaceWindowDecos(m_sActiveSwipe.pWorkspaceBegin->m_iID);
                return;
            }
            m_sActiveSwipe.delta = 0;
            return;
        }

        PWORKSPACE->m_bForceRendering = true;
        PWORKSPACE->m_fAlpha.setValueAndWarp(1.f);

        if (workspaceIDLeft != workspaceIDRight) {
            const auto PWORKSPACEL = g_pCompositor->getWorkspaceByID(workspaceIDLeft);

            if (PWORKSPACEL) {
                PWORKSPACEL->m_bForceRendering = false;
                PWORKSPACEL->m_fAlpha.setValueAndWarp(0.f);
            }
        }

        if (VERTANIMS) {
            PWORKSPACE->m_vRenderOffset.setValueAndWarp(
                Vector2D(0, ((-m_sActiveSwipe.delta) / *PSWIPEDIST) * m_sActiveSwipe.pMonitor->vecSize.y + m_sActiveSwipe.pMonitor->vecSize.y));
            m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.setValueAndWarp(Vector2D(0, ((-m_sActiveSwipe.delta) / *PSWIPEDIST) * m_sActiveSwipe.pMonitor->vecSize.y));
        } else {
            PWORKSPACE->m_vRenderOffset.setValueAndWarp(
                Vector2D(((-m_sActiveSwipe.delta) / *PSWIPEDIST) * m_sActiveSwipe.pMonitor->vecSize.x + m_sActiveSwipe.pMonitor->vecSize.x, 0));
            m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.setValueAndWarp(Vector2D(((-m_sActiveSwipe.delta) / *PSWIPEDIST) * m_sActiveSwipe.pMonitor->vecSize.x, 0));
        }

        g_pCompositor->updateWorkspaceWindowDecos(workspaceIDRight);
    }

    g_pHyprRenderer->damageMonitor(m_sActiveSwipe.pMonitor);

    g_pCompositor->updateWorkspaceWindowDecos(m_sActiveSwipe.pWorkspaceBegin->m_iID);

    if (*PSWIPEFOREVER) {
        if (abs(m_sActiveSwipe.delta) >= *PSWIPEDIST) {
            onSwipeEnd(nullptr);
            beginWorkspaceSwipe();
        }
    }
}
