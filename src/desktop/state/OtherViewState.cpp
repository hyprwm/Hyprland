#include "OtherViewState.hpp"
#include "../../event/EventBus.hpp"
#include "../view/View.hpp"

#include <algorithm>

using namespace Desktop;

COtherViewState::COtherViewState() {
    m_listeners.viewCreate = Event::bus()->m_events.view.create.listen([this](PHLVIEW view) {
        if (!view || view->type() != View::VIEW_TYPE_LOCK_SCREEN)
            return;

        m_views.emplace_back(view);
    });

    m_listeners.viewDestroy = Event::bus()->m_events.view.destroy.listen([this](const Event::SViewDestroyEvent& event) {
        if (event.type != View::VIEW_TYPE_LOCK_SCREEN)
            return;

        std::erase_if(m_views, [&](const auto& view) {
            const auto VIEW = view.lock();
            return !VIEW || rc<uintptr_t>(VIEW.get()) == event.address;
        });
    });
}

const std::vector<WP<View::IView>>& COtherViewState::views() const {
    return m_views;
}

void COtherViewState::clear() {
    m_views.clear();
}

UP<COtherViewState>& Desktop::otherViewState() {
    static UP<COtherViewState> state = makeUnique<COtherViewState>();
    return state;
}
