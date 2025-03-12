#pragma once

#include <vector>
#include "WaylandProtocol.hpp"
#include "helpers/sync/SyncReleaser.hpp"
#include "linux-drm-syncobj-v1.hpp"
#include "../helpers/signal/Signal.hpp"
#include "types/SurfaceState.hpp"
#include <hyprutils/os/FileDescriptor.hpp>
#include <list>

class CWLSurfaceResource;
class CDRMSyncobjTimelineResource;
class CSyncTimeline;
struct SSurfaceState;

class CDRMSyncPointState {
  public:
    CDRMSyncPointState() = default;
    CDRMSyncPointState(WP<CDRMSyncobjTimelineResource> resource_, uint64_t point_, bool acquirePoint);
    ~CDRMSyncPointState() = default;

    const uint64_t&                                  point();
    WP<CDRMSyncobjTimelineResource>                  resource();
    WP<CSyncTimeline>                                timeline();
    bool                                             expired();
    Hyprutils::Memory::CUniquePointer<CSyncReleaser> createSyncRelease();
    bool                                             addWaiter(const std::function<void()>& waiter);
    bool                                             comitted();
    Hyprutils::OS::CFileDescriptor                   exportAsFD();
    void                                             signal();

  private:
    WP<CDRMSyncobjTimelineResource> m_resource         = {};
    uint64_t                        m_point            = 0;
    WP<CSyncTimeline>               m_timeline         = {};
    bool                            m_acquirePoint     = false;
    bool                            m_acquireCommitted = false;
    bool                            m_releaseTaken     = false;
};

class CDRMSyncobjSurfaceResource {
  public:
    CDRMSyncobjSurfaceResource(UP<CWpLinuxDrmSyncobjSurfaceV1>&& resource_, SP<CWLSurfaceResource> surface_);
    ~CDRMSyncobjSurfaceResource();

    bool protocolError();
    bool good();

  private:
    void                            removeAllWaiters();
    WP<CWLSurfaceResource>          surface;
    UP<CWpLinuxDrmSyncobjSurfaceV1> resource;

    CDRMSyncPointState              pendingAcquire;
    CDRMSyncPointState              pendingRelease;
    std::vector<SP<SSurfaceState>>  pendingStates;

    struct {
        CHyprSignalListener surfacePrecommit;
    } listeners;
};

class CDRMSyncobjTimelineResource {
  public:
    CDRMSyncobjTimelineResource(UP<CWpLinuxDrmSyncobjTimelineV1>&& resource_, Hyprutils::OS::CFileDescriptor&& fd_);
    ~CDRMSyncobjTimelineResource() = default;
    static WP<CDRMSyncobjTimelineResource> fromResource(wl_resource*);

    bool                                   good();

    Hyprutils::OS::CFileDescriptor         fd;
    SP<CSyncTimeline>                      timeline;

  private:
    UP<CWpLinuxDrmSyncobjTimelineV1> resource;
};

class CDRMSyncobjManagerResource {
  public:
    CDRMSyncobjManagerResource(UP<CWpLinuxDrmSyncobjManagerV1>&& resource_);
    ~CDRMSyncobjManagerResource() = default;

    bool good();

  private:
    UP<CWpLinuxDrmSyncobjManagerV1> resource;
};

class CDRMSyncobjProtocol : public IWaylandProtocol {
  public:
    CDRMSyncobjProtocol(const wl_interface* iface, const int& ver, const std::string& name);
    ~CDRMSyncobjProtocol() = default;

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyResource(CDRMSyncobjManagerResource* resource);
    void destroyResource(CDRMSyncobjTimelineResource* resource);
    void destroyResource(CDRMSyncobjSurfaceResource* resource);

    //
    std::vector<UP<CDRMSyncobjManagerResource>>  m_vManagers;
    std::vector<UP<CDRMSyncobjTimelineResource>> m_vTimelines;
    std::vector<UP<CDRMSyncobjSurfaceResource>>  m_vSurfaces;

    //
    int drmFD = -1;

    friend class CDRMSyncobjManagerResource;
    friend class CDRMSyncobjTimelineResource;
    friend class CDRMSyncobjSurfaceResource;
};

namespace PROTO {
    inline UP<CDRMSyncobjProtocol> sync;
};
