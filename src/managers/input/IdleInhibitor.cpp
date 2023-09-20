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
        Debug::log(LOG, "IdleInhibitor got window {}", PINHIBIT->pWindow);

    recheckIdleInhibitorStatus();
}

void CInputManager::recheckIdleInhibitorStatus() {

    for (auto& ii : m_lIdleInhibitors) {
        if (!ii.pWindow) {
            g_pCompositor->setIdleActivityInhibit(false);
            return;
        } else if (g_pHyprRenderer->shouldRenderWindow(ii.pWindow)) {
            g_pCompositor->setIdleActivityInhibit(false);
            return;
        }
    }

    // check manual user-set inhibitors
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_eIdleInhibitMode == IDLEINHIBIT_NONE)
            continue;

        if (w->m_eIdleInhibitMode == IDLEINHIBIT_ALWAYS) {
            g_pCompositor->setIdleActivityInhibit(false);
            return;
        }

        if (w->m_eIdleInhibitMode == IDLEINHIBIT_FOCUS && g_pCompositor->isWindowActive(w.get())) {
            g_pCompositor->setIdleActivityInhibit(false);
            return;
        }

        if (w->m_eIdleInhibitMode == IDLEINHIBIT_FULLSCREEN && w->m_bIsFullscreen && g_pCompositor->isWorkspaceVisible(w->m_iWorkspaceID)) {
            g_pCompositor->setIdleActivityInhibit(false);
            return;
        }
    }

    g_pCompositor->setIdleActivityInhibit(true);
    return;
}