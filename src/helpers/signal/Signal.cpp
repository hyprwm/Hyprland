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

    for (auto& l : m_vStaticListeners) {
        l->emit(data);
    }

    if (dirty)
        std::erase_if(m_vListeners, [](const auto& other) { return other.expired(); });
}

CHyprSignalListener CSignal::registerListener(std::function<void(std::any)> handler) {
    CHyprSignalListener listener = makeShared<CSignalListener>(handler);
    m_vListeners.emplace_back(WP<CSignalListener>(listener));
    return listener;
}

void CSignal::registerStaticListener(std::function<void(void*, std::any)> handler, void* owner) {
    m_vStaticListeners.emplace_back(std::make_unique<CStaticSignalListener>(handler, owner));
}