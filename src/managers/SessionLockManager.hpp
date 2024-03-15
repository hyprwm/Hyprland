#pragma once

#include "../defines.hpp"
#include "../helpers/Timer.hpp"
#include <cstdint>
#include <unordered_map>

struct SSessionLockSurface {
    wlr_session_lock_surface_v1* pWlrLockSurface = nullptr;
    uint64_t                     iMonitorID      = -1;

    bool                         mapped = false;

    DYNLISTENER(map);
    DYNLISTENER(destroy);
    DYNLISTENER(commit);
};

struct SSessionLock {
    bool                                              active   = false;
    wlr_session_lock_v1*                              pWlrLock = nullptr;

    std::vector<std::unique_ptr<SSessionLockSurface>> vSessionLockSurfaces;
    std::unordered_map<uint64_t, CTimer>              mMonitorsWithoutMappedSurfaceTimers;

    DYNLISTENER(newSurface);
    DYNLISTENER(unlock);
    DYNLISTENER(destroy);
};

class CSessionLockManager {
  public:
    CSessionLockManager()  = default;
    ~CSessionLockManager() = default;

    void                 onNewSessionLock(wlr_session_lock_v1*);
    SSessionLockSurface* getSessionLockSurfaceForMonitor(uint64_t);

    float                getRedScreenAlphaForMonitor(uint64_t);

    bool                 isSessionLocked();
    bool                 isSurfaceSessionLock(wlr_surface*);

    void                 removeSessionLockSurface(SSessionLockSurface*);

    void                 activateLock();

  private:
    SSessionLock m_sSessionLock;
};

inline std::unique_ptr<CSessionLockManager> g_pSessionLockManager;