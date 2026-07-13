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

        std::erase_if(m_layers, [&](auto& x) { return !x || rc<uintptr_t>(x.get()) == event.address; });
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
