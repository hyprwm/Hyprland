#include "DRMSyncobj.hpp"
#include <algorithm>

#include "core/Compositor.hpp"
#include "../helpers/sync/SyncTimeline.hpp"
#include "../Compositor.hpp"

#include <fcntl.h>
using namespace Hyprutils::OS;

CDRMSyncPointState::CDRMSyncPointState(WP<CDRMSyncobjTimelineResource> resource_, uint64_t point_, bool acquirePoint) :
    m_resource(resource_), m_point(point_), m_acquirePoint(acquirePoint) {}

const uint64_t& CDRMSyncPointState::point() {
    return m_point;
}

WP<CDRMSyncobjTimelineResource> CDRMSyncPointState::resource() {
    return m_resource;
}

WP<CSyncTimeline> CDRMSyncPointState::timeline() {
    if (expired()) {
        Debug::log(ERR, "CDRMSyncPointState: getting a timeline on a expired point");
        return {};
    }

    return m_resource->timeline;
}

bool CDRMSyncPointState::expired() {
    return !m_resource || !m_resource->timeline;
}

UP<CSyncReleaser> CDRMSyncPointState::createSyncRelease() {
    if (expired()) {
        Debug::log(ERR, "CDRMSyncPointState: creating a sync releaser on an expired point");
        return nullptr;
    }

    if (m_releaseTaken) {
        Debug::log(ERR, "CDRMSyncPointState: creating a sync releaser on an already created SyncRelease");
    }

    m_releaseTaken = true;
    return makeUnique<CSyncReleaser>(m_resource->timeline, m_point);
}

bool CDRMSyncPointState::addWaiter(const std::function<void()>& waiter) {
    if (expired()) {
        Debug::log(ERR, "CDRMSyncPointState: adding a waiter on an expired point");
        return false;
    }

    m_acquireWaited = true;
    return m_resource->timeline->addWaiter(waiter, m_point, 0u);
}

bool CDRMSyncPointState::waited() {
    return m_acquireWaited;
}

CFileDescriptor CDRMSyncPointState::exportAsFD() {
    if (expired()) {
        Debug::log(ERR, "CDRMSyncPointState: exporting a FD on an expired point");
        return {};
    }

    return m_resource->timeline->exportAsSyncFileFD(m_point);
}

void CDRMSyncPointState::signal() {
    if (expired()) {
        Debug::log(ERR, "CDRMSyncPointState: signaling on an expired point");
        return;
    }

    m_resource->timeline->signal(m_point);
}

CDRMSyncobjSurfaceResource::CDRMSyncobjSurfaceResource(UP<CWpLinuxDrmSyncobjSurfaceV1>&& resource_, SP<CWLSurfaceResource> surface_) :
    surface(surface_), resource(std::move(resource_)) {
    if UNLIKELY (!good())
        return;

    resource->setData(this);

    resource->setOnDestroy([this](CWpLinuxDrmSyncobjSurfaceV1* r) { PROTO::sync->destroyResource(this); });
    resource->setDestroy([this](CWpLinuxDrmSyncobjSurfaceV1* r) { PROTO::sync->destroyResource(this); });

    resource->setSetAcquirePoint([this](CWpLinuxDrmSyncobjSurfaceV1* r, wl_resource* timeline_, uint32_t hi, uint32_t lo) {
        if (!surface) {
            resource->error(WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_SURFACE, "Surface is gone");
            return;
        }

        auto timeline   = CDRMSyncobjTimelineResource::fromResource(timeline_);
        pending.acquire = {timeline, ((uint64_t)hi << 32) | (uint64_t)lo, true};
    });

    resource->setSetReleasePoint([this](CWpLinuxDrmSyncobjSurfaceV1* r, wl_resource* timeline_, uint32_t hi, uint32_t lo) {
        if (!surface) {
            resource->error(WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_SURFACE, "Surface is gone");
            return;
        }

        auto timeline   = CDRMSyncobjTimelineResource::fromResource(timeline_);
        pending.release = {timeline, ((uint64_t)hi << 32) | (uint64_t)lo, false};
    });

    listeners.surfaceBufferAttach = surface->events.bufferAttach.registerListener([this](std::any d) {
        if (!surface->pending.buffer)
            return; // null buffer, dont add points.

        if (!pending.acquire.expired()) {
            surface->pending.buffer->acquire = makeUnique<CDRMSyncPointState>(pending.acquire);
            pending.acquire                  = {};
        }

        if (!pending.release.expired()) {
            surface->pending.buffer->release      = makeUnique<CDRMSyncPointState>(pending.release);
            surface->pending.buffer->syncReleaser = pending.release.createSyncRelease();
            pending.release                       = {};
        }
    });

    listeners.surfacePrecommit = surface->events.precommit.registerListener([this](std::any d) {
        if (!surface->pending.newBuffer || !surface->pending.buffer) {
            return; // dont wait on null buffer
        }

        if (protocolError())
            return;

        if (surface->pending.buffer->acquire->waited()) {
            pendingCommits++;
            return;
        }

        if (surface->pending.buffer->acquire->addWaiter([this] {
                if (!surface)
                    return;

                surface->unlockPendingState();
                for (auto i = 0u; i <= this->pendingCommits; i++)
                    surface->commitPendingState();

                this->pendingCommits = 0;
            })) {
            surface->lockPendingState();
        }
    });

    listeners.surfaceCommit = surface->events.roleCommit.registerListener([this](std::any d) {
        // apply timelines if new ones have been attached, otherwise don't touch
        // the current ones
        /*if (pending.releaseTimeline) {
            current.releaseTimeline = pending.releaseTimeline;
            current.releasePoint    = pending.releasePoint;
        }

        if (pending.acquireTimeline) {
            current.acquireTimeline = pending.acquireTimeline;
            current.acquirePoint    = pending.acquirePoint;
        }

        pending.releaseTimeline.reset();
        pending.acquireTimeline.reset();*/
    });
}

bool CDRMSyncobjSurfaceResource::protocolError() {
    if (!surface->pending.texture) {
        resource->error(WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_BUFFER, "Missing buffer");
        surface->pending.rejected = true;
        return true;
    }

    if (!!surface->pending.buffer->acquire->expired() != !!surface->pending.buffer->release->expired()) {
        resource->error(surface->pending.buffer->acquire->expired() ? WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_RELEASE_POINT :
                                                                      WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_ACQUIRE_POINT,
                        "Missing timeline");
        surface->pending.rejected = true;
        return true;
    }

    if (surface->pending.buffer->acquire->timeline() == surface->pending.buffer->release->timeline()) {
        if (surface->pending.buffer->acquire->point() >= surface->pending.buffer->release->point()) {
            resource->error(WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_CONFLICTING_POINTS, "Acquire and release points are on the same timeline, and acquire >= release");
            surface->pending.rejected = true;
            return true;
        }
    }

    return false;
}

bool CDRMSyncobjSurfaceResource::good() {
    return resource->resource();
}

CDRMSyncobjTimelineResource::CDRMSyncobjTimelineResource(UP<CWpLinuxDrmSyncobjTimelineV1>&& resource_, CFileDescriptor&& fd_) : fd(std::move(fd_)), resource(std::move(resource_)) {
    if UNLIKELY (!good())
        return;

    resource->setData(this);

    resource->setOnDestroy([this](CWpLinuxDrmSyncobjTimelineV1* r) { PROTO::sync->destroyResource(this); });
    resource->setDestroy([this](CWpLinuxDrmSyncobjTimelineV1* r) { PROTO::sync->destroyResource(this); });

    timeline = CSyncTimeline::create(PROTO::sync->drmFD, fd.get());

    if (!timeline) {
        resource->error(WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_INVALID_TIMELINE, "Timeline failed importing");
        return;
    }
}

WP<CDRMSyncobjTimelineResource> CDRMSyncobjTimelineResource::fromResource(wl_resource* res) {
    for (const auto& r : PROTO::sync->m_vTimelines) {
        if (r && r->resource && r->resource->resource() == res)
            return r;
    }

    return {};
}

bool CDRMSyncobjTimelineResource::good() {
    return resource->resource();
}

CDRMSyncobjManagerResource::CDRMSyncobjManagerResource(UP<CWpLinuxDrmSyncobjManagerV1>&& resource_) : resource(std::move(resource_)) {
    if UNLIKELY (!good())
        return;

    resource->setOnDestroy([this](CWpLinuxDrmSyncobjManagerV1* r) { PROTO::sync->destroyResource(this); });
    resource->setDestroy([this](CWpLinuxDrmSyncobjManagerV1* r) { PROTO::sync->destroyResource(this); });

    resource->setGetSurface([this](CWpLinuxDrmSyncobjManagerV1* r, uint32_t id, wl_resource* surf) {
        if UNLIKELY (!surf) {
            resource->error(-1, "Invalid surface");
            return;
        }

        auto SURF = CWLSurfaceResource::fromResource(surf);
        if UNLIKELY (!SURF) {
            resource->error(-1, "Invalid surface (2)");
            return;
        }

        if UNLIKELY (SURF->syncobj) {
            resource->error(WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_SURFACE_EXISTS, "Surface already has a syncobj attached");
            return;
        }

        const auto& RESOURCE = PROTO::sync->m_vSurfaces.emplace_back(
            makeUnique<CDRMSyncobjSurfaceResource>(makeUnique<CWpLinuxDrmSyncobjSurfaceV1>(resource->client(), resource->version(), id), SURF));
        if UNLIKELY (!RESOURCE->good()) {
            resource->noMemory();
            PROTO::sync->m_vSurfaces.pop_back();
            return;
        }

        SURF->syncobj = RESOURCE;

        LOGM(LOG, "New linux_syncobj at {:x} for surface {:x}", (uintptr_t)RESOURCE.get(), (uintptr_t)SURF.get());
    });

    resource->setImportTimeline([this](CWpLinuxDrmSyncobjManagerV1* r, uint32_t id, int32_t fd) {
        const auto& RESOURCE = PROTO::sync->m_vTimelines.emplace_back(
            makeUnique<CDRMSyncobjTimelineResource>(makeUnique<CWpLinuxDrmSyncobjTimelineV1>(resource->client(), resource->version(), id), CFileDescriptor{fd}));
        if UNLIKELY (!RESOURCE->good()) {
            resource->noMemory();
            PROTO::sync->m_vTimelines.pop_back();
            return;
        }

        LOGM(LOG, "New linux_drm_timeline at {:x}", (uintptr_t)RESOURCE.get());
    });
}

bool CDRMSyncobjManagerResource::good() {
    return resource->resource();
}

CDRMSyncobjProtocol::CDRMSyncobjProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name), drmFD(g_pCompositor->m_iDRMFD) {}

void CDRMSyncobjProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto& RESOURCE = m_vManagers.emplace_back(makeUnique<CDRMSyncobjManagerResource>(makeUnique<CWpLinuxDrmSyncobjManagerV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
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
