#pragma once

#include "../defines.hpp"
#include "../helpers/Timer.hpp"
#include "../helpers/signal/Signal.hpp"
#include <cstdint>
#include <unordered_map>

class CSessionLockSurface;
class CSessionLock;
class CWLSurfaceResource;

struct SSessionLockSurface {
    SSessionLockSurface(SP<CSessionLockSurface> surface_);

    WP<CSessionLockSurface> surface;
    WP<CWLSurfaceResource>  pWlrSurface;
    uint64_t                iMonitorID = -1;

    bool                    mapped = false;

    struct {
        CHyprSignalListener map;
        CHyprSignalListener destroy;
        CHyprSignalListener commit;
    } listeners;
};

struct SSessionLock {
    WP<CSessionLock>                                  lock;

    std::vector<std::unique_ptr<SSessionLockSurface>> vSessionLockSurfaces;
    std::unordered_map<uint64_t, CTimer>              mMonitorsWithoutMappedSurfaceTimers;

    struct {
        CHyprSignalListener newSurface;
        CHyprSignalListener unlock;
        CHyprSignalListener destroy;
    } listeners;
};

class CSessionLockManager {
  public:
    CSessionLockManager();
    ~CSessionLockManager() = default;

    SSessionLockSurface* getSessionLockSurfaceForMonitor(uint64_t);

    float                getRedScreenAlphaForMonitor(uint64_t);

    bool                 isSessionLocked();
    bool                 isSessionLockPresent();
    bool                 isSurfaceSessionLock(SP<CWLSurfaceResource>);

    void                 removeSessionLockSurface(SSessionLockSurface*);

  private:
    UP<SSessionLock> m_pSessionLock;

    struct {
        CHyprSignalListener newLock;
    } listeners;

    void onNewSessionLock(SP<CSessionLock> pWlrLock);
};

inline std::unique_ptr<CSessionLockManager> g_pSessionLockManager;