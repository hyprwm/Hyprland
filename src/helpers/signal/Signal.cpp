#include "Signal.hpp"
#include <algorithm>

void CSignal::emit(std::any data) {
    bool dirty = false;

    for (auto& l : m_vListeners) {
        if (const CHyprSignalListener L = l.lock())
            L->emit(data);
        else
            dirty = true;
    }

    if (dirty)
        std::erase_if(m_vListeners, [](const auto& other) { return !other.lock(); });
}

CHyprSignalListener CSignal::registerListener(std::function<void(std::any)> handler) {
    CHyprSignalListener listener = std::make_shared<CSignalListener>(handler);
    m_vListeners.emplace_back(std::weak_ptr<CSignalListener>(listener));
    return listener;
}
