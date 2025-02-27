#pragma once

#include <vector>
#include "WaylandProtocol.hpp"
#include "linux-drm-syncobj-v1.hpp"
#include "../helpers/signal/Signal.hpp"
#include <hyprutils/os/FileDescriptor.hpp>

class CWLSurfaceResource;
class CDRMSyncobjTimelineResource;
class CSyncTimeline;

class CDRMSyncobjSurfaceResource {
  public:
    CDRMSyncobjSurfaceResource(UP<CWpLinuxDrmSyncobjSurfaceV1>&& resource_, SP<CWLSurfaceResource> surface_);
    ~CDRMSyncobjSurfaceResource() = default;

    bool                   good();

    WP<CWLSurfaceResource> surface;
    struct {
        WP<CDRMSyncobjTimelineResource> acquireTimeline, releaseTimeline;
        uint64_t                        acquirePoint = 0, releasePoint = 0;
    } current, pending;

  private:
    UP<CWpLinuxDrmSyncobjSurfaceV1> resource;

    struct {
        CHyprSignalListener surfacePrecommit;
        CHyprSignalListener surfaceCommit;
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
