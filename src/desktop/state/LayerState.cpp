#include "LayerState.hpp"
#include "../../event/EventBus.hpp"
#include "../view/LayerSurface.hpp"

#include <algorithm>

using namespace Desktop;

CLayerState::CLayerState() {
    m_listeners.viewCreate = Event::bus()->m_events.view.create.listen([this](PHLVIEW view) {
        const auto LAYER = View::CLayerSurface::fromView(view);
        if (!LAYER)
            return;

        m_layers.emplace_back(LAYER);
    });

    m_listeners.viewDestroy = Event::bus()->m_events.view.destroy.listen([this](const Event::SViewDestroyEvent& event) {
        if (event.type != View::VIEW_TYPE_LAYER_SURFACE)
            return;

        // A CLayerSurface is always an IView via a static upcast, so compare control-block identity directly.
        // Must NOT dereference x->m_self here: this event is emitted from ~IView, by which point CLayerSurface's
        // members (incl. m_self) are already destroyed. PHLVIEWREF{x} only reads x's own impl_/data pointer.
        std::erase_if(m_layers, [&](auto& x) { return !x || event.view == PHLVIEWREF{x}; });
    });
}

const std::vector<PHLLS>& CLayerState::layers() const {
    return m_layers;
}

void CLayerState::removeSafe(PHLLS ls) {
    std::erase_if(m_layers, [&ls](auto& el) { return !el || el == ls; });
}

void CLayerState::clear() {
    m_layers.clear();
}

UP<CLayerState>& Desktop::layerState() {
    static UP<CLayerState> state = makeUnique<CLayerState>();
    return state;
}
