#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "SurfaceState.hpp"
#include <deque>

class CWLSurfaceResource;

class CSurfaceStateQueue {
  public:
    CSurfaceStateQueue() = default;
    explicit CSurfaceStateQueue(WP<CWLSurfaceResource> surf);

    void              clear();
    WP<SSurfaceState> enqueue(UP<SSurfaceState>&& state);
    void              dropState(const WP<SSurfaceState>& state);
    void              lock(const WP<SSurfaceState>& state, eLockReason reason);
    void              unlock(const WP<SSurfaceState>& state, eLockReason reason = LOCK_REASON_NONE);
    void              unlockFirst(eLockReason reason);
    void              tryProcess();

  private:
    std::deque<UP<SSurfaceState>>                    m_queue;
    WP<CWLSurfaceResource>                           m_surface;

    typename std::deque<UP<SSurfaceState>>::iterator find(const WP<SSurfaceState>& state);
};
