#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "ext-session-lock-v1.hpp"
#include "../helpers/signal/Signal.hpp"

class CMonitor;
class CSessionLock;

class CSessionLockSurface {
  public:
    CSessionLockSurface(SP<CExtSessionLockSurfaceV1> resource_, wlr_surface* surface_, CMonitor* pMonitor_, WP<CSessionLock> owner_);
    ~CSessionLockSurface();

    bool         good();
    bool         inert();
    CMonitor*    monitor();
    wlr_surface* surface();

    struct {
        CSignal map;
        CSignal destroy;
        CSignal commit;
    } events;

  private:
    SP<CExtSessionLockSurfaceV1> resource;
    WP<CSessionLock>             sessionLock;
    wlr_surface*                 pSurface = nullptr;
    CMonitor*                    pMonitor = nullptr;

    bool                         ackdConfigure = false;
    bool                         committed     = false;

    void                         sendConfigure();

    DYNLISTENER(surfaceCommit);
    DYNLISTENER(surfaceDestroy);

    struct {
        CHyprSignalListener monitorMode;
    } listeners;
};

class CSessionLock {
  public:
    CSessionLock(SP<CExtSessionLockV1> resource_);
    ~CSessionLock();

    bool good();
    void sendLocked();

    struct {
        CSignal newLockSurface; // SP<CSessionLockSurface>
        CSignal unlockAndDestroy;
        CSignal destroyed; // fires regardless of whether there was a unlockAndDestroy or not.
    } events;

  private:
    SP<CExtSessionLockV1> resource;

    bool                  inert = false;

    friend class CSessionLockProtocol;
};

class CSessionLockProtocol : public IWaylandProtocol {
  public:
    CSessionLockProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    bool         isLocked();

    struct {
        CSignal newLock; // SP<CSessionLock>
    } events;

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyResource(CSessionLock* lock);
    void destroyResource(CSessionLockSurface* surf);
    void onLock(CExtSessionLockManagerV1* pMgr, uint32_t id);
    void onGetLockSurface(CExtSessionLockV1* lock, uint32_t id, wl_resource* surface, wl_resource* output);

    bool locked = false;

    //
    std::vector<UP<CExtSessionLockManagerV1>> m_vManagers;
    std::vector<SP<CSessionLock>>             m_vLocks;
    std::vector<SP<CSessionLockSurface>>      m_vLockSurfaces;

    friend class CSessionLock;
    friend class CSessionLockSurface;
};

namespace PROTO {
    inline UP<CSessionLockProtocol> sessionLock;
};
