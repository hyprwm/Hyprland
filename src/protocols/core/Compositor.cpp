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
#include "config/ConfigValue.hpp"
#include "protocols/types/SurfaceRole.hpp"
#include "render/Texture.hpp"
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
    if UNLIKELY (!good())
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
    if UNLIKELY (!good())
        return;

    pClient = resource->client();

    resource->setData(this);

    role = makeShared<CDefaultSurfaceRole>();

    resource->setDestroy([this](CWlSurface* r) { destroy(); });
    resource->setOnDestroy([this](CWlSurface* r) { destroy(); });

    resource->setAttach([this](CWlSurface* r, wl_resource* buffer, int32_t x, int32_t y) {
        pending.offset = {x, y};
        pending.updated |= SSurfaceState::eUpdatedProperties::SURFACE_UPDATED_BUFFER | SSurfaceState::eUpdatedProperties::SURFACE_UPDATED_OFFSET;

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
        if (pending.buffer)
            pending.bufferDamage.intersect(CBox{{}, pending.bufferSize});

        if (!pending.buffer)
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

        if (!syncobj)
            commitPendingState(pending);
    });

    resource->setDamage([this](CWlSurface* r, int32_t x, int32_t y, int32_t w, int32_t h) {
        pending.updated |= SSurfaceState::eUpdatedProperties::SURFACE_UPDATED_DAMAGE;
        pending.damage.add(CBox{x, y, w, h});
    });
    resource->setDamageBuffer([this](CWlSurface* r, int32_t x, int32_t y, int32_t w, int32_t h) {
        pending.updated |= SSurfaceState::eUpdatedProperties::SURFACE_UPDATED_DAMAGE;
        pending.bufferDamage.add(CBox{x, y, w, h});
    });

    resource->setSetBufferScale([this](CWlSurface* r, int32_t scale) {
        pending.updated |= SSurfaceState::eUpdatedProperties::SURFACE_UPDATED_SCALE;
        pending.scale = scale;
    });
    resource->setSetBufferTransform([this](CWlSurface* r, uint32_t tr) {
        pending.updated |= SSurfaceState::eUpdatedProperties::SURFACE_UPDATED_TRANSFORM;
        pending.transform = (wl_output_transform)tr;
    });

    resource->setSetInputRegion([this](CWlSurface* r, wl_resource* region) {
        if (!region) {
            pending.input = CBox{{}, {INT32_MAX, INT32_MAX}};
            return;
        }

        pending.updated |= SSurfaceState::eUpdatedProperties::SURFACE_UPDATED_INPUT;

        auto RG       = CWLRegionResource::fromResource(region);
        pending.input = RG->region;
    });

    resource->setSetOpaqueRegion([this](CWlSurface* r, wl_resource* region) {
        if (!region) {
            pending.opaque = CBox{{}, {}};
            return;
        }

        pending.updated |= SSurfaceState::eUpdatedProperties::SURFACE_UPDATED_OPAQUE;

        auto RG        = CWLRegionResource::fromResource(region);
        pending.opaque = RG->region;
    });

    resource->setFrame([this](CWlSurface* r, uint32_t id) { callbacks.emplace_back(makeShared<CWLCallbackResource>(makeShared<CWlCallback>(pClient, 1, id))); });

    resource->setOffset([this](CWlSurface* r, int32_t x, int32_t y) {
        pending.updated |= SSurfaceState::eUpdatedProperties::SURFACE_UPDATED_OFFSET;
        pending.offset = {x, y};
    });
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

void CWLSurfaceResource::enter(PHLMONITOR monitor) {
    if (std::find(enteredOutputs.begin(), enteredOutputs.end(), monitor) != enteredOutputs.end())
        return;

    if UNLIKELY (!PROTO::outputs.contains(monitor->szName)) {
        // can happen on unplug/replug
        LOGM(ERR, "enter() called on a non-existent output global");
        return;
    }

    if UNLIKELY (PROTO::outputs.at(monitor->szName)->isDefunct()) {
        LOGM(ERR, "enter() called on a defunct output global");
        return;
    }

    auto output = PROTO::outputs.at(monitor->szName)->outputResourceFrom(pClient);

    if UNLIKELY (!output || !output->getResource() || !output->getResource()->resource()) {
        LOGM(ERR, "Cannot enter surface {:x} to {}, client hasn't bound the output", (uintptr_t)this, monitor->szName);
        return;
    }

    enteredOutputs.emplace_back(monitor);

    resource->sendEnter(output->getResource().get());
}

void CWLSurfaceResource::leave(PHLMONITOR monitor) {
    if UNLIKELY (std::find(enteredOutputs.begin(), enteredOutputs.end(), monitor) == enteredOutputs.end())
        return;

    auto output = PROTO::outputs.at(monitor->szName)->outputResourceFrom(pClient);

    if UNLIKELY (!output) {
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

    for (auto const& c : callbacks) {
        c->send(now);
    }

    callbacks.clear();
}

void CWLSurfaceResource::resetRole() {
    role = makeShared<CDefaultSurfaceRole>();
}

void CWLSurfaceResource::bfHelper(std::vector<SP<CWLSurfaceResource>> const& nodes, std::function<void(SP<CWLSurfaceResource>, const Vector2D&, void*)> fn, void* data) {

    std::vector<SP<CWLSurfaceResource>> nodes2;
    nodes2.reserve(nodes.size() * 2);

    // first, gather all nodes below
    for (auto const& n : nodes) {
        std::erase_if(n->subsurfaces, [](const auto& e) { return e.expired(); });
        // subsurfaces is sorted lowest -> highest
        for (auto const& c : n->subsurfaces) {
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

    for (auto const& n : nodes) {
        Vector2D offset = {};
        if (n->role->role() == SURFACE_ROLE_SUBSURFACE) {
            auto subsurface = ((CSubsurfaceRole*)n->role.get())->subsurface.lock();
            offset          = subsurface->posRelativeToParent();
        }

        fn(n, offset, data);
    }

    for (auto const& n : nodes) {
        for (auto const& c : n->subsurfaces) {
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

SP<CWLSurfaceResource> CWLSurfaceResource::findFirstPreorderHelper(SP<CWLSurfaceResource> root, std::function<bool(SP<CWLSurfaceResource>)> fn) {
    if (fn(root))
        return root;
    for (auto const& sub : root->subsurfaces) {
        if (sub.expired() || sub->surface.expired())
            continue;
        const auto found = findFirstPreorderHelper(sub->surface.lock(), fn);
        if (found)
            return found;
    }
    return nullptr;
}

SP<CWLSurfaceResource> CWLSurfaceResource::findFirstPreorder(std::function<bool(SP<CWLSurfaceResource>)> fn) {
    return findFirstPreorderHelper(self.lock(), fn);
}

std::pair<SP<CWLSurfaceResource>, Vector2D> CWLSurfaceResource::at(const Vector2D& localCoords, bool allowsInput) {
    std::vector<std::pair<SP<CWLSurfaceResource>, Vector2D>> surfs;
    breadthfirst([&surfs](SP<CWLSurfaceResource> surf, const Vector2D& offset, void* data) { surfs.emplace_back(std::make_pair<>(surf, offset)); }, &surfs);

    for (auto const& [surf, pos] : surfs | std::views::reverse) {
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
    if UNLIKELY (mapped)
        return;

    mapped = true;

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    frame(&now);

    current.bufferDamage = CBox{{}, {INT32_MAX, INT32_MAX}};
    pending.bufferDamage = CBox{{}, {INT32_MAX, INT32_MAX}};
}

void CWLSurfaceResource::unmap() {
    if UNLIKELY (!mapped)
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

void CWLSurfaceResource::commitPendingState(SSurfaceState& state) {
    auto lastTexture = current.texture;
    current.updateFrom(state);
    state.updated = 0;

    if (current.buffer) {
        if (current.buffer->buffer->isSynchronous())
            current.updateSynchronousTexture(lastTexture);

        // if the surface is a cursor, update the shm buffer
        // TODO: don't update the entire texture
        if (role->role() == SURFACE_ROLE_CURSOR)
            updateCursorShm(current.accumulateBufferDamage());
    }

    if (current.texture)
        current.texture->m_eTransform = wlTransformToHyprutils(current.transform);

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

    // release the buffer if it's synchronous (SHM) as update() has done everything thats needed
    // so we can let the app know we're done.
    // if it doesn't have a role, we can't release it yet, in case it gets turned into a cursor.
    if (current.buffer && current.buffer->buffer && current.buffer->buffer->isSynchronous() && role->role() != SURFACE_ROLE_UNASSIGNED)
        dropCurrentBuffer();
}

void CWLSurfaceResource::updateCursorShm(CRegion damage) {
    if (damage.empty())
        return;

    auto buf = current.buffer ? current.buffer->buffer : SP<IHLBuffer>{};

    if UNLIKELY (!buf)
        return;

    auto& shmData  = CCursorSurfaceRole::cursorPixelData(self.lock());
    auto  shmAttrs = buf->shm();

    if (!shmAttrs.success) {
        LOGM(TRACE, "updateCursorShm: ignoring, not a shm buffer");
        return;
    }

    damage.intersect(CBox{0, 0, buf->size.x, buf->size.y});

    // no need to end, shm.
    auto [pixelData, fmt, bufLen] = buf->beginDataPtr(0);

    shmData.resize(bufLen);

    if (const auto RECTS = damage.getRects(); RECTS.size() == 1 && RECTS.at(0).x2 == buf->size.x && RECTS.at(0).y2 == buf->size.y)
        memcpy(shmData.data(), pixelData, bufLen);
    else {
        for (auto& box : damage.getRects()) {
            for (auto y = box.y1; y < box.y2; ++y) {
                // bpp is 32 INSALLAH
                auto begin = 4 * box.y1 * (box.x2 - box.x1) + box.x1;
                auto len   = 4 * (box.x2 - box.x1);
                memcpy((uint8_t*)shmData.data() + begin, (uint8_t*)pixelData + begin, len);
            }
        }
    }
}

void CWLSurfaceResource::presentFeedback(timespec* when, PHLMONITOR pMonitor, bool discarded) {
    frame(when);
    auto FEEDBACK = makeShared<CQueuedPresentationData>(self.lock());
    FEEDBACK->attachMonitor(pMonitor);
    if (discarded)
        FEEDBACK->discarded();
    else
        FEEDBACK->presented();
    PROTO::presentation->queueData(FEEDBACK);

    if (!pMonitor || !pMonitor->inTimeline || !syncobj)
        return;

    // attach explicit sync
    g_pHyprRenderer->explicitPresented.emplace_back(self.lock());
}

CWLCompositorResource::CWLCompositorResource(SP<CWlCompositor> resource_) : resource(resource_) {
    if UNLIKELY (!good())
        return;

    resource->setOnDestroy([this](CWlCompositor* r) { PROTO::compositor->destroyResource(this); });

    resource->setCreateSurface([](CWlCompositor* r, uint32_t id) {
        const auto RESOURCE = PROTO::compositor->m_vSurfaces.emplace_back(makeShared<CWLSurfaceResource>(makeShared<CWlSurface>(r->client(), r->version(), id)));

        if UNLIKELY (!RESOURCE->good()) {
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

        if UNLIKELY (!RESOURCE->good()) {
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

    if UNLIKELY (!RESOURCE->good()) {
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

void CWLCompositorProtocol::forEachSurface(std::function<void(SP<CWLSurfaceResource>)> fn) {
    for (auto& surf : m_vSurfaces) {
        fn(surf);
    }
}
