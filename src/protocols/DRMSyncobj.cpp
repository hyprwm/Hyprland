#include "DRMSyncobj.hpp"
#include <algorithm>

#include "core/Compositor.hpp"
#include "../helpers/sync/SyncTimeline.hpp"
#include "../Compositor.hpp"
#include "render/OpenGL.hpp"

#include <fcntl.h>
using namespace Hyprutils::OS;

CDRMSyncPointState::CDRMSyncPointState(SP<CSyncTimeline> timeline_, uint64_t point_) : m_timeline(timeline_), m_point(point_) {}

const uint64_t& CDRMSyncPointState::point() {
    return m_point;
}

WP<CSyncTimeline> CDRMSyncPointState::timeline() {
    return m_timeline;
}

UP<CSyncReleaser> CDRMSyncPointState::createSyncRelease() {
    if (m_releaseTaken)
        Debug::log(ERR, "CDRMSyncPointState: creating a sync releaser on an already created SyncRelease");

    m_releaseTaken = true;
    return makeUnique<CSyncReleaser>(m_timeline, m_point);
}

bool CDRMSyncPointState::addWaiter(const std::function<void()>& waiter) {
    m_acquireCommitted = true;
    return m_timeline->addWaiter(waiter, m_point, 0u);
}

bool CDRMSyncPointState::comitted() {
    return m_acquireCommitted;
}

CFileDescriptor CDRMSyncPointState::exportAsFD() {
    return m_timeline->exportAsSyncFileFD(m_point);
}

void CDRMSyncPointState::signal() {
    m_timeline->signal(m_point);
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

        auto timeline  = CDRMSyncobjTimelineResource::fromResource(timeline_);
        pendingAcquire = {timeline->timeline, ((uint64_t)hi << 32) | (uint64_t)lo};
    });

    resource->setSetReleasePoint([this](CWpLinuxDrmSyncobjSurfaceV1* r, wl_resource* timeline_, uint32_t hi, uint32_t lo) {
        if (!surface) {
            resource->error(WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_SURFACE, "Surface is gone");
            return;
        }

        auto timeline  = CDRMSyncobjTimelineResource::fromResource(timeline_);
        pendingRelease = {timeline->timeline, ((uint64_t)hi << 32) | (uint64_t)lo};
    });

    listeners.surfacePrecommit = surface->events.precommit.registerListener([this](std::any d) {
        const bool PENDING_HAS_NEW_BUFFER = surface->pending.updated & SSurfaceState::eUpdatedProperties::SURFACE_UPDATED_BUFFER;

        if (!surface->pending.buffer && PENDING_HAS_NEW_BUFFER && !surface->pending.texture) {
            removeAllWaiters();
            surface->commitPendingState(surface->pending);
            return; // null buffer attached.
        }

        if (!surface->pending.buffer && !PENDING_HAS_NEW_BUFFER && surface->current.buffer) {
            surface->current.bufferDamage.clear();
            surface->current.damage.clear();
            surface->commitPendingState(surface->current);
            return; // no new buffer, but we still have current around and a commit happend, commit current again.
        }

        if (!surface->pending.buffer && !PENDING_HAS_NEW_BUFFER && !surface->current.buffer) {
            surface->commitPendingState(surface->pending); // no pending buffer, no current buffer. probably first commit
            return;
        }

        if (pendingAcquire.timeline()) {
            surface->pending.buffer.acquire = makeUnique<CDRMSyncPointState>(std::move(pendingAcquire));
            pendingAcquire                  = {};
        }

        if (pendingRelease.timeline()) {
            surface->pending.buffer.release = makeUnique<CDRMSyncPointState>(std::move(pendingRelease));
            pendingRelease                  = {};
        }

        if (protocolError())
            return;

        const auto& state = pendingStates.emplace_back(makeShared<SSurfaceState>(surface->pending));
        surface->pending.damage.clear();
        surface->pending.bufferDamage.clear();
        surface->pending.updated &= ~SSurfaceState::eUpdatedProperties::SURFACE_UPDATED_BUFFER;
        surface->pending.updated &= ~SSurfaceState::eUpdatedProperties::SURFACE_UPDATED_DAMAGE;
        surface->pending.buffer = {};

        state->buffer->syncReleaser = state->buffer.release->createSyncRelease();
        state->buffer.acquire->addWaiter([this, surf = surface, wp = CWeakPointer<SSurfaceState>(*std::prev(pendingStates.end()))] {
            if (!surf)
                return;

            surf->commitPendingState(*wp.lock());
            std::erase(pendingStates, wp);
        });
    });
}

void CDRMSyncobjSurfaceResource::removeAllWaiters() {
    for (auto& s : pendingStates) {
        if (s && s->buffer && s->buffer.acquire)
            s->buffer.acquire->timeline()->removeAllWaiters();
    }

    pendingStates.clear();
}

CDRMSyncobjSurfaceResource::~CDRMSyncobjSurfaceResource() {
    removeAllWaiters();
}

bool CDRMSyncobjSurfaceResource::protocolError() {
    if (!surface->pending.buffer) {
        resource->error(WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_BUFFER, "Missing buffer");
        surface->pending.rejected = true;
        return true;
    }

    if (!surface->pending.buffer.acquire || !surface->pending.buffer.acquire->timeline()) {
        resource->error(WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_ACQUIRE_POINT, "Missing acquire timeline");
        surface->pending.rejected = true;
        return true;
    }

    if (!surface->pending.buffer.release || !surface->pending.buffer.release->timeline()) {
        resource->error(WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_RELEASE_POINT, "Missing release timeline");
        surface->pending.rejected = true;
        return true;
    }

    if (surface->pending.buffer.acquire->timeline() == surface->pending.buffer.release->timeline()) {
        if (surface->pending.buffer.acquire->point() >= surface->pending.buffer.release->point()) {
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

    timeline = CSyncTimeline::create(PROTO::sync->drmFD, std::move(fd));

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
