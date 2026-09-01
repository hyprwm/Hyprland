#include "WorkspaceSwipeGesture.hpp"

#include "../../../../Compositor.hpp"
#include "../../../../config/ConfigManager.hpp"
#include "../../../../state/WorkspaceState.hpp"
#include "../../../../desktop/state/FocusState.hpp"
#include "../../../../overview/Overview.hpp"
#include "../../../../render/Renderer.hpp"

#include "../../UnifiedWorkspaceSwipeGesture.hpp"

static float overviewMoveDelta(float delta) {
    if (!Config::mgr())
        return delta;

    static auto PSWIPEINVR = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_invert");
    return *PSWIPEINVR ? delta : -delta;
}

void CWorkspaceSwipeGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e);

    m_overview           = nullptr;
    m_overviewMoveActive = false;

    const auto OVERVIEW = Overview::overview().get();
    if (OVERVIEW && OVERVIEW->isOpen()) {
        m_overview = OVERVIEW;
        if (const auto MOVABLE = dynamic_cast<Overview::IOverviewGestureMovable*>(OVERVIEW))
            m_overviewMoveActive = MOVABLE->beginMoveGesture();
        return;
    }

    static auto PSWIPENEW = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_create_new");

    if (g_pSessionLockManager->isSessionLocked() || g_pUnifiedWorkspaceSwipe->isGestureInProgress())
        return;

    int onMonitor = 0;
    for (auto const& w : State::workspaceState()->workspaces()) {
        if (w->m_monitor == Desktop::focusState()->monitor() && !State::workspaceState()->isSpecial(w->m_id))
            onMonitor++;
    }

    if (onMonitor < 2 && !*PSWIPENEW)
        return; // disallow swiping when there's 1 workspace on a monitor

    g_pUnifiedWorkspaceSwipe->begin();
}

void CWorkspaceSwipeGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    if (m_overview) {
        if (Overview::overview().get() == m_overview && m_overviewMoveActive) {
            if (const auto MOVABLE = dynamic_cast<Overview::IOverviewGestureMovable*>(m_overview))
                MOVABLE->updateMoveGesture(overviewMoveDelta(distance(e)));
        }
        return;
    }

    if (!g_pUnifiedWorkspaceSwipe->isGestureInProgress())
        return;

    const float  DELTA = distance(e);

    static auto  PSWIPEINVR = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_invert");

    const double D = g_pUnifiedWorkspaceSwipe->m_delta + (*PSWIPEINVR ? -DELTA : DELTA);
    g_pUnifiedWorkspaceSwipe->update(D);
}

void CWorkspaceSwipeGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    if (m_overview) {
        if (Overview::overview().get() == m_overview && m_overviewMoveActive) {
            if (const auto MOVABLE = dynamic_cast<Overview::IOverviewGestureMovable*>(m_overview))
                MOVABLE->endMoveGesture();
        }

        m_overview           = nullptr;
        m_overviewMoveActive = false;
        return;
    }

    if (!g_pUnifiedWorkspaceSwipe->isGestureInProgress())
        return;

    g_pUnifiedWorkspaceSwipe->end();
}

bool CWorkspaceSwipeGesture::isDirectionSensitive() {
    return true;
}
