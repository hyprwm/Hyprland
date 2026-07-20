#include "SurfaceStateQueue.hpp"
#include "../core/Compositor.hpp"
#include "../PresentationTime.hpp"
#include "SurfaceState.hpp"

CSurfaceStateQueue::CSurfaceStateQueue(WP<CWLSurfaceResource> surf) : m_surface(std::move(surf)) {}

void CSurfaceStateQueue::clear() {
    for (const auto& state : m_queue) {
        state->cancelFenceWaiter();
        PROTO::presentation->discardFeedbacks(state->presentationFeedbacks);
    }

    m_queue.clear();
}

WP<SSurfaceState> CSurfaceStateQueue::enqueue(UP<SSurfaceState>&& state) {
    return m_queue.emplace_back(std::move(state));
}

void CSurfaceStateQueue::dropState(const WP<SSurfaceState>& state) {
    const auto& it = find(state);
    if (it == m_queue.end())
        return;

    (*it)->cancelFenceWaiter();
    PROTO::presentation->discardFeedbacks((*it)->presentationFeedbacks);

    m_queue.erase(it);
}

void CSurfaceStateQueue::lock(const WP<SSurfaceState>& weakState, eLockReason reason) {
    ASSERT(reason != LOCK_REASON_NONE);
    const auto& it = find(weakState);
    if (it == m_queue.end())
        return;

    it->get()->lockMask |= reason;
}

void CSurfaceStateQueue::unlock(const WP<SSurfaceState>& state, eLockReason reason) {
    ASSERT(reason != LOCK_REASON_NONE);
    const auto& it = find(state);
    if (it == m_queue.end())
        return;

    it->get()->lockMask &= ~reason;
    tryProcess();
}

void CSurfaceStateQueue::unlockFence(const WP<SSurfaceState>& state) {
    auto it = find(state);
    if (it == m_queue.end())
        return;

    for (const auto& s : m_queue) {
        if (!s->fenceSignaled())
            continue;

        s->lockMask &= ~LOCK_REASON_FENCE;
        s->cancelFenceWaiter();
    }

    tryProcess();
}

void CSurfaceStateQueue::unlockFirst(eLockReason reason) {
    ASSERT(reason != LOCK_REASON_NONE);
    for (const auto& it : m_queue) {
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
    while (!m_queue.empty()) {
        auto& front = m_queue.front();

        // a FIFO barrier only holds the front until the current state's barrier clears
        if (front->lockMask & LOCK_REASON_FIFO && !m_surface->m_current.barrierSet)
            front->lockMask &= ~LOCK_REASON_FIFO;

        if (front->isLocked())
            break;

        auto next = std::next(m_queue.begin());
        if (next != m_queue.end() && !(*next)->isLocked()) {
            front->mergeFrom(**next);
            (*next)->cancelFenceWaiter();
            m_queue.erase(next);
            continue;
        }

        m_surface->commitState(*front);
        m_queue.pop_front();
    }
}
