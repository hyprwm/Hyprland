#include "DRMSyncobj.hpp"
#include <algorithm>

#include "core/Compositor.hpp"
#include "../helpers/sync/SyncTimeline.hpp"
#include "../Compositor.hpp"

#include <fcntl.h>

CDRMSyncobjSurfaceResource::CDRMSyncobjSurfaceResource(SP<CWpLinuxDrmSyncobjSurfaceV1> resource_, SP<CWLSurfaceResource> surface_) : surface(surface_), resource(resource_) {
    if (!good())
        return;

    resource->setData(this);

    resource->setOnDestroy([this](CWpLinuxDrmSyncobjSurfaceV1* r) { PROTO::sync->destroyResource(this); });
    resource->setDestroy([this](CWpLinuxDrmSyncobjSurfaceV1* r) { PROTO::sync->destroyResource(this); });

    resource->setSetAcquirePoint([this](CWpLinuxDrmSyncobjSurfaceV1* r, wl_resource* timeline_, uint32_t hi, uint32_t lo) {
        if (!surface) {
            resource->error(WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_SURFACE, "Surface is gone");
            return;
        }

        auto timeline           = CDRMSyncobjTimelineResource::fromResource(timeline_);
        pending.acquireTimeline = timeline;
        pending.acquirePoint    = ((uint64_t)hi << 32) | (uint64_t)lo;
    });

    resource->setSetReleasePoint([this](CWpLinuxDrmSyncobjSurfaceV1* r, wl_resource* timeline_, uint32_t hi, uint32_t lo) {
        if (!surface) {
            resource->error(WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_SURFACE, "Surface is gone");
            return;
        }

        auto timeline           = CDRMSyncobjTimelineResource::fromResource(timeline_);
        pending.releaseTimeline = timeline;
        pending.releasePoint    = ((uint64_t)hi << 32) | (uint64_t)lo;
    });

    listeners.surfacePrecommit = surface->events.precommit.registerListener([this](std::any d) {
        if ((pending.acquireTimeline || pending.releaseTimeline) && !surface->pending.texture) {
            resource->error(WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_BUFFER, "Missing buffer");
            surface->pending.rejected = true;
            return;
        }

        if (!surface->pending.newBuffer)
            return; // this commit does not change the state here

        if (!!pending.acquireTimeline != !!pending.releaseTimeline) {
            resource->error(pending.acquireTimeline ? WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_RELEASE_POINT : WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_ACQUIRE_POINT,
                            "Missing timeline");
            surface->pending.rejected = true;
            return;
        }

        if (!pending.acquireTimeline)
            return;

        // wait for the acquire timeline to materialize
        auto materialized = pending.acquireTimeline->timeline->check(pending.acquirePoint, DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE);
        if (!materialized.has_value()) {
            LOGM(ERR, "Failed to check the acquire timeline");
            resource->noMemory();
            return;
        }

        if (materialized)
            return;

        surface->lockPendingState();
        pending.acquireTimeline->timeline->addWaiter([this]() { surface->unlockPendingState(); }, pending.acquirePoint, DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE);
    });

    listeners.surfaceCommit = surface->events.roleCommit.registerListener([this](std::any d) {
        // apply timelines if new ones have been attached, otherwise don't touch
        // the current ones
        if (pending.releaseTimeline) {
            current.releaseTimeline = pending.releaseTimeline;
            current.releasePoint    = pending.releasePoint;
        }

        if (pending.acquireTimeline) {
            current.acquireTimeline = pending.acquireTimeline;
            current.acquirePoint    = pending.acquirePoint;
        }

        pending.releaseTimeline.reset();
        pending.acquireTimeline.reset();
    });
}

bool CDRMSyncobjSurfaceResource::good() {
    return resource->resource();
}

CDRMSyncobjTimelineResource::CDRMSyncobjTimelineResource(SP<CWpLinuxDrmSyncobjTimelineV1> resource_, int fd_) : fd(fd_), resource(resource_) {
    if (!good())
        return;

    resource->setData(this);

    resource->setOnDestroy([this](CWpLinuxDrmSyncobjTimelineV1* r) { PROTO::sync->destroyResource(this); });
    resource->setDestroy([this](CWpLinuxDrmSyncobjTimelineV1* r) { PROTO::sync->destroyResource(this); });

    timeline = CSyncTimeline::create(PROTO::sync->drmFD, fd);

    if (!timeline) {
        resource->error(WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_INVALID_TIMELINE, "Timeline failed importing");
        return;
    }
}

CDRMSyncobjTimelineResource::~CDRMSyncobjTimelineResource() {
    if (fd >= 0)
        close(fd);
}

SP<CDRMSyncobjTimelineResource> CDRMSyncobjTimelineResource::fromResource(wl_resource* res) {
    auto data = (CDRMSyncobjTimelineResource*)(((CWpLinuxDrmSyncobjTimelineV1*)wl_resource_get_user_data(res))->data());
    return data ? data->self.lock() : nullptr;
}

bool CDRMSyncobjTimelineResource::good() {
    return resource->resource();
}

CDRMSyncobjManagerResource::CDRMSyncobjManagerResource(SP<CWpLinuxDrmSyncobjManagerV1> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setOnDestroy([this](CWpLinuxDrmSyncobjManagerV1* r) { PROTO::sync->destroyResource(this); });
    resource->setDestroy([this](CWpLinuxDrmSyncobjManagerV1* r) { PROTO::sync->destroyResource(this); });

    resource->setGetSurface([this](CWpLinuxDrmSyncobjManagerV1* r, uint32_t id, wl_resource* surf) {
        if (!surf) {
            resource->error(-1, "Invalid surface");
            return;
        }

        auto SURF = CWLSurfaceResource::fromResource(surf);
        if (!SURF) {
            resource->error(-1, "Invalid surface (2)");
            return;
        }

        if (SURF->syncobj) {
            resource->error(WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_SURFACE_EXISTS, "Surface already has a syncobj attached");
            return;
        }

        auto RESOURCE = makeShared<CDRMSyncobjSurfaceResource>(makeShared<CWpLinuxDrmSyncobjSurfaceV1>(resource->client(), resource->version(), id), SURF);
        if (!RESOURCE->good()) {
            resource->noMemory();
            return;
        }

        PROTO::sync->m_vSurfaces.emplace_back(RESOURCE);
        SURF->syncobj = RESOURCE;

        LOGM(LOG, "New linux_syncobj at {:x} for surface {:x}", (uintptr_t)RESOURCE.get(), (uintptr_t)SURF.get());
    });

    resource->setImportTimeline([this](CWpLinuxDrmSyncobjManagerV1* r, uint32_t id, int32_t fd) {
        auto RESOURCE = makeShared<CDRMSyncobjTimelineResource>(makeShared<CWpLinuxDrmSyncobjTimelineV1>(resource->client(), resource->version(), id), fd);
        if (!RESOURCE->good()) {
            resource->noMemory();
            return;
        }

        PROTO::sync->m_vTimelines.emplace_back(RESOURCE);
        RESOURCE->self = RESOURCE;

        LOGM(LOG, "New linux_drm_timeline at {:x}", (uintptr_t)RESOURCE.get());
    });
}

bool CDRMSyncobjManagerResource::good() {
    return resource->resource();
}

CDRMSyncobjProtocol::CDRMSyncobjProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    drmFD = g_pCompositor->m_iDRMFD;
}

void CDRMSyncobjProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CDRMSyncobjManagerResource>(makeShared<CWpLinuxDrmSyncobjManagerV1>(client, ver, id)));

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }
}

void CDRMSyncobjProtocol::destroyResource(CDRMSyncobjManagerResource* resource) {
    std::erase_if(m_vManagers, [resource](const auto& e) { return e.get() == resource; });
}

void CDRMSyncobjProtocol::destroyResource(CDRMSyncobjTimelineResource* resource) {
    std::erase_if(m_vTimelines, [resource](const auto& e) { return e.get() == resource; });
}

void CDRMSyncobjProtocol::destroyResource(CDRMSyncobjSurfaceResource* resource) {
    std::erase_if(m_vSurfaces, [resource](const auto& e) { return e.get() == resource; });
}
