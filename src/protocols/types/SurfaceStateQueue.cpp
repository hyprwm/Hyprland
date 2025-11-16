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

void CSurfaceStateQueue::dropState(const WP<SSurfaceState>& state) {
    auto it = find(state);
    if (it == m_queue.end())
        return;

    m_queue.erase(it);
}

void CSurfaceStateQueue::lock(const WP<SSurfaceState>& weakState, eLockReason reason) {
    auto it = find(weakState);
    if (it == m_queue.end())
        return;

    it->get()->lockMask |= reason;
}

void CSurfaceStateQueue::unlock(const WP<SSurfaceState>& state, eLockReason reason) {
    auto it = find(state);
    if (it == m_queue.end())
        return;

    it->get()->lockMask &= ~reason;
    tryProcess();
}

void CSurfaceStateQueue::unlockFirst(eLockReason reason) {
    for (auto& it : m_queue) {
        if ((it->lockMask & reason) != LOCK_REASON_NONE) {
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
    if (m_queue.empty())
        return;

    auto front = m_queue.begin();
    if (front->get()->lockMask != LOCK_REASON_NONE)
        return;

    auto next = std::next(front);
    if (next == m_queue.end()) {
        m_surface->commitState(**front);
        m_queue.pop_front();
        return;
    }

    while (!m_queue.empty() && next != m_queue.end() && next->get()->lockMask == LOCK_REASON_NONE && !next->get()->updated.bits.buffer) {
        next->get()->updateFrom(**front, true);
        m_queue.pop_front();

        front = m_queue.begin();
        next  = std::next(front);
    }

    m_surface->commitState(**front);
    m_queue.pop_front();
}
