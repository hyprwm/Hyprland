#include "Compositor.hpp"
#include "Output.hpp"
#include "Seat.hpp"
#include "../types/WLBuffer.hpp"
#include <algorithm>
#include <ranges>
#include "Subcompositor.hpp"
#include "../Viewporter.hpp"
#include "../../helpers/Monitor.hpp"
#include "../../helpers/sync/SyncReleaser.hpp"
#include "../PresentationTime.hpp"
#include "../DRMSyncobj.hpp"
#include "../../render/Renderer.hpp"
#include <cstring>

class CDefaultSurfaceRole : public ISurfaceRole {
  public:
    virtual eSurfaceRole role() {
        return SURFACE_ROLE_UNASSIGNED;
    }
};

CWLCallbackResource::CWLCallbackResource(SP<CWlCallback> resource_) : resource(resource_) {
    ;
}

bool CWLCallbackResource::good() {
    return resource->resource();
}

void CWLCallbackResource::send(timespec* now) {
    resource->sendDone(now->tv_sec * 1000 + now->tv_nsec / 1000000);
}

CWLRegionResource::CWLRegionResource(SP<CWlRegion> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setData(this);

    resource->setDestroy([this](CWlRegion* r) { PROTO::compositor->destroyResource(this); });
    resource->setOnDestroy([this](CWlRegion* r) { PROTO::compositor->destroyResource(this); });

    resource->setAdd([this](CWlRegion* r, int32_t x, int32_t y, int32_t w, int32_t h) { region.add(CBox{x, y, w, h}); });
    resource->setSubtract([this](CWlRegion* r, int32_t x, int32_t y, int32_t w, int32_t h) { region.subtract(CBox{x, y, w, h}); });
}

bool CWLRegionResource::good() {
    return resource->resource();
}

SP<CWLRegionResource> CWLRegionResource::fromResource(wl_resource* res) {
    auto data = (CWLRegionResource*)(((CWlRegion*)wl_resource_get_user_data(res))->data());
    return data ? data->self.lock() : nullptr;
}

CWLSurfaceResource::CWLSurfaceResource(SP<CWlSurface> resource_) : resource(resource_) {
    if (!good())
        return;

    pClient = resource->client();

    resource->setData(this);

    role = makeShared<CDefaultSurfaceRole>();

    resource->setDestroy([this](CWlSurface* r) { destroy(); });
    resource->setOnDestroy([this](CWlSurface* r) { destroy(); });

    resource->setAttach([this](CWlSurface* r, wl_resource* buffer, int32_t x, int32_t y) {
        pending.offset    = {x, y};
        pending.newBuffer = true;

        if (!buffer) {
            pending.buffer.reset();
            pending.texture.reset();
        } else {
            auto res           = CWLBufferResource::fromResource(buffer);
            pending.buffer     = res && res->buffer ? makeShared<CHLBufferReference>(res->buffer.lock(), self.lock()) : nullptr;
            pending.size       = res && res->buffer ? res->buffer->size : Vector2D{};
            pending.texture    = res && res->buffer ? res->buffer->texture : nullptr;
            pending.bufferSize = res && res->buffer ? res->buffer->size : Vector2D{};
        }

        Vector2D oldBufSize = current.buffer ? current.bufferSize : Vector2D{};
        Vector2D newBufSize = pending.buffer ? pending.bufferSize : Vector2D{};

        if (oldBufSize != newBufSize || current.buffer != pending.buffer)
            pending.bufferDamage = CBox{{}, {INT32_MAX, INT32_MAX}};
    });

    resource->setCommit([this](CWlSurface* r) {
        if (pending.texture)
            pending.bufferDamage.intersect(CBox{{}, pending.bufferSize});

        if (!pending.texture)
            pending.size = {};
        else if (pending.viewport.hasDestination)
            pending.size = pending.viewport.destination;
        else if (pending.viewport.hasSource)
            pending.size = pending.viewport.source.size();
        else {
            Vector2D tfs = pending.transform % 2 == 1 ? Vector2D{pending.bufferSize.y, pending.bufferSize.x} : pending.bufferSize;
            pending.size = tfs / pending.scale;
        }

        pending.damage.intersect(CBox{{}, pending.size});

        events.precommit.emit();
        if (pending.rejected) {
            dropPendingBuffer();
            return;
        }

        if (stateLocks <= 0)
            commitPendingState();
    });

    resource->setDamage([this](CWlSurface* r, int32_t x, int32_t y, int32_t w, int32_t h) { pending.damage.add(CBox{x, y, w, h}); });
    resource->setDamageBuffer([this](CWlSurface* r, int32_t x, int32_t y, int32_t w, int32_t h) { pending.bufferDamage.add(CBox{x, y, w, h}); });

    resource->setSetBufferScale([this](CWlSurface* r, int32_t scale) { pending.scale = scale; });
    resource->setSetBufferTransform([this](CWlSurface* r, uint32_t tr) { pending.transform = (wl_output_transform)tr; });

    resource->setSetInputRegion([this](CWlSurface* r, wl_resource* region) {
        if (!region) {
            pending.input = CBox{{}, {INT32_MAX, INT32_MAX}};
            return;
        }

        auto RG       = CWLRegionResource::fromResource(region);
        pending.input = RG->region;
    });

    resource->setSetOpaqueRegion([this](CWlSurface* r, wl_resource* region) {
        if (!region) {
            pending.opaque = CBox{{}, {}};
            return;
        }

        auto RG        = CWLRegionResource::fromResource(region);
        pending.opaque = RG->region;
    });

    resource->setFrame([this](CWlSurface* r, uint32_t id) { callbacks.emplace_back(makeShared<CWLCallbackResource>(makeShared<CWlCallback>(pClient, 1, id))); });

    resource->setOffset([this](CWlSurface* r, int32_t x, int32_t y) { pending.offset = {x, y}; });
}

CWLSurfaceResource::~CWLSurfaceResource() {
    events.destroy.emit();
}

void CWLSurfaceResource::destroy() {
    if (mapped) {
        events.unmap.emit();
        unmap();
    }
    events.destroy.emit();
    releaseBuffers(false);
    PROTO::compositor->destroyResource(this);
}

void CWLSurfaceResource::dropPendingBuffer() {
    pending.buffer.reset();
}

void CWLSurfaceResource::dropCurrentBuffer() {
    current.buffer.reset();
}

SP<CWLSurfaceResource> CWLSurfaceResource::fromResource(wl_resource* res) {
    auto data = (CWLSurfaceResource*)(((CWlSurface*)wl_resource_get_user_data(res))->data());
    return data ? data->self.lock() : nullptr;
}

bool CWLSurfaceResource::good() {
    return resource->resource();
}

wl_client* CWLSurfaceResource::client() {
    return pClient;
}

void CWLSurfaceResource::enter(SP<CMonitor> monitor) {
    if (std::find(enteredOutputs.begin(), enteredOutputs.end(), monitor) != enteredOutputs.end())
        return;

    if (!PROTO::outputs.contains(monitor->szName)) {
        // can happen on unplug/replug
        LOGM(ERR, "enter() called on a non-existent output global");
        return;
    }

    if (PROTO::outputs.at(monitor->szName)->isDefunct()) {
        LOGM(ERR, "enter() called on a defunct output global");
        return;
    }

    auto output = PROTO::outputs.at(monitor->szName)->outputResourceFrom(pClient);

    if (!output || !output->getResource() || !output->getResource()->resource()) {
        LOGM(ERR, "Cannot enter surface {:x} to {}, client hasn't bound the output", (uintptr_t)this, monitor->szName);
        return;
    }

    enteredOutputs.emplace_back(monitor);

    resource->sendEnter(output->getResource().get());
}

void CWLSurfaceResource::leave(SP<CMonitor> monitor) {
    if (std::find(enteredOutputs.begin(), enteredOutputs.end(), monitor) == enteredOutputs.end())
        return;

    auto output = PROTO::outputs.at(monitor->szName)->outputResourceFrom(pClient);

    if (!output) {
        LOGM(ERR, "Cannot leave surface {:x} from {}, client hasn't bound the output", (uintptr_t)this, monitor->szName);
        return;
    }

    std::erase(enteredOutputs, monitor);

    resource->sendLeave(output->getResource().get());
}

void CWLSurfaceResource::sendPreferredTransform(wl_output_transform t) {
    if (resource->version() < 6)
        return;
    resource->sendPreferredBufferTransform(t);
}

void CWLSurfaceResource::sendPreferredScale(int32_t scale) {
    if (resource->version() < 6)
        return;
    resource->sendPreferredBufferScale(scale);
}

void CWLSurfaceResource::frame(timespec* now) {
    if (callbacks.empty())
        return;

    for (auto& c : callbacks) {
        c->send(now);
    }

    callbacks.clear();
}

void CWLSurfaceResource::resetRole() {
    role = makeShared<CDefaultSurfaceRole>();
}

void CWLSurfaceResource::bfHelper(std::vector<SP<CWLSurfaceResource>> nodes, std::function<void(SP<CWLSurfaceResource>, const Vector2D&, void*)> fn, void* data) {

    std::vector<SP<CWLSurfaceResource>> nodes2;

    // first, gather all nodes below
    for (auto& n : nodes) {
        std::erase_if(n->subsurfaces, [](const auto& e) { return e.expired(); });
        // subsurfaces is sorted lowest -> highest
        for (auto& c : n->subsurfaces) {
            if (c->zIndex >= 0)
                break;
            if (c->surface.expired())
                continue;
            nodes2.push_back(c->surface.lock());
        }
    }

    if (!nodes2.empty())
        bfHelper(nodes2, fn, data);

    nodes2.clear();

    for (auto& n : nodes) {
        Vector2D offset = {};
        if (n->role->role() == SURFACE_ROLE_SUBSURFACE) {
            auto subsurface = ((CSubsurfaceRole*)n->role.get())->subsurface.lock();
            offset          = subsurface->posRelativeToParent();
        }

        fn(n, offset, data);
    }

    for (auto& n : nodes) {
        for (auto& c : n->subsurfaces) {
            if (c->zIndex < 0)
                continue;
            if (c->surface.expired())
                continue;
            nodes2.push_back(c->surface.lock());
        }
    }

    if (!nodes2.empty())
        bfHelper(nodes2, fn, data);
}

void CWLSurfaceResource::breadthfirst(std::function<void(SP<CWLSurfaceResource>, const Vector2D&, void*)> fn, void* data) {
    std::vector<SP<CWLSurfaceResource>> surfs;
    surfs.push_back(self.lock());
    bfHelper(surfs, fn, data);
}

std::pair<SP<CWLSurfaceResource>, Vector2D> CWLSurfaceResource::at(const Vector2D& localCoords, bool allowsInput) {
    std::vector<std::pair<SP<CWLSurfaceResource>, Vector2D>> surfs;
    breadthfirst([](SP<CWLSurfaceResource> surf, const Vector2D& offset,
                    void* data) { ((std::vector<std::pair<SP<CWLSurfaceResource>, Vector2D>>*)data)->emplace_back(std::make_pair<>(surf, offset)); },
                 &surfs);

    for (auto& [surf, pos] : surfs | std::views::reverse) {
        if (!allowsInput) {
            const auto BOX = CBox{pos, surf->current.size};
            if (BOX.containsPoint(localCoords))
                return {surf, localCoords - pos};
        } else {
            const auto REGION = surf->current.input.copy().intersect(CBox{{}, surf->current.size}).translate(pos);
            if (REGION.containsPoint(localCoords))
                return {surf, localCoords - pos};
        }
    }

    return {nullptr, {}};
}

uint32_t CWLSurfaceResource::id() {
    return wl_resource_get_id(resource->resource());
}

void CWLSurfaceResource::map() {
    if (mapped)
        return;

    mapped = true;

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    frame(&now);

    current.bufferDamage = CBox{{}, {INT32_MAX, INT32_MAX}};
    pending.bufferDamage = CBox{{}, {INT32_MAX, INT32_MAX}};
}

void CWLSurfaceResource::unmap() {
    if (!mapped)
        return;

    mapped = false;

    // release the buffers.
    // this is necessary for XWayland to function correctly,
    // as it does not unmap via the traditional commit(null buffer) method, but via the X11 protocol.
    releaseBuffers();
}

void CWLSurfaceResource::releaseBuffers(bool onlyCurrent) {
    if (!onlyCurrent)
        dropPendingBuffer();
    dropCurrentBuffer();
}

void CWLSurfaceResource::error(int code, const std::string& str) {
    resource->error(code, str);
}

SP<CWlSurface> CWLSurfaceResource::getResource() {
    return resource;
}

CBox CWLSurfaceResource::extends() {
    CRegion full = CBox{{}, current.size};
    breadthfirst(
        [](SP<CWLSurfaceResource> surf, const Vector2D& offset, void* d) {
            if (surf->role->role() != SURFACE_ROLE_SUBSURFACE)
                return;

            ((CRegion*)d)->add(CBox{offset, surf->current.size});
        },
        &full);
    return full.getExtents();
}

Vector2D CWLSurfaceResource::sourceSize() {
    if (!current.texture)
        return {};

    if (current.viewport.hasSource)
        return current.viewport.source.size();

    Vector2D trc = current.transform % 2 == 1 ? Vector2D{current.bufferSize.y, current.bufferSize.x} : current.bufferSize;
    return trc / current.scale;
}

CRegion CWLSurfaceResource::accumulateCurrentBufferDamage() {
    if (!current.texture)
        return {};

    CRegion surfaceDamage = current.damage;
    if (current.viewport.hasDestination) {
        Vector2D scale = sourceSize() / current.viewport.destination;
        surfaceDamage.scale(scale);
    }

    if (current.viewport.hasSource)
        surfaceDamage.translate(current.viewport.source.pos());

    Vector2D trc = current.transform % 2 == 1 ? Vector2D{current.bufferSize.y, current.bufferSize.x} : current.bufferSize;

    return surfaceDamage.scale(current.scale).transform(wlTransformToHyprutils(invertTransform(current.transform)), trc.x, trc.y).add(current.bufferDamage);
}

void CWLSurfaceResource::lockPendingState() {
    stateLocks++;
}

void CWLSurfaceResource::unlockPendingState() {
    stateLocks--;
    if (stateLocks <= 0)
        commitPendingState();
}

void CWLSurfaceResource::commitPendingState() {
    auto    previousBuffer       = current.buffer;
    CRegion previousBufferDamage = accumulateCurrentBufferDamage();

    current = pending;
    pending.damage.clear();
    pending.bufferDamage.clear();
    pending.newBuffer = false;

    events.roleCommit.emit();

    if (syncobj && syncobj->current.releaseTimeline && syncobj->current.releaseTimeline->timeline && current.buffer && current.buffer->buffer)
        current.buffer->releaser = makeShared<CSyncReleaser>(syncobj->current.releaseTimeline->timeline, syncobj->current.releasePoint);

    if (current.texture)
        current.texture->m_eTransform = wlTransformToHyprutils(current.transform);

    if (current.buffer && current.buffer->buffer) {
        current.buffer->buffer->update(accumulateCurrentBufferDamage());

        // if the surface is a cursor, update the shm buffer
        // TODO: don't update the entire texture
        if (role->role() == SURFACE_ROLE_CURSOR)
            updateCursorShm();

        // release the buffer if it's synchronous as update() has done everything thats needed
        // so we can let the app know we're done.
        if (current.buffer->buffer->isSynchronous()) {
            dropCurrentBuffer();
            dropPendingBuffer(); // pending atm is just a copied ref of the current, drop it too to send a release
        }
    }

    // TODO: we should _accumulate_ and not replace above if sync
    if (role->role() == SURFACE_ROLE_SUBSURFACE) {
        auto subsurface = ((CSubsurfaceRole*)role.get())->subsurface.lock();
        if (subsurface->sync)
            return;

        events.commit.emit();
    } else {
        // send commit to all synced surfaces in this tree.
        breadthfirst(
            [](SP<CWLSurfaceResource> surf, const Vector2D& offset, void* data) {
                if (surf->role->role() == SURFACE_ROLE_SUBSURFACE) {
                    auto subsurface = ((CSubsurfaceRole*)surf->role.get())->subsurface.lock();
                    if (!subsurface->sync)
                        return;
                }
                surf->events.commit.emit();
            },
            nullptr);
    }

    // for async buffers, we can only release the buffer once we are unrefing it from current.
    // if the backend took it, ref it with the lambda. Otherwise, the end of this scope will release it.
    if (previousBuffer && previousBuffer->buffer && !previousBuffer->buffer->isSynchronous()) {
        if (previousBuffer->buffer->lockedByBackend && !previousBuffer->buffer->hlEvents.backendRelease) {
            previousBuffer->buffer->lock();
            previousBuffer->buffer->unlockOnBufferRelease(self);
        }
    }

    lastBuffer = current.buffer ? current.buffer->buffer : WP<IHLBuffer>{};
}

void CWLSurfaceResource::updateCursorShm() {
    auto buf = current.buffer ? current.buffer->buffer : lastBuffer;

    if (!buf)
        return;

    // TODO: actually use damage
    auto& shmData  = CCursorSurfaceRole::cursorPixelData(self.lock());
    auto  shmAttrs = buf->shm();

    if (!shmAttrs.success) {
        LOGM(TRACE, "updateCursorShm: ignoring, not a shm buffer");
        return;
    }

    // no need to end, shm.
    auto [pixelData, fmt, bufLen] = buf->beginDataPtr(0);

    shmData.resize(bufLen);
    memcpy(shmData.data(), pixelData, bufLen);
}

void CWLSurfaceResource::presentFeedback(timespec* when, CMonitor* pMonitor) {
    frame(when);
    auto FEEDBACK = makeShared<CQueuedPresentationData>(self.lock());
    FEEDBACK->attachMonitor(pMonitor);
    FEEDBACK->presented();
    PROTO::presentation->queueData(FEEDBACK);

    if (!pMonitor || !pMonitor->outTimeline || !syncobj)
        return;

    // attach explicit sync
    g_pHyprRenderer->explicitPresented.emplace_back(self.lock());
}

CWLCompositorResource::CWLCompositorResource(SP<CWlCompositor> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setOnDestroy([this](CWlCompositor* r) { PROTO::compositor->destroyResource(this); });

    resource->setCreateSurface([](CWlCompositor* r, uint32_t id) {
        const auto RESOURCE = PROTO::compositor->m_vSurfaces.emplace_back(makeShared<CWLSurfaceResource>(makeShared<CWlSurface>(r->client(), r->version(), id)));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::compositor->m_vSurfaces.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;

        LOGM(LOG, "New wl_surface with id {} at {:x}", id, (uintptr_t)RESOURCE.get());

        PROTO::compositor->events.newSurface.emit(RESOURCE);
    });

    resource->setCreateRegion([](CWlCompositor* r, uint32_t id) {
        const auto RESOURCE = PROTO::compositor->m_vRegions.emplace_back(makeShared<CWLRegionResource>(makeShared<CWlRegion>(r->client(), r->version(), id)));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::compositor->m_vRegions.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;

        LOGM(LOG, "New wl_region with id {} at {:x}", id, (uintptr_t)RESOURCE.get());
    });
}

bool CWLCompositorResource::good() {
    return resource->resource();
}

CWLCompositorProtocol::CWLCompositorProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CWLCompositorProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CWLCompositorResource>(makeShared<CWlCompositor>(client, ver, id)));

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }
}

void CWLCompositorProtocol::destroyResource(CWLCompositorResource* resource) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == resource; });
}

void CWLCompositorProtocol::destroyResource(CWLSurfaceResource* resource) {
    std::erase_if(m_vSurfaces, [&](const auto& other) { return other.get() == resource; });
}

void CWLCompositorProtocol::destroyResource(CWLRegionResource* resource) {
    std::erase_if(m_vRegions, [&](const auto& other) { return other.get() == resource; });
}
