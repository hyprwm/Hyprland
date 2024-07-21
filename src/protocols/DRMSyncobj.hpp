#pragma once

#include <memory>
#include <vector>
#include "WaylandProtocol.hpp"
#include "linux-drm-syncobj-v1.hpp"
#include "../helpers/signal/Signal.hpp"

class CWLSurfaceResource;
class CDRMSyncobjTimelineResource;
class CSyncTimeline;

class CDRMSyncobjSurfaceResource {
  public:
    CDRMSyncobjSurfaceResource(SP<CWpLinuxDrmSyncobjSurfaceV1> resource_, SP<CWLSurfaceResource> surface_);

    bool                            good();

    WP<CWLSurfaceResource>          surface;
    WP<CDRMSyncobjTimelineResource> acquireTimeline, releaseTimeline;
    uint64_t                        acquirePoint = 0, releasePoint = 0;

  private:
    SP<CWpLinuxDrmSyncobjSurfaceV1> resource;

    struct {
        CHyprSignalListener surfacePrecommit;
    } listeners;
};

class CDRMSyncobjTimelineResource {
  public:
    CDRMSyncobjTimelineResource(SP<CWpLinuxDrmSyncobjTimelineV1> resource_, int fd_);
    static SP<CDRMSyncobjTimelineResource> fromResource(wl_resource*);

    bool                                   good();

    WP<CDRMSyncobjTimelineResource>        self;
    int                                    fd = -1;
    SP<CSyncTimeline>                      timeline;

  private:
    SP<CWpLinuxDrmSyncobjTimelineV1> resource;
};

class CDRMSyncobjManagerResource {
  public:
    CDRMSyncobjManagerResource(SP<CWpLinuxDrmSyncobjManagerV1> resource_);

    bool good();

  private:
    SP<CWpLinuxDrmSyncobjManagerV1> resource;
};

class CDRMSyncobjProtocol : public IWaylandProtocol {
  public:
    CDRMSyncobjProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyResource(CDRMSyncobjManagerResource* resource);
    void destroyResource(CDRMSyncobjTimelineResource* resource);
    void destroyResource(CDRMSyncobjSurfaceResource* resource);

    //
    std::vector<SP<CDRMSyncobjManagerResource>>  m_vManagers;
    std::vector<SP<CDRMSyncobjTimelineResource>> m_vTimelines;
    std::vector<SP<CDRMSyncobjSurfaceResource>>  m_vSurfaces;

    //
    int drmFD = -1;

    friend class CDRMSyncobjManagerResource;
    friend class CDRMSyncobjTimelineResource;
    friend class CDRMSyncobjSurfaceResource;
};

namespace PROTO {
    inline UP<CDRMSyncobjProtocol> sync;
};
