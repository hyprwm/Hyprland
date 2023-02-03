#pragma once

#include "../defines.hpp"

struct SSessionLockSurface {
    wlr_session_lock_surface_v1* pWlrLockSurface = nullptr;
    int                          iMonitorID      = -1;

    bool                         mapped = false;

    DYNLISTENER(map);
    DYNLISTENER(destroy);
    DYNLISTENER(commit);
};

struct SSessionLock {
    bool                                              active   = false;
    wlr_session_lock_v1*                              pWlrLock = nullptr;

    std::vector<std::unique_ptr<SSessionLockSurface>> vSessionLockSurfaces;

    DYNLISTENER(newSurface);
    DYNLISTENER(unlock);
    DYNLISTENER(destroy);
};

class CSessionLockManager {
  public:
    CSessionLockManager()  = default;
    ~CSessionLockManager() = default;

    void                 onNewSessionLock(wlr_session_lock_v1*);
    SSessionLockSurface* getSessionLockSurfaceForMonitor(const int&);

    bool                 isSessionLocked();
    bool                 isSurfaceSessionLock(wlr_surface*);

    void                 removeSessionLockSurface(SSessionLockSurface*);

    void                 activateLock();

  private:
    SSessionLock m_sSessionLock;
};

inline std::unique_ptr<CSessionLockManager> g_pSessionLockManager;