#include "FadingOutState.hpp"

#include <algorithm>

using namespace Desktop;

const std::vector<PHLWINDOWREF>& CFadingOutState::windows() const {
    return m_windows;
}

const std::vector<PHLLSREF>& CFadingOutState::layers() const {
    return m_layers;
}

void CFadingOutState::add(PHLWINDOW w) {
    const auto FOUND = std::ranges::find_if(m_windows, [&](PHLWINDOWREF& other) { return other.lock() == w; });

    if (FOUND != m_windows.end())
        return;

    m_windows.emplace_back(w);
}

void CFadingOutState::add(PHLLS ls) {
    const auto FOUND = std::ranges::find_if(m_layers, [&](auto& other) { return other.lock() == ls; });

    if (FOUND != m_layers.end())
        return;

    m_layers.emplace_back(ls);
}

void CFadingOutState::remove(PHLWINDOW w) {
    std::erase_if(m_windows, [&w](const auto& el) { return el.lock() == w; });
}

void CFadingOutState::remove(PHLLS ls) {
    std::erase_if(m_layers, [&ls](const auto& el) { return el.lock() == ls; });
}

void CFadingOutState::removeExpiredLayers() {
    std::erase_if(m_layers, [](const auto& el) { return el.expired(); });
}

void CFadingOutState::clear() {
    m_windows.clear();
    m_layers.clear();
}

UP<CFadingOutState>& Desktop::fadingOutState() {
    static UP<CFadingOutState> state = makeUnique<CFadingOutState>();
    return state;
}
