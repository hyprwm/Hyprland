#include "GlobalViewMethods.hpp"
#include "../../Compositor.hpp"

#include "LayerSurface.hpp"
#include "Window.hpp"
#include "Popup.hpp"
#include "Subsurface.hpp"

#include "../../protocols/core/Compositor.hpp"
#include "../../protocols/core/Subcompositor.hpp"

using namespace Desktop;
using namespace Desktop::View;

std::vector<SP<IView>> View::getViewsForWorkspace(PHLWORKSPACE ws) {
    // std::vector<SP<IView>> views;

    // for (const auto& w : g_pCompositor->m_windows) {
    //     if (!w->visible() || w->m_workspace != ws)
    //         continue;

    //     views.emplace_back(w);

    //     w->m_subsurfaceHead->wlSurface()->resource()->breadthfirst([&views] (SP<CWLSurfaceResource> s, const Vector2D & pos, void * data) {
    //         auto surf = CWLSurface::fromResource(s);
    //         if (!surf)
    //             return;

    //         vievs.emplace_back(surf->view());
    //     });
    // }

    // for (const auto& l : g_pCompositor->m_layers) {
    //     if (!l->visi)
    // }
}
