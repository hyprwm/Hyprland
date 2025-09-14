#pragma once

#include <vector>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "commit-timing-v1.hpp"

#include "../helpers/signal/Signal.hpp"

class CWLSurfaceResource;
class CEventLoopTimer;
class CSurfaceScopeLock;

class CCommitTimerResource {
  public:
    CCommitTimerResource(UP<CWpCommitTimerV1>&& resource_, SP<CWLSurfaceResource> surface);
    ~CCommitTimerResource();

    bool                     good();

    WP<CCommitTimerResource> m_self;

  private:
    UP<CWpCommitTimerV1>   m_resource;

    WP<CWLSurfaceResource> m_surface;

    bool                   m_timerPresent = false;

    struct STimerLock {
        SP<CEventLoopTimer>   timer;
        SP<CSurfaceScopeLock> lock;
    };

    std::vector<SP<STimerLock>> m_timers;

    struct {
        CHyprSignalListener surfacePrecommit;
    } m_listeners;

    friend class CCommitTimingProtocol;
    friend class CCommitTimingManagerResource;
};

class CCommitTimingManagerResource {
  public:
    CCommitTimingManagerResource(UP<CWpCommitTimingManagerV1>&& resource_);
    ~CCommitTimingManagerResource();

    bool good();

  private:
    UP<CWpCommitTimingManagerV1> m_resource;
};

class CCommitTimingProtocol : public IWaylandProtocol {
  public:
    CCommitTimingProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyResource(CCommitTimingManagerResource* resource);
    void destroyResource(CCommitTimerResource* resource);

    //
    std::vector<UP<CCommitTimingManagerResource>> m_managers;
    std::vector<UP<CCommitTimerResource>>         m_timers;

    friend class CCommitTimingManagerResource;
    friend class CCommitTimerResource;
};

namespace PROTO {
    inline UP<CCommitTimingProtocol> commitTiming;
};
