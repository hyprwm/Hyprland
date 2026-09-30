#include "WorkspaceSwipeGesture.hpp"

#include "../../../../Compositor.hpp"
#include "../../../../state/WorkspaceState.hpp"
#include "../../../../desktop/state/FocusState.hpp"
#include "../../../../render/Renderer.hpp"

#include "../../UnifiedWorkspaceSwipeGesture.hpp"

CWorkspaceSwipeGesture::~CWorkspaceSwipeGesture() {
    cancel();
}

void CWorkspaceSwipeGesture::cancel() {
    const auto SESSION = m_sessionID;
    m_sessionID        = 0;
    if (SESSION && g_pUnifiedWorkspaceSwipe && SESSION == g_pUnifiedWorkspaceSwipe->sessionID())
        g_pUnifiedWorkspaceSwipe->cancel();
}

void CWorkspaceSwipeGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    cancel();
    ITrackpadGesture::begin(e);

    static auto PSWIPENEW = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_create_new");

    if (g_pSessionLockManager->isSessionLocked() || g_pUnifiedWorkspaceSwipe->isGestureInProgress())
        return;

    const auto MONITOR = Desktop::focusState()->monitor();
    if (!MONITOR)
        return;

    int onMonitor = 0;
    for (auto const& w : State::Workspace::state()->workspaces()) {
        if (w->m_monitor == MONITOR && w->type() != Workspace::eWorkspaceType::SPECIAL)
            onMonitor++;
    }

    if (onMonitor < 2 && !*PSWIPENEW)
        return; // disallow swiping when there's 1 workspace on a monitor

    if (g_pUnifiedWorkspaceSwipe->begin(MONITOR))
        m_sessionID = g_pUnifiedWorkspaceSwipe->sessionID();
}

void CWorkspaceSwipeGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    if (!m_sessionID || m_sessionID != g_pUnifiedWorkspaceSwipe->sessionID())
        return;

    const float  DELTA = distance(e);

    static auto  PSWIPEINVR = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_invert");

    const double D = g_pUnifiedWorkspaceSwipe->m_delta + (*PSWIPEINVR ? -DELTA : DELTA);
    g_pUnifiedWorkspaceSwipe->update(D);
}

void CWorkspaceSwipeGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    const auto SESSION = m_sessionID;
    m_sessionID        = 0;
    if (!SESSION || SESSION != g_pUnifiedWorkspaceSwipe->sessionID())
        return;

    g_pUnifiedWorkspaceSwipe->end();
}

bool CWorkspaceSwipeGesture::isDirectionSensitive() {
    return true;
}
