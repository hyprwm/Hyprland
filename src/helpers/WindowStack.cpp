#include "WindowStack.hpp"
#include "desktop/Window.hpp"
#include "managers/HookSystemManager.hpp"
#include "render/Renderer.hpp"

void CWindowStack::registerListeners() {
    m_events.windowRendered = g_pHyprRenderer->signal.windowRendered.registerListener([this](std::any d) {
        auto win = std::any_cast<PHLWINDOW>(d);
        markWindowRendered(win);
    });
    m_events.endOfRendering = g_pHyprRenderer->signal.endRendering.registerListener([this](std::any d) { resetRenderWindowStates(); });
}

void CWindowStack::add(PHLWINDOW window) {
    m_windows.emplace_back(window);
    m_renderWindows.emplace_back(SRenderWindow{.rendered = false, .window = window});
}

void CWindowStack::addFadingOut(PHLWINDOW window) {
    m_windowsFadingOut.emplace_back(window);
}

void CWindowStack::clear() {
    m_windows.clear();
    m_windowsFadingOut.clear();
}

void CWindowStack::moveToZ(PHLWINDOW w, bool top) {
    if (top) {
        for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
            if (*it == w) {
                std::rotate(it, it + 1, m_windows.end());
                break;
            }
        }
    } else {
        for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
            if (*it == w) {
                std::rotate(it, it + 1, m_windows.rend());
                break;
            }
        }
    }
}

void CWindowStack::removeSafe(PHLWINDOW window) {
    if (!window->m_fadingOut) {
        EMIT_HOOK_EVENT("destroyWindow", window);

        std::erase_if(m_windows, [&](SP<CWindow>& el) { return el == window; });
        std::erase_if(m_windowsFadingOut, [&](PHLWINDOWREF el) { return el.lock() == window; });
        std::erase_if(m_renderWindows, [&](SRenderWindow& el) { return el.window == window; });
    }
}

const std::vector<PHLWINDOW>& CWindowStack::windows() {
    return m_windows;
}

const std::vector<PHLWINDOWREF>& CWindowStack::windowsFadingOut() {
    return m_windowsFadingOut;
}

const std::vector<CWindowStack::SRenderWindow>& CWindowStack::renderWindows() {
    return m_renderWindows;
}

void CWindowStack::markWindowRendered(PHLWINDOW window) {
    for (auto& w : m_renderWindows) {
        if (w.window != window)
            continue;

        w.rendered = true;
    }
}

void CWindowStack::resetRenderWindowStates() {
    for (auto& w : m_renderWindows)
        w.rendered = false;
}
