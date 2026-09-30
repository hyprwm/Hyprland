#include "UnifiedWorkspaceSwipeGesture.hpp"

#include "../../Compositor.hpp"
#include "../../state/WorkspaceState.hpp"
#include "../../state/workspace/Resolver.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../render/Renderer.hpp"
#include "InputManager.hpp"
#include "../../layout/space/Space.hpp"
#include "../../layout/algorithm/Algorithm.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"

static bool sameWorkspaceIdentity(const State::Workspace::STarget& target, const PHLWORKSPACE& workspace) {
    return workspace && target.id && *target.id == workspace->id() && target.type == workspace->type() && target.address == workspace->addressableName();
}

bool CUnifiedWorkspaceSwipeGesture::isGestureInProgress() {
    return !!m_workspaceBegin;
}

void CUnifiedWorkspaceSwipeGesture::acquire(PHLWORKSPACE workspace) {
    if (!workspace)
        return;

    if (std::ranges::none_of(m_participants, [&workspace](const auto& participant) { return participant.workspace == workspace; }))
        m_participants.emplace_back(SParticipant{
            .workspace = workspace,
            .offset    = workspace->m_renderOffset->goal(),
            .alpha     = workspace->m_alpha->goal(),
            .forced    = workspace->m_forceRendering,
        });

    workspace->m_forceRendering = true;
}

void CUnifiedWorkspaceSwipeGesture::restore(const SParticipant& participant) {
    const auto WORKSPACE = participant.workspace.lock();
    const auto MONITOR   = m_monitor.lock();
    if (!WORKSPACE || !MONITOR || WORKSPACE->m_monitor != MONITOR)
        return;
    if (WORKSPACE == m_workspaceBegin ? MONITOR->m_activeWorkspace != WORKSPACE : WORKSPACE->visible())
        return; // Placement or activation has superseded this preview.

    g_pHyprRenderer->damageMonitor(MONITOR);
    WORKSPACE->m_renderOffset->setValueAndWarp(participant.offset);
    WORKSPACE->m_alpha->setValueAndWarp(participant.alpha);
    WORKSPACE->updateWindowDecos();
}

void CUnifiedWorkspaceSwipeGesture::release(PHLWORKSPACE workspace) {
    const auto IT = std::ranges::find_if(m_participants, [&workspace](const auto& participant) { return participant.workspace == workspace; });
    if (IT == m_participants.end())
        return;

    restore(*IT);
    workspace->m_forceRendering = IT->forced;
    m_participants.erase(IT);
}

void CUnifiedWorkspaceSwipeGesture::finishParticipants(PHLWORKSPACE animatedNeighbor) {
    for (const auto& participant : m_participants) {
        const auto WORKSPACE = participant.workspace.lock();
        if (!WORKSPACE)
            continue;
        if (WORKSPACE != m_workspaceBegin && WORKSPACE != animatedNeighbor)
            restore(participant);
        WORKSPACE->m_forceRendering = participant.forced;
    }
    m_participants.clear();
}

void CUnifiedWorkspaceSwipeGesture::releaseOtherParticipants(PHLWORKSPACE workspace) {
    std::erase_if(m_participants, [this, &workspace](const auto& participant) {
        const auto WORKSPACE = participant.workspace.lock();
        if (!WORKSPACE)
            return true;
        if (WORKSPACE == m_workspaceBegin || WORKSPACE == workspace)
            return false;

        restore(participant);
        WORKSPACE->m_forceRendering = participant.forced;
        return true;
    });
}

void CUnifiedWorkspaceSwipeGesture::restoreLayers() {
    const auto MONITOR = m_monitor.lock();
    if (!MONITOR || !MONITOR->m_activeWorkspace)
        return;

    const auto WORKSPACE = MONITOR->m_activeWorkspace;
    const auto FSWINDOW  = Fullscreen::controller()->getFullscreenWindow(WORKSPACE);
    const auto MODE      = FSWINDOW ? Fullscreen::controller()->getFullscreenModes(FSWINDOW).internal : Fullscreen::FSMODE_NONE;
    const auto SPACE     = WORKSPACE->space();
    const bool HIDE      = MODE == Fullscreen::FSMODE_FULLSCREEN &&
        (!FSWINDOW || !Fullscreen::controller()->layoutManagedFS(FSWINDOW) || (SPACE && SPACE->algorithm() && Fullscreen::controller()->hasFullscreen(WORKSPACE, true)));

    for (const auto& ls : MONITOR->m_layerSurfaceLayers[2]) {
        *ls->alpha()[Desktop::View::LS_ALPHA_FADE] = HIDE ? 0.F : 1.F;
    }
}

void CUnifiedWorkspaceSwipeGesture::reset() {
    m_workspaceBegin.reset();
    m_monitor.reset();
    m_delta            = 0;
    m_initialDirection = 0;
    m_avgSpeed         = 0;
    m_speedPoints      = 0;
}

void CUnifiedWorkspaceSwipeGesture::cancel() {
    for (const auto& participant : m_participants) {
        restore(participant);
        if (const auto WORKSPACE = participant.workspace.lock())
            WORKSPACE->m_forceRendering = participant.forced;
    }
    m_participants.clear();
    restoreLayers();
    reset();
}

void CUnifiedWorkspaceSwipeGesture::begin() {
    if (isGestureInProgress())
        return;

    const auto MONITOR = Desktop::focusState()->monitor();
    if (!MONITOR || !MONITOR->m_activeWorkspace)
        return;

    const auto PWORKSPACE = MONITOR->m_activeWorkspace;

    LOG(Log::DEBUG, "CUnifiedWorkspaceSwipeGesture::begin: Starting a swipe from {}", PWORKSPACE->displayName());

    m_workspaceBegin = PWORKSPACE;
    m_delta          = 0;
    m_monitor        = MONITOR;
    m_avgSpeed       = 0;
    m_speedPoints    = 0;

    const auto FSWINDOW         = Fullscreen::controller()->getFullscreenWindow(PWORKSPACE);
    const auto INTERNAL_FS_MODE = FSWINDOW ? Fullscreen::controller()->getFullscreenModes(FSWINDOW).internal : Fullscreen::FSMODE_NONE;

    if (INTERNAL_FS_MODE == Fullscreen::FSMODE_FULLSCREEN) {
        for (auto const& ls : MONITOR->m_layerSurfaceLayers[2]) {
            *ls->alpha()[Desktop::View::LS_ALPHA_FADE] = 1.F;
        }
    }
}

void CUnifiedWorkspaceSwipeGesture::update(double delta) {
    if (!isGestureInProgress())
        return;
    if (!m_monitor) {
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

    const auto   SWIPEDISTANCE = std::clamp(*PSWIPEDIST, sc<int64_t>(1LL), sc<int64_t>(UINT32_MAX));
    const auto   XDISTANCE     = m_monitor->m_size.x + *PWORKSPACEGAP;
    const auto   YDISTANCE     = m_monitor->m_size.y + *PWORKSPACEGAP;
    const auto   ANIMSTYLE     = m_workspaceBegin->m_renderOffset->getStyle();
    const bool   VERTANIMS     = ANIMSTYLE == "slidevert" || ANIMSTYLE.starts_with("slidefadevert");
    const double d             = m_delta - delta;
    m_delta                    = delta;

    m_avgSpeed = (m_avgSpeed * m_speedPoints + abs(d)) / (m_speedPoints + 1);
    m_speedPoints++;

    auto workspaceLeft  = State::Workspace::resolver()->getWorkspaceTargetFromString((*PSWIPEUSER ? "r-1" : "m-1"));
    auto workspaceRight = State::Workspace::resolver()->getWorkspaceTargetFromString((*PSWIPEUSER ? "r+1" : "m+1"));

    if (!workspaceLeft.valid() || !workspaceRight.valid() || (sameWorkspaceIdentity(workspaceLeft, m_workspaceBegin) && !*PSWIPENEW)) {
        cancel();
        return;
    }

    acquire(m_workspaceBegin);

    m_delta = std::clamp(m_delta, sc<double>(-SWIPEDISTANCE), sc<double>(SWIPEDISTANCE));

    const auto BEGIN_ID = m_workspaceBegin->numberedID();
    const auto LEFT_ID  = std::get_if<Workspace::SWorkspaceNumberedID>(&*workspaceLeft.id);
    const auto RIGHT_ID = std::get_if<Workspace::SWorkspaceNumberedID>(&*workspaceRight.id);
    if ((sameWorkspaceIdentity(workspaceLeft, m_workspaceBegin) && *PSWIPENEW && (m_delta < 0)) ||
        (m_delta > 0 && m_workspaceBegin->getWindowCount() == 0 && BEGIN_ID && RIGHT_ID && RIGHT_ID->value <= *BEGIN_ID) ||
        (m_delta < 0 && BEGIN_ID && LEFT_ID && *BEGIN_ID <= LEFT_ID->value)) {

        m_delta = 0;
        releaseOtherParticipants(nullptr);
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

    releaseOtherParticipants(State::Workspace::state()->find(m_delta < 0 ? workspaceLeft : workspaceRight));

    if (m_delta < 0) {
        const auto PWORKSPACE = State::Workspace::state()->find(workspaceLeft);

        if ((BEGIN_ID && LEFT_ID && LEFT_ID->value > *BEGIN_ID) || !PWORKSPACE) {
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

        acquire(PWORKSPACE);
        PWORKSPACE->m_alpha->setValueAndWarp(1.f);

        if (VERTANIMS) {
            PWORKSPACE->m_renderOffset->setValueAndWarp(Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE - YDISTANCE));
            m_workspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE));
        } else {
            PWORKSPACE->m_renderOffset->setValueAndWarp(Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE - XDISTANCE, 0.0));
            m_workspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE, 0.0));
        }

        PWORKSPACE->updateWindowDecos();
    } else {
        const auto PWORKSPACE = State::Workspace::state()->find(workspaceRight);

        if ((BEGIN_ID && RIGHT_ID && RIGHT_ID->value < *BEGIN_ID) || !PWORKSPACE) {
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

        acquire(PWORKSPACE);
        PWORKSPACE->m_alpha->setValueAndWarp(1.f);

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
    if (!m_monitor) {
        cancel();
        return;
    }

    static auto PSWIPEPERC    = CConfigValue<Config::FLOAT>("gestures:workspace_swipe_cancel_ratio");
    static auto PSWIPEDIST    = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_distance");
    static auto PSWIPEFORC    = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_min_speed_to_force");
    static auto PSWIPENEW     = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_create_new");
    static auto PSWIPEUSER    = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_use_r");
    static auto PWORKSPACEGAP = CConfigValue<Config::INTEGER>("general:gaps_workspaces");
    const auto  ANIMSTYLE     = m_workspaceBegin->m_renderOffset->getStyle();
    const bool  VERTANIMS     = ANIMSTYLE == "slidevert" || ANIMSTYLE.starts_with("slidefadevert");

    // commit
    auto       workspaceLeft  = State::Workspace::resolver()->getWorkspaceTargetFromString((*PSWIPEUSER ? "r-1" : "m-1"));
    auto       workspaceRight = State::Workspace::resolver()->getWorkspaceTargetFromString((*PSWIPEUSER ? "r+1" : "m+1"));
    const auto SWIPEDISTANCE  = std::clamp(*PSWIPEDIST, sc<int64_t>(1LL), sc<int64_t>(UINT32_MAX));

    // If we've been swiping off the right end with PSWIPENEW enabled, there is
    // no workspace there yet, and we need to choose an ID for a new one now.
    const auto BEGIN_ID = m_workspaceBegin->numberedID();
    const auto RIGHT_ID = workspaceRight.id ? std::get_if<Workspace::SWorkspaceNumberedID>(&*workspaceRight.id) : nullptr;
    if (BEGIN_ID && RIGHT_ID && RIGHT_ID->value <= *BEGIN_ID && *PSWIPENEW)
        workspaceRight = State::Workspace::resolver()->getWorkspaceTargetFromString("r+1");

    if (!workspaceLeft.valid() || !workspaceRight.valid()) {
        cancel();
        return;
    }

    auto       PWORKSPACER = State::Workspace::state()->find(workspaceRight); // not guaranteed if PSWIPENEW || PSWIPENUMBER
    auto       PWORKSPACEL = State::Workspace::state()->find(workspaceLeft);  // not guaranteed if PSWIPENUMBER

    const auto RENDEROFFSETMIDDLE = m_workspaceBegin->m_renderOffset->value();
    const auto XDISTANCE          = m_monitor->m_size.x + *PWORKSPACEGAP;
    const auto YDISTANCE          = m_monitor->m_size.y + *PWORKSPACEGAP;

    if ((abs(m_delta) < SWIPEDISTANCE * *PSWIPEPERC && (*PSWIPEFORC == 0 || (*PSWIPEFORC != 0 && m_avgSpeed < *PSWIPEFORC))) || abs(m_delta) < 2) {
        // revert
        if (abs(m_delta) < 2) {
            if (PWORKSPACEL != m_workspaceBegin)
                release(PWORKSPACEL);
            if (PWORKSPACER != m_workspaceBegin)
                release(PWORKSPACER);
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

    } else if (m_delta < 0) {
        // switch to left
        const auto RENDEROFFSET = PWORKSPACEL ? PWORKSPACEL->m_renderOffset->value() : Vector2D();

        if (PWORKSPACEL)
            m_monitor->changeWorkspace(PWORKSPACEL);
        else {
            PWORKSPACEL = State::Workspace::state()->create(workspaceLeft, m_monitor.lock());
            if (!PWORKSPACEL) {
                cancel();
                return;
            }
            m_monitor->changeWorkspace(PWORKSPACEL);
        }

        if (PWORKSPACEL) {
            PWORKSPACEL->m_renderOffset->setValue(RENDEROFFSET);
            PWORKSPACEL->m_alpha->setValueAndWarp(1.f);
        }

        m_workspaceBegin->m_renderOffset->setValue(RENDEROFFSETMIDDLE);
        if (VERTANIMS)
            *m_workspaceBegin->m_renderOffset = Vector2D(0.0, YDISTANCE);
        else
            *m_workspaceBegin->m_renderOffset = Vector2D(XDISTANCE, 0.0);
        m_workspaceBegin->m_alpha->setValueAndWarp(1.f);

        g_pInputManager->unconstrainMouse();

        LOG(Log::DEBUG, "Ended swipe to the left");

    } else {
        // switch to right
        const auto RENDEROFFSET = PWORKSPACER ? PWORKSPACER->m_renderOffset->value() : Vector2D();

        if (PWORKSPACER)
            m_monitor->changeWorkspace(PWORKSPACER);
        else {
            PWORKSPACER = State::Workspace::state()->create(workspaceRight, m_monitor.lock());
            if (!PWORKSPACER) {
                cancel();
                return;
            }
            m_monitor->changeWorkspace(PWORKSPACER);
        }

        if (PWORKSPACER) {
            PWORKSPACER->m_renderOffset->setValue(RENDEROFFSET);
            PWORKSPACER->m_alpha->setValueAndWarp(1.f);
        }

        m_workspaceBegin->m_renderOffset->setValue(RENDEROFFSETMIDDLE);
        if (VERTANIMS)
            *m_workspaceBegin->m_renderOffset = Vector2D(0.0, -YDISTANCE);
        else
            *m_workspaceBegin->m_renderOffset = Vector2D(-XDISTANCE, 0.0);
        m_workspaceBegin->m_alpha->setValueAndWarp(1.f);

        g_pInputManager->unconstrainMouse();

        LOG(Log::DEBUG, "Ended swipe to the right");
    }

    g_pHyprRenderer->damageMonitor(m_monitor.lock());

    finishParticipants(m_delta < 0 ? PWORKSPACEL : PWORKSPACER);
    restoreLayers();
    reset();
    g_pInputManager->refocus();
}
