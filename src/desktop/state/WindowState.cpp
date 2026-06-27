#include "WindowState.hpp"
#include "../../event/EventBus.hpp"
#include "../view/Window.hpp"

#include <algorithm>

using namespace Desktop;

CWindowState::CWindowState() {
    m_listeners.viewCreate = Event::bus()->m_events.view.create.listen([this](PHLVIEW view) {
        const auto WINDOW = View::CWindow::fromView(view);
        if (!WINDOW)
            return;

        m_windows.emplace_back(WINDOW);
    });

    m_listeners.viewDestroy = Event::bus()->m_events.view.destroy.listen([this](const Event::SViewDestroyEvent& event) {
        if (event.type != View::VIEW_TYPE_WINDOW)
            return;

        std::erase_if(m_windows, [&](auto& x) { return !x || rc<uintptr_t>(x.get()) == event.address; });
    });
}

void CWindowState::removeSafe(PHLWINDOW w) {
    std::erase_if(m_windows, [&w](auto& el) { return !el || el == w; });
}

const std::vector<PHLWINDOW>& CWindowState::windows() const {
    return m_windows;
}

void CWindowState::moveToTop(PHLWINDOW w) {
    if (!w || m_windows.empty() || m_windows.back() == w)
        return;

    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        if (*it != w)
            continue;

        std::rotate(it, it + 1, m_windows.end());
        return;
    }
}

void CWindowState::moveToBottom(PHLWINDOW w) {
    if (!w || m_windows.empty() || m_windows.front() == w)
        return;

    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
        if (*it != w)
            continue;

        std::rotate(it, it + 1, m_windows.rend());
        return;
    }
}

void CWindowState::clear() {
    m_windows.clear();
}

UP<CWindowState>& Desktop::windowState() {
    static UP<CWindowState> state = makeUnique<CWindowState>();
    return state;
}
