#include "InputManager.hpp"
#include "../../Compositor.hpp"

void Events::listener_newIdleInhibitor(wl_listener* listener, void* data) {
    const auto WLRIDLEINHIBITOR = (wlr_idle_inhibitor_v1*)data;

    if (!WLRIDLEINHIBITOR)
        return;

    g_pInputManager->newIdleInhibitor(WLRIDLEINHIBITOR);
}

void CInputManager::newIdleInhibitor(wlr_idle_inhibitor_v1* pInhibitor) {
    const auto PINHIBIT = &m_lIdleInhibitors.emplace_back();

    Debug::log(LOG, "New idle inhibitor registered");

    PINHIBIT->pWlrInhibitor = pInhibitor;

    PINHIBIT->hyprListener_Destroy.initCallback(
        &pInhibitor->events.destroy,
        [](void* owner, void* data) {
            const auto PINH = (SIdleInhibitor*)owner;

            g_pInputManager->m_lIdleInhibitors.remove(*PINH);

            Debug::log(LOG, "Destroyed an idleinhibitor");

            g_pInputManager->recheckIdleInhibitorStatus();
        },
        PINHIBIT, "IdleInhibitor");

    PINHIBIT->pWindow = g_pCompositor->getWindowFromSurface(pInhibitor->surface);

    if (PINHIBIT->pWindow)
        Debug::log(LOG, "IdleInhibitor got window %x (%s)", PINHIBIT->pWindow, PINHIBIT->pWindow->m_szTitle.c_str());

    recheckIdleInhibitorStatus();
}

void CInputManager::recheckIdleInhibitorStatus() {

    for (auto& ii : m_lIdleInhibitors) {
        if (!ii.pWindow) {
            wlr_idle_set_enabled(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sSeat.seat, false);
            return;
        } else if (g_pHyprRenderer->shouldRenderWindow(ii.pWindow)) {
            wlr_idle_set_enabled(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sSeat.seat, false);
            return;
        }
    }

    // check manual user-set inhibitors
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_eIdleInhibitMode == IDLEINHIBIT_NONE)
            continue;

        if (w->m_eIdleInhibitMode == IDLEINHIBIT_ALWAYS) {
            wlr_idle_set_enabled(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sSeat.seat, false);
            return;
        }

        if (w->m_eIdleInhibitMode == IDLEINHIBIT_FOCUS && g_pCompositor->isWindowActive(w.get())) {
            wlr_idle_set_enabled(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sSeat.seat, false);
            return;
        }

        if (w->m_eIdleInhibitMode == IDLEINHIBIT_FULLSCREEN && w->m_bIsFullscreen && g_pCompositor->isWorkspaceVisible(w->m_iWorkspaceID)) {
            wlr_idle_set_enabled(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sSeat.seat, false);
            return;
        }
    }

    wlr_idle_set_enabled(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sSeat.seat, true);
    return;
}