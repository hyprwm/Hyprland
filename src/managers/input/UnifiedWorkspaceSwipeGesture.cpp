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

uint64_t CUnifiedWorkspaceSwipeGesture::sessionID() const {
    return m_sessionID;
}

bool CUnifiedWorkspaceSwipeGesture::validOrigin() const {
    const auto MONITOR = m_monitor.lock();
    return MONITOR && MONITOR->m_enabled && m_workspaceBegin && m_workspaceBegin->m_monitor == MONITOR && MONITOR->m_activeWorkspace == m_workspaceBegin;
}

void CUnifiedWorkspaceSwipeGesture::resetListeners() {
    m_listeners.monitorDisconnect.reset();
    m_listeners.monitorDestroy.reset();
    m_listeners.workspaceMonitorChanged.reset();
    m_listeners.workspaceActiveChanged.reset();
}

CUnifiedWorkspaceSwipeGesture::SParticipant& CUnifiedWorkspaceSwipeGesture::acquire(PHLWORKSPACE workspace) {
    const auto IT = std::ranges::find_if(m_participants, [&workspace](const auto& participant) { return participant.workspace == workspace; });
    if (IT != m_participants.end()) {
        workspace->m_forceRendering = true;
        return *IT;
    }

    auto& participant           = m_participants.emplace_back(SParticipant{
        .workspace = workspace,
        .offset    = workspace->m_renderOffset->goal(),
        .alpha     = workspace->m_alpha->goal(),
        .forced    = workspace->m_forceRendering,
    });
    workspace->m_forceRendering = true;
    return participant;
}

void CUnifiedWorkspaceSwipeGesture::setPreviewOffset(PHLWORKSPACE workspace, const Vector2D& offset) {
    acquire(workspace).previewOffset = offset;
    workspace->m_renderOffset->setValueAndWarp(offset);
}

void CUnifiedWorkspaceSwipeGesture::setPreviewAlpha(PHLWORKSPACE workspace, float alpha) {
    acquire(workspace).previewAlpha = alpha;
    workspace->m_alpha->setValueAndWarp(alpha);
}

void CUnifiedWorkspaceSwipeGesture::restore(const SParticipant& participant) {
    const auto WORKSPACE = participant.workspace.lock();
    const auto MONITOR   = WORKSPACE ? WORKSPACE->m_monitor.lock() : nullptr;
    if (!WORKSPACE || !MONITOR)
        return;
    if (WORKSPACE != m_workspaceBegin && (WORKSPACE->visible() || MONITOR->m_activeWorkspace == WORKSPACE))
        return; // An activated neighbor must not regain its hidden preview baseline.

    // Placement may move an untouched preview or replace it with a new animation.
    // Restore only properties that still belong to this gesture, independently.
    const bool OFFSET = participant.previewOffset && !WORKSPACE->m_renderOffset->isBeingAnimated() && WORKSPACE->m_renderOffset->value() == *participant.previewOffset &&
        WORKSPACE->m_renderOffset->goal() == *participant.previewOffset;
    const bool ALPHA = participant.previewAlpha && !WORKSPACE->m_alpha->isBeingAnimated() && WORKSPACE->m_alpha->value() == *participant.previewAlpha &&
        WORKSPACE->m_alpha->goal() == *participant.previewAlpha;
    if (!OFFSET && !ALPHA)
        return;

    if (const auto ORIGINAL = m_monitor.lock(); ORIGINAL && ORIGINAL != MONITOR)
        g_pHyprRenderer->damageMonitor(ORIGINAL);
    g_pHyprRenderer->damageMonitor(MONITOR);
    if (OFFSET)
        WORKSPACE->m_renderOffset->setValueAndWarp(participant.offset);
    if (ALPHA)
        WORKSPACE->m_alpha->setValueAndWarp(participant.alpha);
    WORKSPACE->updateWindowDecos();
}

void CUnifiedWorkspaceSwipeGesture::release(PHLWORKSPACE workspace) {
    if (!workspace)
        return;

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
    resetListeners();
    m_workspaceBegin.reset();
    m_monitor.reset();
    m_delta            = 0;
    m_initialDirection = 0;
    m_avgSpeed         = 0;
    m_speedPoints      = 0;
    m_sessionID        = 0;
}

void CUnifiedWorkspaceSwipeGesture::cancel() {
    resetListeners();
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
    begin(Desktop::focusState()->monitor());
}

bool CUnifiedWorkspaceSwipeGesture::begin(PHLMONITOR monitor) {
    if (isGestureInProgress())
        return false;

    const auto MONITOR = monitor;
    if (!MONITOR || !MONITOR->m_enabled || !MONITOR->m_activeWorkspace)
        return false;

    const auto PWORKSPACE = MONITOR->m_activeWorkspace;

    LOG(Log::DEBUG, "CUnifiedWorkspaceSwipeGesture::begin: Starting a swipe from {}", PWORKSPACE->displayName());

    m_workspaceBegin   = PWORKSPACE;
    m_delta            = 0;
    m_monitor          = MONITOR;
    m_avgSpeed         = 0;
    m_speedPoints      = 0;
    m_initialDirection = 0;
    m_sessionID        = ++m_sessionSerial;

    const auto VALIDATE = [this] {
        if (!validOrigin())
            cancel();
    };
    m_listeners.monitorDisconnect       = MONITOR->m_events.disconnect.listen([this] { cancel(); });
    m_listeners.monitorDestroy          = MONITOR->m_events.destroy.listen([this] { cancel(); });
    m_listeners.workspaceMonitorChanged = PWORKSPACE->m_events.monitorChanged.listen(VALIDATE);
    m_listeners.workspaceActiveChanged  = PWORKSPACE->m_events.activeChanged.listen(VALIDATE);

    const auto FSWINDOW         = Fullscreen::controller()->getFullscreenWindow(PWORKSPACE);
    const auto INTERNAL_FS_MODE = FSWINDOW ? Fullscreen::controller()->getFullscreenModes(FSWINDOW).internal : Fullscreen::FSMODE_NONE;

    if (INTERNAL_FS_MODE == Fullscreen::FSMODE_FULLSCREEN) {
        for (auto const& ls : MONITOR->m_layerSurfaceLayers[2]) {
            *ls->alpha()[Desktop::View::LS_ALPHA_FADE] = 1.F;
        }
    }
    return true;
}

void CUnifiedWorkspaceSwipeGesture::update(double delta) {
    if (!isGestureInProgress())
        return;
    if (!validOrigin()) {
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

    auto workspaceLeft  = State::Workspace::resolver()->getWorkspaceTargetFromString((*PSWIPEUSER ? "r-1" : "m-1"), m_monitor.lock());
    auto workspaceRight = State::Workspace::resolver()->getWorkspaceTargetFromString((*PSWIPEUSER ? "r+1" : "m+1"), m_monitor.lock());

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
        setPreviewOffset(m_workspaceBegin, Vector2D(0.0, 0.0));
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
                    setPreviewOffset(m_workspaceBegin, Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE));
                else
                    setPreviewOffset(m_workspaceBegin, Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE, 0.0));

                m_workspaceBegin->updateWindowDecos();
                return;
            }
            m_delta = 0;
            return;
        }

        setPreviewAlpha(PWORKSPACE, 1.f);

        if (VERTANIMS) {
            setPreviewOffset(PWORKSPACE, Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE - YDISTANCE));
            setPreviewOffset(m_workspaceBegin, Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE));
        } else {
            setPreviewOffset(PWORKSPACE, Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE - XDISTANCE, 0.0));
            setPreviewOffset(m_workspaceBegin, Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE, 0.0));
        }

        PWORKSPACE->updateWindowDecos();
    } else {
        const auto PWORKSPACE = State::Workspace::state()->find(workspaceRight);

        if ((BEGIN_ID && RIGHT_ID && RIGHT_ID->value < *BEGIN_ID) || !PWORKSPACE) {
            if (*PSWIPENEW) {
                g_pHyprRenderer->damageMonitor(m_monitor.lock());

                if (VERTANIMS)
                    setPreviewOffset(m_workspaceBegin, Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE));
                else
                    setPreviewOffset(m_workspaceBegin, Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE, 0.0));

                m_workspaceBegin->updateWindowDecos();
                return;
            }
            m_delta = 0;
            return;
        }

        setPreviewAlpha(PWORKSPACE, 1.f);

        if (VERTANIMS) {
            setPreviewOffset(PWORKSPACE, Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE + YDISTANCE));
            setPreviewOffset(m_workspaceBegin, Vector2D(0.0, ((-m_delta) / SWIPEDISTANCE) * YDISTANCE));
        } else {
            setPreviewOffset(PWORKSPACE, Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE + XDISTANCE, 0.0));
            setPreviewOffset(m_workspaceBegin, Vector2D(((-m_delta) / SWIPEDISTANCE) * XDISTANCE, 0.0));
        }

        PWORKSPACE->updateWindowDecos();
    }

    g_pHyprRenderer->damageMonitor(m_monitor.lock());

    m_workspaceBegin->updateWindowDecos();

    if (*PSWIPEFOREVER) {
        if (abs(m_delta) >= SWIPEDISTANCE) {
            const auto MONITOR = m_monitor.lock();
            const auto SESSION = m_sessionID;
            const auto SERIAL  = m_sessionSerial;
            // Refocus callbacks may start (and even finish) a replacement after our reset.
            if (endSegment() && !isGestureInProgress() && m_sessionSerial == SERIAL && begin(MONITOR))
                m_sessionID = SESSION;
        }
    }
}

void CUnifiedWorkspaceSwipeGesture::end() {
    endSegment();
}

bool CUnifiedWorkspaceSwipeGesture::endSegment() {
    if (!isGestureInProgress())
        return false;
    if (!validOrigin()) {
        cancel();
        return false;
    }

    const auto  SESSION      = m_sessionID;
    const auto  ORIGIN       = m_workspaceBegin;
    const auto  MONITOR      = m_monitor.lock();
    const auto  OWNS_SESSION = [this, SESSION, &ORIGIN, &MONITOR] { return m_sessionID == SESSION && m_workspaceBegin == ORIGIN && m_monitor == MONITOR; };

    static auto PSWIPEPERC    = CConfigValue<Config::FLOAT>("gestures:workspace_swipe_cancel_ratio");
    static auto PSWIPEDIST    = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_distance");
    static auto PSWIPEFORC    = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_min_speed_to_force");
    static auto PSWIPENEW     = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_create_new");
    static auto PSWIPEUSER    = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_use_r");
    static auto PWORKSPACEGAP = CConfigValue<Config::INTEGER>("general:gaps_workspaces");
    const auto  ANIMSTYLE     = m_workspaceBegin->m_renderOffset->getStyle();
    const bool  VERTANIMS     = ANIMSTYLE == "slidevert" || ANIMSTYLE.starts_with("slidefadevert");

    // commit
    auto       workspaceLeft  = State::Workspace::resolver()->getWorkspaceTargetFromString((*PSWIPEUSER ? "r-1" : "m-1"), m_monitor.lock());
    auto       workspaceRight = State::Workspace::resolver()->getWorkspaceTargetFromString((*PSWIPEUSER ? "r+1" : "m+1"), m_monitor.lock());
    const auto SWIPEDISTANCE  = std::clamp(*PSWIPEDIST, sc<int64_t>(1LL), sc<int64_t>(UINT32_MAX));

    // If we've been swiping off the right end with PSWIPENEW enabled, there is
    // no workspace there yet, and we need to choose an ID for a new one now.
    const auto BEGIN_ID = m_workspaceBegin->numberedID();
    const auto RIGHT_ID = workspaceRight.id ? std::get_if<Workspace::SWorkspaceNumberedID>(&*workspaceRight.id) : nullptr;
    if (BEGIN_ID && RIGHT_ID && RIGHT_ID->value <= *BEGIN_ID && *PSWIPENEW)
        workspaceRight = State::Workspace::resolver()->getWorkspaceTargetFromString("r+1", m_monitor.lock());

    if (!workspaceLeft.valid() || !workspaceRight.valid()) {
        cancel();
        return false;
    }

    // Our own activation emits the same signals that invalidate an externally replaced session.
    resetListeners();

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

        if (!PWORKSPACEL) {
            PWORKSPACEL = State::Workspace::state()->create(workspaceLeft, MONITOR);
            if (!OWNS_SESSION())
                return false;
            if (!PWORKSPACEL || !validOrigin()) {
                cancel();
                return false;
            }
        }
        MONITOR->changeWorkspace(PWORKSPACEL);
        if (!OWNS_SESSION())
            return false;

        if (!MONITOR->m_enabled || ORIGIN->m_monitor != MONITOR || MONITOR->m_activeWorkspace != PWORKSPACEL) {
            cancel();
            return false;
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

        if (!PWORKSPACER) {
            PWORKSPACER = State::Workspace::state()->create(workspaceRight, MONITOR);
            if (!OWNS_SESSION())
                return false;
            if (!PWORKSPACER || !validOrigin()) {
                cancel();
                return false;
            }
        }
        MONITOR->changeWorkspace(PWORKSPACER);
        if (!OWNS_SESSION())
            return false;

        if (!MONITOR->m_enabled || ORIGIN->m_monitor != MONITOR || MONITOR->m_activeWorkspace != PWORKSPACER) {
            cancel();
            return false;
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
    return true;
}
