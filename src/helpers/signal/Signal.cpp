#include "Signal.hpp"
#include <algorithm>

void CSignal::emit(std::any data) {
    bool                             dirty = false;

    std::vector<SP<CSignalListener>> listeners;
    for (auto& l : m_vListeners) {
        if (l.expired()) {
            dirty = true;
            continue;
        }

        listeners.emplace_back(l.lock());
    }

    std::vector<CStaticSignalListener*> statics;
    for (auto& l : m_vStaticListeners) {
        statics.emplace_back(l.get());
    }

    for (auto& l : listeners) {
        // if there is only one lock, it means the event is only held by the listeners
        // vector and was removed during our iteration
        if (l.strongRef() == 1) {
            dirty = true;
            continue;
        }
        l->emit(data);
    }

    for (auto& l : statics) {
        l->emit(data);
    }

    // release SPs
    listeners.clear();

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