#include "InputManager.hpp"
#include "../../Compositor.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../desktop/view/types/AlphaModifiable.hpp"
#include "../../protocols/IdleInhibit.hpp"
#include "../../protocols/IdleNotify.hpp"
#include "../../protocols/core/Compositor.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"

void CInputManager::newIdleInhibitor(std::any inhibitor) {
    const auto PINHIBIT = m_idleInhibitors.emplace_back(makeUnique<SIdleInhibitor>()).get();
    PINHIBIT->inhibitor = std::any_cast<SP<CIdleInhibitor>>(inhibitor);

    LOG(Log::DEBUG, "New idle inhibitor registered for surface {:x}", rc<uintptr_t>(PINHIBIT->inhibitor->m_surface.get()));

    PINHIBIT->inhibitor->m_listeners.destroy = PINHIBIT->inhibitor->m_resource->m_events.destroy.listen([this, PINHIBIT] {
        std::erase_if(m_idleInhibitors, [PINHIBIT](const auto& other) { return other.get() == PINHIBIT; });
        recheckIdleInhibitorStatus();
    });

    auto WLSurface = Desktop::View::CWLSurface::fromResource(PINHIBIT->inhibitor->m_surface.lock());

    if (!WLSurface) {
        LOG(Log::DEBUG, "Inhibitor has no HL Surface attached to it, likely meaning it's a non-desktop element. Assuming it's visible.");
        PINHIBIT->nonDesktop = true;
        recheckIdleInhibitorStatus();
        return;
    }

    PINHIBIT->surfaceDestroyListener =
        WLSurface->m_events.destroy.listen([this, PINHIBIT] { std::erase_if(m_idleInhibitors, [PINHIBIT](const auto& other) { return other.get() == PINHIBIT; }); });

    recheckIdleInhibitorStatus();
}

void CInputManager::recheckIdleInhibitorStatus() {

    for (auto const& ii : m_idleInhibitors) {
        if (ii->nonDesktop) {
            PROTO::idle->setInhibit(true);
            return;
        }

        auto WLSurface = Desktop::View::CWLSurface::fromResource(ii->inhibitor->m_surface.lock());

        if (!WLSurface || !WLSurface->view())
            continue;

        const auto VIEW          = WLSurface->view();
        const auto ALPHAMODIFIER = dynamicPointerCast<Desktop::View::IAlphaModifiable>(VIEW);
        if (VIEW->mapped() && VIEW->acceptsInput() && (!ALPHAMODIFIER || ALPHAMODIFIER->alphaNonZero())) {
            PROTO::idle->setInhibit(true);
            return;
        }
    }

    // check manual user-set inhibitors
    for (auto const& w : Desktop::windowState()->windows()) {
        if (isWindowInhibiting(w)) {
            PROTO::idle->setInhibit(true);
            return;
        }
    }

    PROTO::idle->setInhibit(false);
}

bool CInputManager::isWindowInhibiting(const PHLWINDOW& w, bool onlyHl) {
    if (w->m_ruleApplicator->idleInhibitMode().valueOrDefault() == Desktop::Rule::IDLEINHIBIT_ALWAYS)
        return true;

    if (w->m_ruleApplicator->idleInhibitMode().valueOrDefault() == Desktop::Rule::IDLEINHIBIT_FOCUS && Desktop::focusState()->isWindowActive(w))
        return true;

    if (w->m_ruleApplicator->idleInhibitMode().valueOrDefault() == Desktop::Rule::IDLEINHIBIT_FULLSCREEN && Fullscreen::controller()->isFullscreen(w) && w->m_workspace &&
        w->m_workspace->visible())
        return true;

    if (onlyHl)
        return false;

    for (auto const& ii : m_idleInhibitors) {
        if (ii->nonDesktop || !ii->inhibitor)
            continue;

        bool isInhibiting = false;
        w->wlSurface()->resource()->breadthfirst(
            [&ii](SP<CWLSurfaceResource> surf, const Vector2D& pos, void* data) {
                if (ii->inhibitor->m_surface != surf)
                    return;

                auto WLSurface = Desktop::View::CWLSurface::fromResource(surf);

                if (!WLSurface || !WLSurface->view())
                    return;

                const auto VIEW          = WLSurface->view();
                const auto ALPHAMODIFIER = dynamicPointerCast<Desktop::View::IAlphaModifiable>(VIEW);
                if (VIEW->mapped() && VIEW->acceptsInput() && (!ALPHAMODIFIER || ALPHAMODIFIER->alphaNonZero()))
                    *sc<bool*>(data) = true;
            },
            &isInhibiting);

        if (isInhibiting)
            return true;
    }

    return false;
}
