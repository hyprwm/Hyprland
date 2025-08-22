#include "WorkspaceSwipeGesture.hpp"

#include "../../../../Compositor.hpp"
#include "../../../../managers/input/InputManager.hpp"
#include "../../../../render/Renderer.hpp"

#include "../../UnifiedWorkspaceSwipeGesture.hpp"

void CWorkspaceSwipeGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e);

    static auto PSWIPENEW = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_create_new");

    if (g_pSessionLockManager->isSessionLocked() || g_pUnifiedWorkspaceSwipe->isGestureInProgress())
        return;

    int onMonitor = 0;
    for (auto const& w : g_pCompositor->getWorkspaces()) {
        if (w->m_monitor == g_pCompositor->m_lastMonitor && !g_pCompositor->isWorkspaceSpecial(w->m_id))
            onMonitor++;
    }

    if (onMonitor < 2 && !*PSWIPENEW)
        return; // disallow swiping when there's 1 workspace on a monitor

    g_pUnifiedWorkspaceSwipe->begin();
}

void CWorkspaceSwipeGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    if (!g_pUnifiedWorkspaceSwipe->isGestureInProgress())
        return;

    const float  DELTA = distance(e);

    static auto  PSWIPEINVR = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_invert");
    const auto   ANIMSTYLE  = g_pUnifiedWorkspaceSwipe->m_workspaceBegin->m_renderOffset->getStyle();
    const bool   VERTANIMS  = ANIMSTYLE == "slidevert" || ANIMSTYLE.starts_with("slidefadevert");

    const double D = g_pUnifiedWorkspaceSwipe->m_delta + (VERTANIMS ? (*PSWIPEINVR ? -DELTA : DELTA) : (*PSWIPEINVR ? -DELTA : DELTA));
    g_pUnifiedWorkspaceSwipe->update(D);
}

void CWorkspaceSwipeGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    if (!g_pUnifiedWorkspaceSwipe->isGestureInProgress())
        return;

    g_pUnifiedWorkspaceSwipe->end();
}
