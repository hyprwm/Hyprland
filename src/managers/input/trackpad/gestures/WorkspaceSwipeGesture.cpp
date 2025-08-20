#include "WorkspaceSwipeGesture.hpp"

#include "../../../../Compositor.hpp"
#include "../../../../managers/input/InputManager.hpp"
#include "../../../../render/Renderer.hpp"
#include "../../../../helpers/Monitor.hpp"
#include "../../../../desktop/LayerSurface.hpp"

void CWorkspaceSwipeGesture::begin(const IPointer::SSwipeUpdateEvent& e) {
    static auto PSWIPENEW = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_create_new");

    if (g_pSessionLockManager->isSessionLocked())
        return;

    int onMonitor = 0;
    for (auto const& w : g_pCompositor->getWorkspaces()) {
        if (w->m_monitor == g_pCompositor->m_lastMonitor && !g_pCompositor->isWorkspaceSpecial(w->m_id))
            onMonitor++;
    }

    if (onMonitor < 2 && !*PSWIPENEW)
        return; // disallow swiping when there's 1 workspace on a monitor

    g_pInputManager->beginWorkspaceSwipe();
}

void CWorkspaceSwipeGesture::update(const IPointer::SSwipeUpdateEvent& e) {
    if (!g_pInputManager->m_activeSwipe.pWorkspaceBegin)
        return;

    static auto  PSWIPEINVR = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_invert");
    const auto   ANIMSTYLE  = g_pInputManager->m_activeSwipe.pWorkspaceBegin->m_renderOffset->getStyle();
    const bool   VERTANIMS  = ANIMSTYLE == "slidevert" || ANIMSTYLE.starts_with("slidefadevert");

    const double D = g_pInputManager->m_activeSwipe.delta + (VERTANIMS ? (*PSWIPEINVR ? -e.delta.y : e.delta.y) : (*PSWIPEINVR ? -e.delta.x : e.delta.x));
    g_pInputManager->updateWorkspaceSwipe(D);
}

void CWorkspaceSwipeGesture::end(const IPointer::SSwipeEndEvent& e) {
    if (!g_pInputManager->m_activeSwipe.pWorkspaceBegin)
        return;

    g_pInputManager->endWorkspaceSwipe();
}
