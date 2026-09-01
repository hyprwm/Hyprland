#include "UnifiedWorkspaceSwipeGesture.hpp"

#include "../../Compositor.hpp"
#include "../../state/WorkspaceState.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../render/Renderer.hpp"
#include "InputManager.hpp"
#include "../../layout/space/Space.hpp"
#include "../../layout/algorithm/Algorithm.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"
#include "../../output/WorkspaceTransition.hpp"

void CUnifiedWorkspaceSwipeGesture::setForceRendering(PHLWORKSPACE workspace, bool force) {
    if (!workspace)
        return;

    if (const auto MONITOR = workspace->m_monitor.lock(); MONITOR)
        MONITOR->m_workspaceTransition->setForceRendering(workspace, force);

    if (force) {
        if (!std::ranges::contains(m_forcedWorkspaces, workspace))
            m_forcedWorkspaces.emplace_back(workspace);
    } else
        std::erase(m_forcedWorkspaces, workspace);
}

void CUnifiedWorkspaceSwipeGesture::clearForcedWorkspaces() {
    const auto FORCEDWORKSPACES = std::move(m_forcedWorkspaces);
    m_forcedWorkspaces.clear();

    for (const auto& weak : FORCEDWORKSPACES) {
        const auto WORKSPACE = weak.lock();
        const auto MONITOR   = WORKSPACE ? WORKSPACE->m_monitor.lock() : nullptr;
        if (!MONITOR)
            continue;

        MONITOR->m_workspaceTransition->setForceRendering(WORKSPACE, false);
        g_pHyprRenderer->damageMonitor(MONITOR);
    }
}

void CUnifiedWorkspaceSwipeGesture::cancel() {
    clearForcedWorkspaces();
    m_workspaceBegin.reset();
    m_monitor.reset();
    m_initialDirection = 0;
}

bool CUnifiedWorkspaceSwipeGesture::isGestureInProgress() {
    return !!m_workspaceBegin;
}

void CUnifiedWorkspaceSwipeGesture::begin() {
    if (isGestureInProgress())
        return;

    m_monitor = Desktop::focusState()->monitor();
    if (!m_monitor)
        return;

    const auto PWORKSPACE = m_monitor->m_activeWorkspace;

    Log::logger->log(Log::DEBUG, "CUnifiedWorkspaceSwipeGesture::begin: Starting a swipe from {}", PWORKSPACE->m_name);

    m_workspaceBegin = PWORKSPACE;
    m_delta          = 0;
    m_avgSpeed       = 0;
    m_speedPoints    = 0;

    const auto FSWINDOW         = Fullscreen::controller()->getFullscreenWindow(PWORKSPACE);
    const auto INTERNAL_FS_MODE = FSWINDOW ? Fullscreen::controller()->getFullscreenModes(FSWINDOW).internal : Fullscreen::FSMODE_NONE;

    if (INTERNAL_FS_MODE == Fullscreen::FSMODE_FULLSCREEN) {
        for (auto const& ls : m_monitor->m_layerSurfaceLayers[2]) {
            *ls->alpha()[Desktop::View::LS_ALPHA_FADE] = 1.F;
        }
    }
}

void CUnifiedWorkspaceSwipeGesture::update(double delta) {
    if (!isGestureInProgress())
        return;

    if (!m_monitor || m_workspaceBegin->m_monitor != m_monitor) {
        cancel();
        return;
    }

    static auto  PSWIPEDIST             = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_distance");
    static auto  PSWIPENEW              = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_create_new");
    static auto  PSWIPEDIRLOCK          = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_direction_lock");
    static auto  PSWIPEDIRLOCKTHRESHOLD = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_direction_lock_threshold");
    static auto  PSWIPEFOREVER          = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_forever");
    static auto  PSWIPEUSER             = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_use_r");
    static auto  PWORKSPACEGAP          = CConfigValue<Config::INTEGER>("general:gaps_workspaces");

    auto&        WORKSPACETRANSITION = *m_monitor->m_workspaceTransition;
    const auto   SWIPEDISTANCE       = std::clamp(*PSWIPEDIST, sc<int64_t>(1LL), sc<int64_t>(UINT32_MAX));
    const auto   XDISTANCE           = m_monitor->m_size.x + *PWORKSPACEGAP;
    const auto   YDISTANCE           = m_monitor->m_size.y + *PWORKSPACEGAP;
    const auto   ANIMSTYLE           = WORKSPACETRANSITION.style(m_workspaceBegin);
    const bool   VERTANIMS           = ANIMSTYLE == "slidevert" || ANIMSTYLE.starts_with("slidefadevert");
    const double d                   = m_delta - delta;
    m_delta                          = delta;

    m_avgSpeed = (m_avgSpeed * m_speedPoints + abs(d)) / (m_speedPoints + 1);
    m_speedPoints++;

    auto workspaceIDLeft  = getWorkspaceIDNameFromString((*PSWIPEUSER ? "r-1" : "m-1")).id;
    auto workspaceIDRight = getWorkspaceIDNameFromString((*PSWIPEUSER ? "r+1" : "m+1")).id;

    if ((workspaceIDLeft == WORKSPACE_INVALID || workspaceIDRight == WORKSPACE_INVALID || workspaceIDLeft == m_workspaceBegin->m_id) && !*PSWIPENEW) {
        cancel();
        return;
    }

    setForceRendering(m_workspaceBegin, true);

    m_delta = std::clamp(m_delta, sc<double>(-SWIPEDISTANCE), sc<double>(SWIPEDISTANCE));

    if ((m_workspaceBegin->m_id == workspaceIDLeft && *PSWIPENEW && (m_delta < 0)) ||
        (m_delta > 0 && m_workspaceBegin->getWindowCount() == 0 && workspaceIDRight <= m_workspaceBegin->m_id) || (m_delta < 0 && m_workspaceBegin->m_id <= workspaceIDLeft)) {

        m_delta = 0;
        g_pHyprRenderer->damageMonitor(m_monitor.lock());
        WORKSPACETRANSITION.ensure(m_workspaceBegin).offset->setValueAndWarp(Vector2D(0.0, 0.0));
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
                    WORKSPACETRANSITION.ensure(m_workspaceBegin).offset->setValueAndWarp(Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE));
                else
                    WORKSPACETRANSITION.ensure(m_workspaceBegin).offset->setValueAndWarp(Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE, 0.0));

                m_workspaceBegin->updateWindowDecos();
                return;
            }
            m_delta = 0;
            return;
        }

        setForceRendering(PWORKSPACE, true);
        WORKSPACETRANSITION.ensure(PWORKSPACE).alpha->setValueAndWarp(1.f);

        if (workspaceIDLeft != workspaceIDRight && workspaceIDRight != m_workspaceBegin->m_id) {
            const auto PWORKSPACER = State::workspaceState()->query().id(workspaceIDRight).run();

            if (PWORKSPACER) {
                setForceRendering(PWORKSPACER, false);
                WORKSPACETRANSITION.ensure(PWORKSPACER).alpha->setValueAndWarp(0.f);
            }
        }

        if (VERTANIMS) {
            WORKSPACETRANSITION.ensure(PWORKSPACE).offset->setValueAndWarp(Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE - YDISTANCE));
            WORKSPACETRANSITION.ensure(m_workspaceBegin).offset->setValueAndWarp(Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE));
        } else {
            WORKSPACETRANSITION.ensure(PWORKSPACE).offset->setValueAndWarp(Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE - XDISTANCE, 0.0));
            WORKSPACETRANSITION.ensure(m_workspaceBegin).offset->setValueAndWarp(Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE, 0.0));
        }

        PWORKSPACE->updateWindowDecos();
    } else {
        const auto PWORKSPACE = State::workspaceState()->query().id(workspaceIDRight).run();

        if (workspaceIDRight < m_workspaceBegin->m_id || !PWORKSPACE) {
            if (*PSWIPENEW) {
                g_pHyprRenderer->damageMonitor(m_monitor.lock());

                if (VERTANIMS)
                    WORKSPACETRANSITION.ensure(m_workspaceBegin).offset->setValueAndWarp(Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE));
                else
                    WORKSPACETRANSITION.ensure(m_workspaceBegin).offset->setValueAndWarp(Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE, 0.0));

                m_workspaceBegin->updateWindowDecos();
                return;
            }
            m_delta = 0;
            return;
        }

        setForceRendering(PWORKSPACE, true);
        WORKSPACETRANSITION.ensure(PWORKSPACE).alpha->setValueAndWarp(1.f);

        if (workspaceIDLeft != workspaceIDRight && workspaceIDLeft != m_workspaceBegin->m_id) {
            const auto PWORKSPACEL = State::workspaceState()->query().id(workspaceIDLeft).run();

            if (PWORKSPACEL) {
                setForceRendering(PWORKSPACEL, false);
                WORKSPACETRANSITION.ensure(PWORKSPACEL).alpha->setValueAndWarp(0.f);
            }
        }

        if (VERTANIMS) {
            WORKSPACETRANSITION.ensure(PWORKSPACE).offset->setValueAndWarp(Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE + YDISTANCE));
            WORKSPACETRANSITION.ensure(m_workspaceBegin).offset->setValueAndWarp(Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE));
        } else {
            WORKSPACETRANSITION.ensure(PWORKSPACE).offset->setValueAndWarp(Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE + XDISTANCE, 0.0));
            WORKSPACETRANSITION.ensure(m_workspaceBegin).offset->setValueAndWarp(Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE, 0.0));
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

    if (!m_monitor || m_workspaceBegin->m_monitor != m_monitor) {
        cancel();
        return;
    }

    static auto PSWIPEPERC          = CConfigValue<Config::FLOAT>("gestures:workspace_swipe_cancel_ratio");
    static auto PSWIPEDIST          = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_distance");
    static auto PSWIPEFORC          = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_min_speed_to_force");
    static auto PSWIPENEW           = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_create_new");
    static auto PSWIPEUSER          = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_use_r");
    static auto PWORKSPACEGAP       = CConfigValue<Config::INTEGER>("general:gaps_workspaces");
    auto&       WORKSPACETRANSITION = *m_monitor->m_workspaceTransition;
    const auto  ANIMSTYLE           = WORKSPACETRANSITION.style(m_workspaceBegin);
    const bool  VERTANIMS           = ANIMSTYLE == "slidevert" || ANIMSTYLE.starts_with("slidefadevert");

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

    const auto   RENDEROFFSETMIDDLE = WORKSPACETRANSITION.ensure(m_workspaceBegin).offset->value();
    const auto   XDISTANCE          = m_monitor->m_size.x + *PWORKSPACEGAP;
    const auto   YDISTANCE          = m_monitor->m_size.y + *PWORKSPACEGAP;

    PHLWORKSPACE pSwitchedTo = nullptr;

    if ((abs(m_delta) < SWIPEDISTANCE * *PSWIPEPERC && (*PSWIPEFORC == 0 || (*PSWIPEFORC != 0 && m_avgSpeed < *PSWIPEFORC))) || abs(m_delta) < 2) {
        // revert
        if (abs(m_delta) < 2) {
            if (PWORKSPACEL)
                WORKSPACETRANSITION.ensure(PWORKSPACEL).offset->setValueAndWarp(Vector2D(0, 0));
            if (PWORKSPACER)
                WORKSPACETRANSITION.ensure(PWORKSPACER).offset->setValueAndWarp(Vector2D(0, 0));
            WORKSPACETRANSITION.ensure(m_workspaceBegin).offset->setValueAndWarp(Vector2D(0, 0));
        } else {
            if (m_delta < 0) {
                // to left

                if (PWORKSPACEL) {
                    if (VERTANIMS)
                        *WORKSPACETRANSITION.ensure(PWORKSPACEL).offset = Vector2D{0.0, -YDISTANCE};
                    else
                        *WORKSPACETRANSITION.ensure(PWORKSPACEL).offset = Vector2D{-XDISTANCE, 0.0};
                }
            } else if (PWORKSPACER) {
                // to right
                if (VERTANIMS)
                    *WORKSPACETRANSITION.ensure(PWORKSPACER).offset = Vector2D{0.0, YDISTANCE};
                else
                    *WORKSPACETRANSITION.ensure(PWORKSPACER).offset = Vector2D{XDISTANCE, 0.0};
            }

            *WORKSPACETRANSITION.ensure(m_workspaceBegin).offset = Vector2D();
        }

        pSwitchedTo = m_workspaceBegin;
    } else if (m_delta < 0) {
        // switch to left
        const auto RENDEROFFSET = PWORKSPACEL ? WORKSPACETRANSITION.ensure(PWORKSPACEL).offset->value() : Vector2D();

        if (PWORKSPACEL)
            m_monitor->changeWorkspace(workspaceIDLeft);
        else {
            m_monitor->changeWorkspace(State::workspaceState()->create(workspaceIDLeft, m_monitor->m_id));
            PWORKSPACEL = State::workspaceState()->query().id(workspaceIDLeft).run();
        }

        WORKSPACETRANSITION.ensure(PWORKSPACEL).offset->setValue(RENDEROFFSET);
        WORKSPACETRANSITION.ensure(PWORKSPACEL).alpha->setValueAndWarp(1.f);

        WORKSPACETRANSITION.ensure(m_workspaceBegin).offset->setValue(RENDEROFFSETMIDDLE);
        if (VERTANIMS)
            *WORKSPACETRANSITION.ensure(m_workspaceBegin).offset = Vector2D(0.0, YDISTANCE);
        else
            *WORKSPACETRANSITION.ensure(m_workspaceBegin).offset = Vector2D(XDISTANCE, 0.0);
        WORKSPACETRANSITION.ensure(m_workspaceBegin).alpha->setValueAndWarp(1.f);

        g_pInputManager->unconstrainMouse();

        Log::logger->log(Log::DEBUG, "Ended swipe to the left");

        pSwitchedTo = PWORKSPACEL;
    } else {
        // switch to right
        const auto RENDEROFFSET = PWORKSPACER ? WORKSPACETRANSITION.ensure(PWORKSPACER).offset->value() : Vector2D();

        if (PWORKSPACER)
            m_monitor->changeWorkspace(workspaceIDRight);
        else {
            m_monitor->changeWorkspace(State::workspaceState()->create(workspaceIDRight, m_monitor->m_id));
            PWORKSPACER = State::workspaceState()->query().id(workspaceIDRight).run();
        }

        WORKSPACETRANSITION.ensure(PWORKSPACER).offset->setValue(RENDEROFFSET);
        WORKSPACETRANSITION.ensure(PWORKSPACER).alpha->setValueAndWarp(1.f);

        WORKSPACETRANSITION.ensure(m_workspaceBegin).offset->setValue(RENDEROFFSETMIDDLE);
        if (VERTANIMS)
            *WORKSPACETRANSITION.ensure(m_workspaceBegin).offset = Vector2D(0.0, -YDISTANCE);
        else
            *WORKSPACETRANSITION.ensure(m_workspaceBegin).offset = Vector2D(-XDISTANCE, 0.0);
        WORKSPACETRANSITION.ensure(m_workspaceBegin).alpha->setValueAndWarp(1.f);

        g_pInputManager->unconstrainMouse();

        Log::logger->log(Log::DEBUG, "Ended swipe to the right");

        pSwitchedTo = PWORKSPACER;
    }

    g_pHyprRenderer->damageMonitor(m_monitor.lock());

    clearForcedWorkspaces();

    m_workspaceBegin   = nullptr;
    m_initialDirection = 0;

    g_pInputManager->refocus();

    // apply alpha
    if (pSwitchedTo) {
        const auto FSWINDOW         = Fullscreen::controller()->getFullscreenWindow(pSwitchedTo);
        const auto FS_MODE_INTERNAL = FSWINDOW ? Fullscreen::controller()->getFullscreenModes(FSWINDOW).internal : Fullscreen::FSMODE_NONE;
        const bool HIDE             = FS_MODE_INTERNAL == Fullscreen::FSMODE_FULLSCREEN &&
            (!FSWINDOW || !Fullscreen::controller()->layoutManagedFS(FSWINDOW) ||
             (pSwitchedTo->m_space && pSwitchedTo->m_space->algorithm() && Fullscreen::controller()->hasFullscreen(pSwitchedTo, true)));

        for (auto const& ls : m_monitor->m_layerSurfaceLayers[2]) {
            *ls->alpha()[Desktop::View::LS_ALPHA_FADE] = HIDE ? 0.F : 1.F;
        }
    }
}
