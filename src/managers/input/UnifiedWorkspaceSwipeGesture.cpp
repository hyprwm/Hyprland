#include "UnifiedWorkspaceSwipeGesture.hpp"

#include "../../Compositor.hpp"
#include "../../render/Renderer.hpp"
#include "InputManager.hpp"

bool CUnifiedWorkspaceSwipeGesture::isGestureInProgress() {
    return m_workspaceBegin;
}

void CUnifiedWorkspaceSwipeGesture::begin() {
    if (isGestureInProgress())
        return;

    const auto PWORKSPACE = g_pCompositor->m_lastMonitor->m_activeWorkspace;

    Debug::log(LOG, "CUnifiedWorkspaceSwipeGesture::begin: Starting a swipe from {}", PWORKSPACE->m_name);

    m_workspaceBegin = PWORKSPACE;
    m_delta          = 0;
    m_monitor        = g_pCompositor->m_lastMonitor;
    m_avgSpeed       = 0;
    m_speedPoints    = 0;

    if (PWORKSPACE->m_hasFullscreenWindow) {
        for (auto const& ls : g_pCompositor->m_lastMonitor->m_layerSurfaceLayers[2]) {
            *ls->m_alpha = 1.f;
        }
    }
}

void CUnifiedWorkspaceSwipeGesture::update(double delta) {
    if (!isGestureInProgress())
        return;

    static auto  PSWIPEDIST             = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_distance");
    static auto  PSWIPEBOUND            = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_bound");
    static auto  PSWIPEDIRLOCK          = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_direction_lock");
    static auto  PSWIPEDIRLOCKTHRESHOLD = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_direction_lock_threshold");
    static auto  PSWIPEFOREVER          = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_forever");
    static auto  PSWIPEUSER             = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_use_r");
    static auto  PWORKSPACEGAP          = CConfigValue<Hyprlang::INT>("general:gaps_workspaces");

    const auto   SWIPEDISTANCE = std::clamp(*PSWIPEDIST, sc<int64_t>(1LL), sc<int64_t>(UINT32_MAX));
    const auto   XDISTANCE     = m_monitor->m_size.x + *PWORKSPACEGAP;
    const auto   YDISTANCE     = m_monitor->m_size.y + *PWORKSPACEGAP;
    const auto   ANIMSTYLE     = m_workspaceBegin->m_renderOffset->getStyle();
    const bool   VERTANIMS     = ANIMSTYLE == "slidevert" || ANIMSTYLE.starts_with("slidefadevert");
    const double d             = m_delta - delta;
    m_delta                    = delta;

    m_avgSpeed = (m_avgSpeed * m_speedPoints + abs(d)) / (m_speedPoints + 1);
    m_speedPoints++;

    auto workspaceIDLeft  = getWorkspaceIDNameFromString((*PSWIPEUSER ? "r-1" : "m-1")).id;
    auto workspaceIDRight = getWorkspaceIDNameFromString((*PSWIPEUSER ? "r+1" : "m+1")).id;

    if ((workspaceIDLeft == WORKSPACE_INVALID || workspaceIDRight == WORKSPACE_INVALID || workspaceIDLeft == m_workspaceBegin->m_id) && *PSWIPEBOUND != 1) {
        m_workspaceBegin = nullptr; // invalidate the swipe
        return;
    }

    m_workspaceBegin->m_forceRendering = true;

    m_delta = std::clamp(m_delta, sc<double>(-SWIPEDISTANCE), sc<double>(SWIPEDISTANCE));

    if ((m_workspaceBegin->m_id == workspaceIDLeft && *PSWIPEBOUND == 1 && (m_delta < 0)) ||
        (m_delta > 0 && m_workspaceBegin->getWindows() == 0 && workspaceIDRight <= m_workspaceBegin->m_id) ||
        (m_delta < 0 && m_workspaceBegin->m_id <= workspaceIDLeft && *PSWIPEBOUND != 2)) {

        m_delta = 0;
        g_pHyprRenderer->damageMonitor(m_monitor.lock());
        m_workspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(0.0, 0.0));
        return;
    }

    if (*PSWIPEDIRLOCK) {
        if (m_initialDirection != 0 && m_initialDirection != (m_delta < 0 ? -1 : 1))
            m_delta = 0;
        else if (m_initialDirection == 0 && abs(m_delta) > *PSWIPEDIRLOCKTHRESHOLD)
            m_initialDirection = m_delta < 0 ? -1 : 1;
    }

    if (m_delta < 0) {
        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(workspaceIDLeft);

        if ((workspaceIDLeft > m_workspaceBegin->m_id && *PSWIPEBOUND != 2) || !PWORKSPACE) {
            if (*PSWIPEBOUND == 1) {
                g_pHyprRenderer->damageMonitor(m_monitor.lock());

                if (VERTANIMS)
                    m_workspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE));
                else
                    m_workspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE, 0.0));

                m_workspaceBegin->updateWindowDecos();
                return;
            }
            m_delta = 0;
            return;
        }

        PWORKSPACE->m_forceRendering = true;
        PWORKSPACE->m_alpha->setValueAndWarp(1.f);

        if (workspaceIDLeft != workspaceIDRight && workspaceIDRight != m_workspaceBegin->m_id) {
            const auto PWORKSPACER = g_pCompositor->getWorkspaceByID(workspaceIDRight);

            if (PWORKSPACER) {
                PWORKSPACER->m_forceRendering = false;
                PWORKSPACER->m_alpha->setValueAndWarp(0.f);
            }
        }

        if (VERTANIMS) {
            PWORKSPACE->m_renderOffset->setValueAndWarp(Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE - YDISTANCE));
            m_workspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE));
        } else {
            PWORKSPACE->m_renderOffset->setValueAndWarp(Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE - XDISTANCE, 0.0));
            m_workspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE, 0.0));
        }

        PWORKSPACE->updateWindowDecos();
    } else {
        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(workspaceIDRight);

        if ((workspaceIDRight < m_workspaceBegin->m_id && *PSWIPEBOUND != 2) || !PWORKSPACE) {
            if (*PSWIPEBOUND) {
                g_pHyprRenderer->damageMonitor(m_monitor.lock());

                if (VERTANIMS)
                    m_workspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE));
                else
                    m_workspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE, 0.0));

                m_workspaceBegin->updateWindowDecos();
                return;
            }
            m_delta = 0;
            return;
        }

        PWORKSPACE->m_forceRendering = true;
        PWORKSPACE->m_alpha->setValueAndWarp(1.f);

        if (workspaceIDLeft != workspaceIDRight && workspaceIDLeft != m_workspaceBegin->m_id) {
            const auto PWORKSPACEL = g_pCompositor->getWorkspaceByID(workspaceIDLeft);

            if (PWORKSPACEL) {
                PWORKSPACEL->m_forceRendering = false;
                PWORKSPACEL->m_alpha->setValueAndWarp(0.f);
            }
        }

        if (VERTANIMS) {
            PWORKSPACE->m_renderOffset->setValueAndWarp(Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE + YDISTANCE));
            m_workspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE));
        } else {
            PWORKSPACE->m_renderOffset->setValueAndWarp(Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE + XDISTANCE, 0.0));
            m_workspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE, 0.0));
        }

        PWORKSPACE->updateWindowDecos();
    }

    g_pHyprRenderer->damageMonitor(m_monitor.lock());

    m_workspaceBegin->updateWindowDecos();

    if (*PSWIPEFOREVER) {
        if (abs(m_delta) >= SWIPEDISTANCE) {
            end();
            begin();
        }
    }
}

void CUnifiedWorkspaceSwipeGesture::end() {
    if (!isGestureInProgress())
        return;

    static auto PSWIPEPERC    = CConfigValue<Hyprlang::FLOAT>("gestures:workspace_swipe_cancel_ratio");
    static auto PSWIPEDIST    = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_distance");
    static auto PSWIPEFORC    = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_min_speed_to_force");
    static auto PSWIPEBOUND   = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_bound");
    static auto PSWIPEUSER    = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_use_r");
    static auto PWORKSPACEGAP = CConfigValue<Hyprlang::INT>("general:gaps_workspaces");
    const auto  ANIMSTYLE     = m_workspaceBegin->m_renderOffset->getStyle();
    const bool  VERTANIMS     = ANIMSTYLE == "slidevert" || ANIMSTYLE.starts_with("slidefadevert");

    // commit
    auto       workspaceIDLeft  = getWorkspaceIDNameFromString((*PSWIPEUSER ? "r-1" : "m-1")).id;
    auto       workspaceIDRight = getWorkspaceIDNameFromString((*PSWIPEUSER ? "r+1" : "m+1")).id;
    const auto SWIPEDISTANCE    = std::clamp(*PSWIPEDIST, sc<int64_t>(1LL), sc<int64_t>(UINT32_MAX));

    // If we've been swiping off the right end with PSWIPENEW enabled, there is
    // no workspace there yet, and we need to choose an ID for a new one now.
    if (workspaceIDRight <= m_workspaceBegin->m_id && *PSWIPEBOUND == 1)
        workspaceIDRight = getWorkspaceIDNameFromString("r+1").id;

    auto         PWORKSPACER = g_pCompositor->getWorkspaceByID(workspaceIDRight); // not guaranteed if PSWIPENEW || PSWIPENUMBER
    auto         PWORKSPACEL = g_pCompositor->getWorkspaceByID(workspaceIDLeft);  // not guaranteed if PSWIPENUMBER

    const auto   RENDEROFFSETMIDDLE = m_workspaceBegin->m_renderOffset->value();
    const auto   XDISTANCE          = m_monitor->m_size.x + *PWORKSPACEGAP;
    const auto   YDISTANCE          = m_monitor->m_size.y + *PWORKSPACEGAP;

    PHLWORKSPACE pSwitchedTo = nullptr;

    if ((abs(m_delta) < SWIPEDISTANCE * *PSWIPEPERC && (*PSWIPEFORC == 0 || (*PSWIPEFORC != 0 && m_avgSpeed < *PSWIPEFORC))) || abs(m_delta) < 2) {
        // revert
        if (abs(m_delta) < 2) {
            if (PWORKSPACEL)
                PWORKSPACEL->m_renderOffset->setValueAndWarp(Vector2D(0, 0));
            if (PWORKSPACER)
                PWORKSPACER->m_renderOffset->setValueAndWarp(Vector2D(0, 0));
            m_workspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(0, 0));
        } else {
            if (m_delta < 0) {
                // to left

                if (PWORKSPACEL) {
                    if (VERTANIMS)
                        *PWORKSPACEL->m_renderOffset = Vector2D{0.0, -YDISTANCE};
                    else
                        *PWORKSPACEL->m_renderOffset = Vector2D{-XDISTANCE, 0.0};
                }
            } else if (PWORKSPACER) {
                // to right
                if (VERTANIMS)
                    *PWORKSPACER->m_renderOffset = Vector2D{0.0, YDISTANCE};
                else
                    *PWORKSPACER->m_renderOffset = Vector2D{XDISTANCE, 0.0};
            }

            *m_workspaceBegin->m_renderOffset = Vector2D();
        }

        pSwitchedTo = m_workspaceBegin;
    } else if (m_delta < 0) {
        // switch to left
        const auto RENDEROFFSET = PWORKSPACEL ? PWORKSPACEL->m_renderOffset->value() : Vector2D();

        if (PWORKSPACEL)
            m_monitor->changeWorkspace(workspaceIDLeft);
        else {
            m_monitor->changeWorkspace(g_pCompositor->createNewWorkspace(workspaceIDLeft, m_monitor->m_id));
            PWORKSPACEL = g_pCompositor->getWorkspaceByID(workspaceIDLeft);
            PWORKSPACEL->rememberPrevWorkspace(m_workspaceBegin);
        }

        PWORKSPACEL->m_renderOffset->setValue(RENDEROFFSET);
        PWORKSPACEL->m_alpha->setValueAndWarp(1.f);

        m_workspaceBegin->m_renderOffset->setValue(RENDEROFFSETMIDDLE);
        if (VERTANIMS)
            *m_workspaceBegin->m_renderOffset = Vector2D(0.0, YDISTANCE);
        else
            *m_workspaceBegin->m_renderOffset = Vector2D(XDISTANCE, 0.0);
        m_workspaceBegin->m_alpha->setValueAndWarp(1.f);

        g_pInputManager->unconstrainMouse();

        Debug::log(LOG, "Ended swipe to the left");

        pSwitchedTo = PWORKSPACEL;
    } else {
        // switch to right
        const auto RENDEROFFSET = PWORKSPACER ? PWORKSPACER->m_renderOffset->value() : Vector2D();

        if (PWORKSPACER)
            m_monitor->changeWorkspace(workspaceIDRight);
        else {
            m_monitor->changeWorkspace(g_pCompositor->createNewWorkspace(workspaceIDRight, m_monitor->m_id));
            PWORKSPACER = g_pCompositor->getWorkspaceByID(workspaceIDRight);
            PWORKSPACER->rememberPrevWorkspace(m_workspaceBegin);
        }

        PWORKSPACER->m_renderOffset->setValue(RENDEROFFSET);
        PWORKSPACER->m_alpha->setValueAndWarp(1.f);

        m_workspaceBegin->m_renderOffset->setValue(RENDEROFFSETMIDDLE);
        if (VERTANIMS)
            *m_workspaceBegin->m_renderOffset = Vector2D(0.0, -YDISTANCE);
        else
            *m_workspaceBegin->m_renderOffset = Vector2D(-XDISTANCE, 0.0);
        m_workspaceBegin->m_alpha->setValueAndWarp(1.f);

        g_pInputManager->unconstrainMouse();

        Debug::log(LOG, "Ended swipe to the right");

        pSwitchedTo = PWORKSPACER;
    }
    m_workspaceBegin->rememberPrevWorkspace(pSwitchedTo);

    g_pHyprRenderer->damageMonitor(m_monitor.lock());

    if (PWORKSPACEL)
        PWORKSPACEL->m_forceRendering = false;
    if (PWORKSPACER)
        PWORKSPACER->m_forceRendering = false;
    m_workspaceBegin->m_forceRendering = false;

    m_workspaceBegin   = nullptr;
    m_initialDirection = 0;

    g_pInputManager->refocus();

    // apply alpha
    for (auto const& ls : g_pCompositor->m_lastMonitor->m_layerSurfaceLayers[2]) {
        *ls->m_alpha = pSwitchedTo->m_hasFullscreenWindow && pSwitchedTo->m_fullscreenMode == FSMODE_FULLSCREEN ? 0.f : 1.f;
    }
}