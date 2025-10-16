#include "SurfaceStateQueue.hpp"
#include "../core/Compositor.hpp"
#include "SurfaceState.hpp"

CSurfaceStateQueue::CSurfaceStateQueue(WP<CWLSurfaceResource> surf) : m_surface(std::move(surf)) {}

void CSurfaceStateQueue::clear() {
    m_queue.clear();
}

WP<SSurfaceState> CSurfaceStateQueue::enqueue(UP<SSurfaceState>&& state) {
    return m_queue.emplace_back(std::move(state));
}

void CSurfaceStateQueue::lock(const WP<SSurfaceState>& weakState, LockReason reason) {
    auto it = find(weakState);
    if (it != m_queue.end())
        it->get()->lockMask |= reason;
}

void CSurfaceStateQueue::unlock(const WP<SSurfaceState>& state, LockReason reason) {
    auto it = find(state);
    if (it != m_queue.end()) {
        it->get()->lockMask &= ~reason;
        tryProcess();
    }
}

void CSurfaceStateQueue::unlockFirst(LockReason reason) {
    for (auto& it : m_queue) {
        if ((it->lockMask & reason) != LockReason::None) {
            it->lockMask &= ~reason;
            break;
        }
    }

    tryProcess();
}

auto CSurfaceStateQueue::find(const WP<SSurfaceState>& state) -> std::deque<UP<SSurfaceState>>::iterator {
    if (state.expired())
        return m_queue.end();

    auto* raw = state.get(); // get raw pointer

    for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
        if (it->get() == raw)
            return it;
    }

    return m_queue.end();
}

void CSurfaceStateQueue::tryProcess() {
    while (!m_queue.empty()) {
        auto& front = m_queue.front();
        if (front->lockMask != LockReason::None)
            return;

        m_surface->commitState(*front);
        m_queue.pop_front();
    }
}
