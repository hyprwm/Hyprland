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

bool CDRMSyncPointState::addWaiter(std::function<void()>&& waiter) {
    m_acquireCommitted = true;
    return m_timeline->addWaiter(std::move(waiter), m_point, 0u);
}

bool CDRMSyncPointState::committed() {
    return m_acquireCommitted;
}

CFileDescriptor CDRMSyncPointState::exportAsFD() {
    return m_timeline->exportAsSyncFileFD(m_point);
}

void CDRMSyncPointState::signal() {
    m_timeline->signal(m_point);
}

CDRMSyncobjSurfaceResource::CDRMSyncobjSurfaceResource(UP<CWpLinuxDrmSyncobjSurfaceV1>&& resource_, SP<CWLSurfaceResource> surface_) :
    m_surface(surface_), m_resource(std::move(resource_)) {
    if UNLIKELY (!good())
        return;

    m_resource->setData(this);

    m_resource->setOnDestroy([this](CWpLinuxDrmSyncobjSurfaceV1* r) { PROTO::sync->destroyResource(this); });
    m_resource->setDestroy([this](CWpLinuxDrmSyncobjSurfaceV1* r) { PROTO::sync->destroyResource(this); });

    m_resource->setSetAcquirePoint([this](CWpLinuxDrmSyncobjSurfaceV1* r, wl_resource* timeline_, uint32_t hi, uint32_t lo) {
        if (!m_surface) {
            m_resource->error(WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_SURFACE, "Surface is gone");
            return;
        }

        auto timeline    = CDRMSyncobjTimelineResource::fromResource(timeline_);
        m_pendingAcquire = {timeline->m_timeline, (sc<uint64_t>(hi) << 32) | sc<uint64_t>(lo)};
    });

    m_resource->setSetReleasePoint([this](CWpLinuxDrmSyncobjSurfaceV1* r, wl_resource* timeline_, uint32_t hi, uint32_t lo) {
        if (!m_surface) {
            m_resource->error(WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_SURFACE, "Surface is gone");
            return;
        }

        auto timeline    = CDRMSyncobjTimelineResource::fromResource(timeline_);
        m_pendingRelease = {timeline->m_timeline, (sc<uint64_t>(hi) << 32) | sc<uint64_t>(lo)};
    });

    m_listeners.surfaceStateCommit = m_surface->m_events.stateCommit.listen([this](auto state) {
        if (!state->updated.bits.buffer || !state->buffer) {
            if (m_pendingAcquire.timeline() || m_pendingRelease.timeline()) {
                m_resource->error(WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_BUFFER, "Missing buffer");
                state->rejected = true;
            }
            return;
        }

        if (!m_pendingAcquire.timeline()) {
            m_resource->error(WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_ACQUIRE_POINT, "Missing acquire timeline");
            state->rejected = true;
            return;
        }

        if (!m_pendingRelease.timeline()) {
            m_resource->error(WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_RELEASE_POINT, "Missing release timeline");
            state->rejected = true;
            return;
        }

        if (m_pendingAcquire.timeline() == m_pendingRelease.timeline() && m_pendingAcquire.point() >= m_pendingRelease.point()) {
            m_resource->error(WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_CONFLICTING_POINTS, "Acquire and release points are on the same timeline, and acquire >= release");
            state->rejected = true;
            return;
        }

        state->updated.bits.acquire = true;
        state->acquire              = m_pendingAcquire;
        state->lockMask |= LockReason::Fence;
        m_pendingAcquire = {};

        state->buffer->addReleasePoint(m_pendingRelease);
        m_pendingRelease = {};
    });
}

bool CDRMSyncobjSurfaceResource::good() {
    return m_resource->resource();
}

CDRMSyncobjTimelineResource::CDRMSyncobjTimelineResource(UP<CWpLinuxDrmSyncobjTimelineV1>&& resource_, CFileDescriptor&& fd_) :
    m_fd(std::move(fd_)), m_resource(std::move(resource_)) {
    if UNLIKELY (!good())
        return;

    m_resource->setData(this);

    m_resource->setOnDestroy([this](CWpLinuxDrmSyncobjTimelineV1* r) { PROTO::sync->destroyResource(this); });
    m_resource->setDestroy([this](CWpLinuxDrmSyncobjTimelineV1* r) { PROTO::sync->destroyResource(this); });

    m_timeline = CSyncTimeline::create(PROTO::sync->m_drmFD, std::move(m_fd));

    if (!m_timeline) {
        m_resource->error(WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_INVALID_TIMELINE, "Timeline failed importing");
        return;
    }
}

WP<CDRMSyncobjTimelineResource> CDRMSyncobjTimelineResource::fromResource(wl_resource* res) {
    for (const auto& r : PROTO::sync->m_timelines) {
        if (r && r->m_resource && r->m_resource->resource() == res)
            return r;
    }

    return {};
}

bool CDRMSyncobjTimelineResource::good() {
    return m_resource->resource();
}

CDRMSyncobjManagerResource::CDRMSyncobjManagerResource(UP<CWpLinuxDrmSyncobjManagerV1>&& resource_) : m_resource(std::move(resource_)) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CWpLinuxDrmSyncobjManagerV1* r) { PROTO::sync->destroyResource(this); });
    m_resource->setDestroy([this](CWpLinuxDrmSyncobjManagerV1* r) { PROTO::sync->destroyResource(this); });

    m_resource->setGetSurface([this](CWpLinuxDrmSyncobjManagerV1* r, uint32_t id, wl_resource* surf) {
        if UNLIKELY (!surf) {
            m_resource->error(-1, "Invalid surface");
            return;
        }

        auto SURF = CWLSurfaceResource::fromResource(surf);
        if UNLIKELY (!SURF) {
            m_resource->error(-1, "Invalid surface (2)");
            return;
        }

        if UNLIKELY (SURF->m_syncobj) {
            m_resource->error(WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_SURFACE_EXISTS, "Surface already has a syncobj attached");
            return;
        }

        const auto& RESOURCE = PROTO::sync->m_surfaces.emplace_back(
            makeUnique<CDRMSyncobjSurfaceResource>(makeUnique<CWpLinuxDrmSyncobjSurfaceV1>(m_resource->client(), m_resource->version(), id), SURF));
        if UNLIKELY (!RESOURCE->good()) {
            m_resource->noMemory();
            PROTO::sync->m_surfaces.pop_back();
            return;
        }

        SURF->m_syncobj = RESOURCE;

        LOGM(LOG, "New linux_syncobj at {:x} for surface {:x}", (uintptr_t)RESOURCE.get(), (uintptr_t)SURF.get());
    });

    m_resource->setImportTimeline([this](CWpLinuxDrmSyncobjManagerV1* r, uint32_t id, int32_t fd) {
        const auto& RESOURCE = PROTO::sync->m_timelines.emplace_back(
            makeUnique<CDRMSyncobjTimelineResource>(makeUnique<CWpLinuxDrmSyncobjTimelineV1>(m_resource->client(), m_resource->version(), id), CFileDescriptor{fd}));
        if UNLIKELY (!RESOURCE->good()) {
            m_resource->noMemory();
            PROTO::sync->m_timelines.pop_back();
            return;
        }

        LOGM(LOG, "New linux_drm_timeline at {:x}", (uintptr_t)RESOURCE.get());
    });
}

bool CDRMSyncobjManagerResource::good() {
    return m_resource->resource();
}

CDRMSyncobjProtocol::CDRMSyncobjProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    if (g_pCompositor->m_drmRenderNode.syncObjSupport)
        m_drmFD = g_pCompositor->m_drmRenderNode.fd;
    else if (g_pCompositor->m_drm.syncobjSupport)
        m_drmFD = g_pCompositor->m_drm.fd;
    else {
        LOGM(ERR, "CDRMSyncobjProtocol: no nodes support explicit sync?");
        return;
    }

    LOGM(LOG, "CDRMSyncobjProtocol: using fd {}", m_drmFD);
}

void CDRMSyncobjProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto& RESOURCE = m_managers.emplace_back(makeUnique<CDRMSyncobjManagerResource>(makeUnique<CWpLinuxDrmSyncobjManagerV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }
}

void CDRMSyncobjProtocol::destroyResource(CDRMSyncobjManagerResource* resource) {
    std::erase_if(m_managers, [resource](const auto& e) { return e.get() == resource; });
}

void CDRMSyncobjProtocol::destroyResource(CDRMSyncobjTimelineResource* resource) {
    std::erase_if(m_timelines, [resource](const auto& e) { return e.get() == resource; });
}

void CDRMSyncobjProtocol::destroyResource(CDRMSyncobjSurfaceResource* resource) {
    std::erase_if(m_surfaces, [resource](const auto& e) { return e.get() == resource; });
}
