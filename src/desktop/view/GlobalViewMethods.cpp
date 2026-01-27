#include "GlobalViewMethods.hpp"
#include "../../Compositor.hpp"

#include "LayerSurface.hpp"
#include "Window.hpp"
#include "Popup.hpp"
#include "Subsurface.hpp"
#include "SessionLock.hpp"

#include "../../protocols/core/Compositor.hpp"
#include "../../protocols/core/Subcompositor.hpp"
#include "../../protocols/SessionLock.hpp"

using namespace Desktop;
using namespace Desktop::View;

std::vector<SP<IView>> View::getViewsForWorkspace(PHLWORKSPACE ws) {
    std::vector<SP<IView>> views;

    for (const auto& w : g_pCompositor->m_windows) {
        if (!w->aliveAndVisible() || w->m_workspace != ws)
            continue;

        views.emplace_back(w);

        w->wlSurface()->resource()->breadthfirst(
            [&views](SP<CWLSurfaceResource> s, const Vector2D& pos, void* data) {
                auto surf = CWLSurface::fromResource(s);
                if (!surf || !s->m_mapped)
                    return;

                views.emplace_back(surf->view());
            },
            nullptr);

        // xwl windows dont have this
        if (w->m_popupHead) {
            w->m_popupHead->breadthfirst(
                [&views](SP<CPopup> s, void* data) {
                    auto surf = s->wlSurface();
                    if (!surf || !s->aliveAndVisible())
                        return;

                    views.emplace_back(surf->view());
                },
                nullptr);
        }
    }

    for (const auto& l : g_pCompositor->m_layers) {
        if (!l->aliveAndVisible() || l->m_monitor != ws->m_monitor)
            continue;

        views.emplace_back(l);

        l->m_popupHead->breadthfirst(
            [&views](SP<CPopup> p, void* data) {
                auto surf = p->wlSurface();
                if (!surf || !p->aliveAndVisible())
                    return;

                views.emplace_back(surf->view());
            },
            nullptr);
    }

    for (const auto& v : g_pCompositor->m_otherViews) {
        if (!v->aliveAndVisible() || !v->desktopComponent())
            continue;

        if (v->type() == VIEW_TYPE_LOCK_SCREEN) {
            const auto LOCK = Desktop::View::CSessionLock::fromView(v);
            if (LOCK->monitor() != ws->m_monitor)
                continue;

            views.emplace_back(LOCK);
            continue;
        }
    }

    return views;
}
