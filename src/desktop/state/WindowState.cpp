#include "WindowState.hpp"
#include "../../event/EventBus.hpp"
#include "../../render/Renderer.hpp"
#include "../view/Window.hpp"

#include <algorithm>
#include <ranges>

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

CWindowQuery CWindowState::query() const {
    return CWindowQuery{*this};
}

void CWindowState::raise(PHLWINDOW w) {
    moveToZ(w, true);
}

void CWindowState::lower(PHLWINDOW w) {
    moveToZ(w, false);
}

void CWindowState::moveToZ(PHLWINDOW w, bool top) {
    if (!View::validMapped(w) || m_windows.empty())
        return;

    w->m_createdOverFullscreen = top;
    w->updateFullscreenInputState();
    *w->alpha(View::WINDOW_ALPHA_FULLSCREEN) = w->isBlockedByFullscreen() ? 0.F : 1.F;

    if (w == (top ? m_windows.back() : m_windows.front()))
        return;

    auto moveSingleToZ = [&](PHLWINDOW pw) -> void {
        if (top)
            moveToTop(pw);
        else
            moveToBottom(pw);

        if (pw->m_isMapped)
            g_pHyprRenderer->damageMonitor(pw->m_monitor.lock());
    };

    if (!w->m_isX11) {
        moveSingleToZ(w);
        return;
    }

    // move X11 transient stack
    std::vector<PHLWINDOW> toMove;

    auto                   collectX11Stack = [&](PHLWINDOW pw, auto&& collectX11Stack) -> void {
        if (top)
            toMove.emplace_back(pw);
        else
            toMove.insert(toMove.begin(), pw);

        for (auto const& other : m_windows) {
            if (other->m_isMapped && !other->isHidden() && other->m_isX11 && other->x11Parent() == pw && other != pw && std::ranges::find(toMove, other) == toMove.end())
                collectX11Stack(other, collectX11Stack);
        }
    };

    collectX11Stack(w, collectX11Stack);
    for (auto const& it : toMove) {
        moveSingleToZ(it);
    }
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
