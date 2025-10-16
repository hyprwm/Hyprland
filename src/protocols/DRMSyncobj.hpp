#pragma once

#include <vector>
#include "WaylandProtocol.hpp"
#include "../helpers/sync/SyncReleaser.hpp"
#include "linux-drm-syncobj-v1.hpp"
#include "../helpers/signal/Signal.hpp"
#include <hyprutils/os/FileDescriptor.hpp>

class CWLSurfaceResource;
class CDRMSyncobjTimelineResource;
class CSyncTimeline;

class CDRMSyncPointState {
  public:
    CDRMSyncPointState() = default;
    CDRMSyncPointState(SP<CSyncTimeline> timeline_, uint64_t point_);
    ~CDRMSyncPointState() = default;

    const uint64_t&                                  point();
    WP<CSyncTimeline>                                timeline();
    Hyprutils::Memory::CUniquePointer<CSyncReleaser> createSyncRelease();
    bool                                             addWaiter(std::function<void()>&& waiter);
    bool                                             committed();
    Hyprutils::OS::CFileDescriptor                   exportAsFD();
    void                                             signal();

    operator bool() const {
        return m_timeline;
    }

  private:
    SP<CSyncTimeline> m_timeline         = {};
    uint64_t          m_point            = 0;
    bool              m_acquireCommitted = false;
    bool              m_releaseTaken     = false;
};

class CDRMSyncobjSurfaceResource {
  public:
    CDRMSyncobjSurfaceResource(UP<CWpLinuxDrmSyncobjSurfaceV1>&& resource_, SP<CWLSurfaceResource> surface_);

    bool good();

  private:
    WP<CWLSurfaceResource>          m_surface;
    UP<CWpLinuxDrmSyncobjSurfaceV1> m_resource;

    CDRMSyncPointState              m_pendingAcquire;
    CDRMSyncPointState              m_pendingRelease;

    struct {
        CHyprSignalListener surfaceStateCommit;
    } m_listeners;
};

class CDRMSyncobjTimelineResource {
  public:
    CDRMSyncobjTimelineResource(UP<CWpLinuxDrmSyncobjTimelineV1>&& resource_, Hyprutils::OS::CFileDescriptor&& fd_);
    ~CDRMSyncobjTimelineResource() = default;
    static WP<CDRMSyncobjTimelineResource> fromResource(wl_resource*);

    bool                                   good();

    Hyprutils::OS::CFileDescriptor         m_fd;
    SP<CSyncTimeline>                      m_timeline;

  private:
    UP<CWpLinuxDrmSyncobjTimelineV1> m_resource;
};

class CDRMSyncobjManagerResource {
  public:
    CDRMSyncobjManagerResource(UP<CWpLinuxDrmSyncobjManagerV1>&& resource_);
    ~CDRMSyncobjManagerResource() = default;

    bool good();

  private:
    UP<CWpLinuxDrmSyncobjManagerV1> m_resource;
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
    std::vector<UP<CDRMSyncobjManagerResource>>  m_managers;
    std::vector<UP<CDRMSyncobjTimelineResource>> m_timelines;
    std::vector<UP<CDRMSyncobjSurfaceResource>>  m_surfaces;

    //
    int m_drmFD = -1;

    friend class CDRMSyncobjManagerResource;
    friend class CDRMSyncobjTimelineResource;
    friend class CDRMSyncobjSurfaceResource;
};

namespace PROTO {
    inline UP<CDRMSyncobjProtocol> sync;
};
