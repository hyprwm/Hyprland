#include "GlobalViewMethods.hpp"
#include "../../Compositor.hpp"

#include "LayerSurface.hpp"
#include "window/Window.hpp"
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

    if (!ws)
        return views;

    for (const auto& w : Desktop::windowState()->windows()) {
        if (!w->mapped() || !w->acceptsInput() || !w->alphaNonZero() || !w->resource() || w->m_workspace != ws)
            continue;

        views.emplace_back(w);

        w->wlSurface()->resource()->breadthfirst(
            [&views](SP<CWLSurfaceResource> s, const Vector2D& pos, void* data) {
                auto surf = CWLSurface::fromResource(s);
                if (!surf || !s->m_mapped)
                    return;

                const auto view = surf->view();
                if (!view || !view->mapped() || !view->acceptsInput())
                    return;

                const auto alphaModifier = dynamicPointerCast<IAlphaModifiable>(view);
                if (alphaModifier && !alphaModifier->alphaNonZero())
                    return;

                views.emplace_back(view);
            },
            nullptr);

        // xwl windows dont have this
        if (w->popupHead()) {
            w->popupHead()->breadthfirst(
                [&views](SP<CPopup> s, void* data) {
                    auto surf = s->wlSurface();
                    if (!surf || !surf->resource() || !s->mapped() || !s->acceptsInput() || !s->alphaNonZero())
                        return;

                    views.emplace_back(surf->view());
                },
                nullptr);
        }
    }

    for (const auto& l : Desktop::layerState()->layers()) {
        if (!l->mapped() || !l->acceptsInput() || !l->alphaNonZero() || l->m_monitor != ws->m_monitor)
            continue;

        views.emplace_back(l);

        l->popupHead()->breadthfirst(
            [&views](SP<CPopup> p, void* data) {
                auto surf = p->wlSurface();
                if (!surf || !surf->resource() || !p->mapped() || !p->acceptsInput() || !p->alphaNonZero())
                    return;

                views.emplace_back(surf->view());
            },
            nullptr);
    }

    for (const auto& vr : Desktop::otherViewState()->views()) {
        const auto v = vr.lock();

        if (!v)
            continue;

        if (!v->mapped() || !v->acceptsInput() || !v->resource() || !v->desktopComponent())
            continue;

        const auto alphaModifier = dynamicPointerCast<IAlphaModifiable>(v);
        if (alphaModifier && !alphaModifier->alphaNonZero())
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
