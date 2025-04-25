#include "InputManager.hpp"
#include "../../Compositor.hpp"
#include "../../protocols/IdleInhibit.hpp"
#include "../../protocols/IdleNotify.hpp"
#include "../../protocols/core/Compositor.hpp"

void CInputManager::newIdleInhibitor(std::any inhibitor) {
    const auto PINHIBIT = m_vIdleInhibitors.emplace_back(makeUnique<SIdleInhibitor>()).get();
    PINHIBIT->inhibitor = std::any_cast<SP<CIdleInhibitor>>(inhibitor);

    Debug::log(LOG, "New idle inhibitor registered for surface {:x}", (uintptr_t)PINHIBIT->inhibitor->surface.get());

    PINHIBIT->inhibitor->listeners.destroy = PINHIBIT->inhibitor->resource->events.destroy.registerListener([this, PINHIBIT](std::any data) {
        std::erase_if(m_vIdleInhibitors, [PINHIBIT](const auto& other) { return other.get() == PINHIBIT; });
        recheckIdleInhibitorStatus();
    });

    auto WLSurface = CWLSurface::fromResource(PINHIBIT->inhibitor->surface.lock());

    if (!WLSurface) {
        Debug::log(LOG, "Inhibitor has no HL Surface attached to it, likely meaning it's a non-desktop element. Assuming it's visible.");
        PINHIBIT->nonDesktop = true;
        recheckIdleInhibitorStatus();
        return;
    }

    PINHIBIT->surfaceDestroyListener = WLSurface->m_events.destroy.registerListener(
        [this, PINHIBIT](std::any data) { std::erase_if(m_vIdleInhibitors, [PINHIBIT](const auto& other) { return other.get() == PINHIBIT; }); });

    recheckIdleInhibitorStatus();
}

void CInputManager::recheckIdleInhibitorStatus() {

    for (auto const& ii : m_vIdleInhibitors) {
        if (ii->nonDesktop) {
            PROTO::idle->setInhibit(true);
            return;
        }

        auto WLSurface = CWLSurface::fromResource(ii->inhibitor->surface.lock());

        if (!WLSurface)
            continue;

        if (WLSurface->visible()) {
            PROTO::idle->setInhibit(true);
            return;
        }
    }

    // check manual user-set inhibitors
    for (auto const& w : g_pCompositor->m_windows) {
        if (isWindowInhibiting(w)) {
            PROTO::idle->setInhibit(true);
            return;
        }
    }

    PROTO::idle->setInhibit(false);
}

bool CInputManager::isWindowInhibiting(const PHLWINDOW& w, bool onlyHl) {
    if (w->m_eIdleInhibitMode == IDLEINHIBIT_ALWAYS)
        return true;

    if (w->m_eIdleInhibitMode == IDLEINHIBIT_FOCUS && g_pCompositor->isWindowActive(w))
        return true;

    if (w->m_eIdleInhibitMode == IDLEINHIBIT_FULLSCREEN && w->isFullscreen() && w->m_pWorkspace && w->m_pWorkspace->isVisible())
        return true;

    if (onlyHl)
        return false;

    for (auto const& ii : m_vIdleInhibitors) {
        if (ii->nonDesktop || !ii->inhibitor)
            continue;

        bool isInhibiting = false;
        w->m_pWLSurface->resource()->breadthfirst(
            [&ii](SP<CWLSurfaceResource> surf, const Vector2D& pos, void* data) {
                if (ii->inhibitor->surface != surf)
                    return;

                auto WLSurface = CWLSurface::fromResource(surf);

                if (!WLSurface)
                    return;

                if (WLSurface->visible())
                    *(bool*)data = true;
            },
            &isInhibiting);

        if (isInhibiting)
            return true;
    }

    return false;
}
