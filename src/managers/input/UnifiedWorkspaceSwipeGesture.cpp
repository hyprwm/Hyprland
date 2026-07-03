#include "UnifiedWorkspaceSwipeGesture.hpp"

#include "../../Compositor.hpp"
#include "../../state/WorkspaceState.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../render/Renderer.hpp"
#include "InputManager.hpp"
#include "../../layout/space/Space.hpp"
#include "../../layout/algorithm/Algorithm.hpp"

bool CUnifiedWorkspaceSwipeGesture::isGestureInProgress() {
    return !!m_workspaceBegin;
}

void CUnifiedWorkspaceSwipeGesture::begin() {
    if (isGestureInProgress())
        return;

    const auto PWORKSPACE = Desktop::focusState()->monitor()->m_activeWorkspace;

    Log::logger->log(Log::DEBUG, "CUnifiedWorkspaceSwipeGesture::begin: Starting a swipe from {}", PWORKSPACE->m_name);

    m_workspaceBegin = PWORKSPACE;
    m_delta          = 0;
    m_monitor        = Desktop::focusState()->monitor();
    m_avgSpeed       = 0;
    m_speedPoints    = 0;

    const auto FSWINDOW = PWORKSPACE->getFullscreenWindow(true);
    const auto FSMODE   = FSWINDOW ? FSWINDOW->m_target->fullscreenMode() : PWORKSPACE->m_fullscreenMode;

    if (FSMODE == FSMODE_FULLSCREEN) {
        for (auto const& ls : Desktop::focusState()->monitor()->m_layerSurfaceLayers[2]) {
            *ls->alpha()[Desktop::View::LS_ALPHA_FADE] = 1.F;
        }
    }
}

void CUnifiedWorkspaceSwipeGesture::update(double delta) {
    if (!isGestureInProgress())
        return;

    static auto  PSWIPEDIST             = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_distance");
    static auto  PSWIPENEW              = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_create_new");
    static auto  PSWIPEDIRLOCK          = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_direction_lock");
    static auto  PSWIPEDIRLOCKTHRESHOLD = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_direction_lock_threshold");
    static auto  PSWIPEFOREVER          = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_forever");
    static auto  PSWIPEUSER             = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_use_r");
    static auto  PWORKSPACEGAP          = CConfigValue<Config::INTEGER>("general:gaps_workspaces");

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

    if ((workspaceIDLeft == WORKSPACE_INVALID || workspaceIDRight == WORKSPACE_INVALID || workspaceIDLeft == m_workspaceBegin->m_id) && !*PSWIPENEW) {
        m_workspaceBegin = nullptr; // invalidate the swipe
        return;
    }

    m_workspaceBegin->m_forceRendering = true;

    m_delta = std::clamp(m_delta, sc<double>(-SWIPEDISTANCE), sc<double>(SWIPEDISTANCE));

    if ((m_workspaceBegin->m_id == workspaceIDLeft && *PSWIPENEW && (m_delta < 0)) ||
        (m_delta > 0 && m_workspaceBegin->getWindowCount() == 0 && workspaceIDRight <= m_workspaceBegin->m_id) || (m_delta < 0 && m_workspaceBegin->m_id <= workspaceIDLeft)) {

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
        const auto PWORKSPACE = State::workspaceState()->query().id(workspaceIDLeft).run();

        if (workspaceIDLeft > m_workspaceBegin->m_id || !PWORKSPACE) {
            if (*PSWIPENEW) {
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
            const auto PWORKSPACER = State::workspaceState()->query().id(workspaceIDRight).run();

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
        const auto PWORKSPACE = State::workspaceState()->query().id(workspaceIDRight).run();

        if (workspaceIDRight < m_workspaceBegin->m_id || !PWORKSPACE) {
            if (*PSWIPENEW) {
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
            const auto PWORKSPACEL = State::workspaceState()->query().id(workspaceIDLeft).run();

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

    static auto PSWIPEPERC    = CConfigValue<Config::FLOAT>("gestures:workspace_swipe_cancel_ratio");
    static auto PSWIPEDIST    = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_distance");
    static auto PSWIPEFORC    = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_min_speed_to_force");
    static auto PSWIPENEW     = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_create_new");
    static auto PSWIPEUSER    = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_use_r");
    static auto PWORKSPACEGAP = CConfigValue<Config::INTEGER>("general:gaps_workspaces");
    const auto  ANIMSTYLE     = m_workspaceBegin->m_renderOffset->getStyle();
    const bool  VERTANIMS     = ANIMSTYLE == "slidevert" || ANIMSTYLE.starts_with("slidefadevert");

    // commit
    auto       workspaceIDLeft  = getWorkspaceIDNameFromString((*PSWIPEUSER ? "r-1" : "m-1")).id;
    auto       workspaceIDRight = getWorkspaceIDNameFromString((*PSWIPEUSER ? "r+1" : "m+1")).id;
    const auto SWIPEDISTANCE    = std::clamp(*PSWIPEDIST, sc<int64_t>(1LL), sc<int64_t>(UINT32_MAX));

    // If we've been swiping off the right end with PSWIPENEW enabled, there is
    // no workspace there yet, and we need to choose an ID for a new one now.
    if (workspaceIDRight <= m_workspaceBegin->m_id && *PSWIPENEW)
        workspaceIDRight = getWorkspaceIDNameFromString("r+1").id;

    auto         PWORKSPACER = State::workspaceState()->query().id(workspaceIDRight).run(); // not guaranteed if PSWIPENEW || PSWIPENUMBER
    auto         PWORKSPACEL = State::workspaceState()->query().id(workspaceIDLeft).run();  // not guaranteed if PSWIPENUMBER

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
            m_monitor->changeWorkspace(State::workspaceState()->create(workspaceIDLeft, m_monitor->m_id));
            PWORKSPACEL = State::workspaceState()->query().id(workspaceIDLeft).run();
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

        Log::logger->log(Log::DEBUG, "Ended swipe to the left");

        pSwitchedTo = PWORKSPACEL;
    } else {
        // switch to right
        const auto RENDEROFFSET = PWORKSPACER ? PWORKSPACER->m_renderOffset->value() : Vector2D();

        if (PWORKSPACER)
            m_monitor->changeWorkspace(workspaceIDRight);
        else {
            m_monitor->changeWorkspace(State::workspaceState()->create(workspaceIDRight, m_monitor->m_id));
            PWORKSPACER = State::workspaceState()->query().id(workspaceIDRight).run();
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

        Log::logger->log(Log::DEBUG, "Ended swipe to the right");

        pSwitchedTo = PWORKSPACER;
    }

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
    if (pSwitchedTo) {
        const auto FSWINDOW = pSwitchedTo->getFullscreenWindow(true);
        const auto FSMODE   = FSWINDOW ? FSWINDOW->m_target->fullscreenMode() : pSwitchedTo->m_fullscreenMode;
        const bool HIDE     = FSMODE == FSMODE_FULLSCREEN &&
            (!FSWINDOW || !FSWINDOW->m_target->layoutManagedFullscreen() ||
             (pSwitchedTo->m_space && pSwitchedTo->m_space->algorithm() && pSwitchedTo->m_space->algorithm()->layoutFullscreenCoversMonitor()));

        for (auto const& ls : Desktop::focusState()->monitor()->m_layerSurfaceLayers[2]) {
            *ls->alpha()[Desktop::View::LS_ALPHA_FADE] = HIDE ? 0.F : 1.F;
        }
    }
}
