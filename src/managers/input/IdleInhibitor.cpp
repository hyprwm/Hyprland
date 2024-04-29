#include "InputManager.hpp"
#include "../../Compositor.hpp"
#include "../../protocols/IdleInhibit.hpp"
#include "../../protocols/IdleNotify.hpp"

void CInputManager::newIdleInhibitor(std::any inhibitor) {
    const auto PINHIBIT = m_vIdleInhibitors.emplace_back(std::make_unique<SIdleInhibitor>()).get();
    PINHIBIT->inhibitor = std::any_cast<std::shared_ptr<CIdleInhibitor>>(inhibitor);

    Debug::log(LOG, "New idle inhibitor registered for surface {:x}", (uintptr_t)PINHIBIT->inhibitor->surface);

    PINHIBIT->inhibitor->listeners.destroy = PINHIBIT->inhibitor->resource.lock()->events.destroy.registerListener([this, PINHIBIT](std::any data) {
        std::erase_if(m_vIdleInhibitors, [PINHIBIT](const auto& other) { return other.get() == PINHIBIT; });
        recheckIdleInhibitorStatus();
    });

    const auto PWINDOW = g_pCompositor->getWindowFromSurface(PINHIBIT->inhibitor->surface);

    if (PWINDOW) {
        PINHIBIT->pWindow               = PWINDOW;
        PINHIBIT->windowDestroyListener = PWINDOW->events.destroy.registerListener([PINHIBIT](std::any data) {
            Debug::log(WARN, "Inhibitor got its window destroyed before its inhibitor resource.");
            PINHIBIT->pWindow.reset();
        });
    } else
        Debug::log(WARN, "Inhibitor is for no window?");
    recheckIdleInhibitorStatus();
}

void CInputManager::recheckIdleInhibitorStatus() {

    for (auto& ii : m_vIdleInhibitors) {
        if (ii->pWindow.expired())
            continue;

        if (g_pHyprRenderer->shouldRenderWindow(ii->pWindow.lock())) {
            PROTO::idle->setInhibit(true);
            return;
        }
    }

    // check manual user-set inhibitors
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_eIdleInhibitMode == IDLEINHIBIT_NONE)
            continue;

        if (w->m_eIdleInhibitMode == IDLEINHIBIT_ALWAYS) {
            PROTO::idle->setInhibit(true);
            return;
        }

        if (w->m_eIdleInhibitMode == IDLEINHIBIT_FOCUS && g_pCompositor->isWindowActive(w)) {
            PROTO::idle->setInhibit(true);
            return;
        }

        if (w->m_eIdleInhibitMode == IDLEINHIBIT_FULLSCREEN && w->m_bIsFullscreen && g_pCompositor->isWorkspaceVisible(w->m_pWorkspace)) {
            PROTO::idle->setInhibit(true);
            return;
        }
    }

    PROTO::idle->setInhibit(false);
    return;
}
