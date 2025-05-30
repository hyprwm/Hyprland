#include "Compositor.hpp"
#include "Output.hpp"
#include "Seat.hpp"
#include "../types/WLBuffer.hpp"
#include <algorithm>
#include <ranges>
#include "Subcompositor.hpp"
#include "../Viewporter.hpp"
#include "../../helpers/Monitor.hpp"
#include "../PresentationTime.hpp"
#include "../DRMSyncobj.hpp"
#include "../types/DMABuffer.hpp"
#include "../../render/Renderer.hpp"
#include "config/ConfigValue.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "protocols/types/SurfaceRole.hpp"
#include "render/Texture.hpp"
#include <cstring>

class CDefaultSurfaceRole : public ISurfaceRole {
  public:
    virtual eSurfaceRole role() {
        return SURFACE_ROLE_UNASSIGNED;
    }
};

CWLCallbackResource::CWLCallbackResource(SP<CWlCallback> resource_) : m_resource(resource_) {
    ;
}

bool CWLCallbackResource::good() {
    return m_resource->resource();
}

void CWLCallbackResource::send(const Time::steady_tp& now) {
    m_resource->sendDone(Time::millis(now));
}

CWLRegionResource::CWLRegionResource(SP<CWlRegion> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setData(this);

    m_resource->setDestroy([this](CWlRegion* r) { PROTO::compositor->destroyResource(this); });
    m_resource->setOnDestroy([this](CWlRegion* r) { PROTO::compositor->destroyResource(this); });

    m_resource->setAdd([this](CWlRegion* r, int32_t x, int32_t y, int32_t w, int32_t h) { m_region.add(CBox{x, y, w, h}); });
    m_resource->setSubtract([this](CWlRegion* r, int32_t x, int32_t y, int32_t w, int32_t h) { m_region.subtract(CBox{x, y, w, h}); });
}

bool CWLRegionResource::good() {
    return m_resource->resource();
}

SP<CWLRegionResource> CWLRegionResource::fromResource(wl_resource* res) {
    auto data = (CWLRegionResource*)(((CWlRegion*)wl_resource_get_user_data(res))->data());
    return data ? data->m_self.lock() : nullptr;
}

CWLSurfaceResource::CWLSurfaceResource(SP<CWlSurface> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_client = m_resource->client();

    m_resource->setData(this);

    m_role = makeShared<CDefaultSurfaceRole>();

    m_resource->setDestroy([this](CWlSurface* r) { destroy(); });
    m_resource->setOnDestroy([this](CWlSurface* r) { destroy(); });

    m_resource->setAttach([this](CWlSurface* r, wl_resource* buffer, int32_t x, int32_t y) {
        m_pending.updated.buffer = true;
        m_pending.updated.offset = true;

        m_pending.offset = {x, y};

        if (m_pending.buffer)
            m_pending.buffer.drop();

        auto buf = buffer ? CWLBufferResource::fromResource(buffer) : nullptr;

        if (buf && buf->m_buffer) {
            m_pending.buffer     = CHLBufferReference(buf->m_buffer.lock());
            m_pending.texture    = buf->m_buffer->m_texture;
            m_pending.size       = buf->m_buffer->size;
            m_pending.bufferSize = buf->m_buffer->size;
        } else {
            m_pending.buffer = {};
            m_pending.texture.reset();
            m_pending.size       = Vector2D{};
            m_pending.bufferSize = Vector2D{};
        }

        if (m_pending.bufferSize != m_current.bufferSize) {
            m_pending.updated.damage = true;
            m_pending.bufferDamage   = CBox{{}, {INT32_MAX, INT32_MAX}};
        }
    });

    m_resource->setCommit([this](CWlSurface* r) {
        if (m_pending.buffer)
            m_pending.bufferDamage.intersect(CBox{{}, m_pending.bufferSize});

        if (!m_pending.buffer)
            m_pending.size = {};
        else if (m_pending.viewport.hasDestination)
            m_pending.size = m_pending.viewport.destination;
        else if (m_pending.viewport.hasSource)
            m_pending.size = m_pending.viewport.source.size();
        else {
            Vector2D tfs   = m_pending.transform % 2 == 1 ? Vector2D{m_pending.bufferSize.y, m_pending.bufferSize.x} : m_pending.bufferSize;
            m_pending.size = tfs / m_pending.scale;
        }

        m_pending.damage.intersect(CBox{{}, m_pending.size});

        m_events.precommit.emit();
        if (m_pending.rejected) {
            m_pending.rejected = false;
            dropPendingBuffer();
            return;
        }

        if ((!m_pending.updated.buffer) ||            // no new buffer attached
            (!m_pending.buffer && !m_pending.texture) // null buffer attached
        ) {
            commitState(m_pending);
            m_pending.reset();
            return;
        }

        // save state while we wait for buffer to become ready to read
        const auto& state = m_pendingStates.emplace(makeUnique<SSurfaceState>(m_pending));
        m_pending.reset();

        auto whenReadable = [this, surf = m_self, state = WP<SSurfaceState>(m_pendingStates.back())] {
            if (!surf || state.expired())
                return;

            while (!m_pendingStates.empty() && m_pendingStates.front() != state) {
                commitState(*m_pendingStates.front());
                m_pendingStates.pop();
            }

            commitState(*m_pendingStates.front());
            m_pendingStates.pop();
        };

        if (state->updated.acquire) {
            // wait on acquire point for this surface, from explicit sync protocol
            state->acquire.addWaiter(whenReadable);
        } else if (state->buffer->isSynchronous()) {
            // synchronous (shm) buffers can be read immediately
            whenReadable();
        } else if (state->buffer->type() == Aquamarine::BUFFER_TYPE_DMABUF && state->buffer->dmabuf().success) {
            // async buffer and is dmabuf, then we can wait on implicit fences
            auto syncFd = dynamic_cast<CDMABuffer*>(state->buffer.m_buffer.get())->exportSyncFile();

            if (syncFd.isValid())
                g_pEventLoopManager->doOnReadable(std::move(syncFd), whenReadable);
            else
                whenReadable();
        } else {
            Debug::log(ERR, "BUG THIS: wl_surface.commit: no acquire, non-dmabuf, async buffer, needs wait... this shouldn't happen");
            whenReadable();
        }
    });

    m_resource->setDamage([this](CWlSurface* r, int32_t x, int32_t y, int32_t w, int32_t h) {
        m_pending.updated.damage = true;
        m_pending.damage.add(CBox{x, y, w, h});
    });
    m_resource->setDamageBuffer([this](CWlSurface* r, int32_t x, int32_t y, int32_t w, int32_t h) {
        m_pending.updated.damage = true;
        m_pending.bufferDamage.add(CBox{x, y, w, h});
    });

    m_resource->setSetBufferScale([this](CWlSurface* r, int32_t scale) {
        if (scale == m_pending.scale)
            return;

        m_pending.updated.scale  = true;
        m_pending.updated.damage = true;

        m_pending.scale        = scale;
        m_pending.bufferDamage = CBox{{}, {INT32_MAX, INT32_MAX}};
    });

    m_resource->setSetBufferTransform([this](CWlSurface* r, uint32_t tr) {
        if (tr == m_pending.transform)
            return;

        m_pending.updated.transform = true;
        m_pending.updated.damage    = true;

        m_pending.transform    = (wl_output_transform)tr;
        m_pending.bufferDamage = CBox{{}, {INT32_MAX, INT32_MAX}};
    });

    m_resource->setSetInputRegion([this](CWlSurface* r, wl_resource* region) {
        m_pending.updated.input = true;

        if (!region) {
            m_pending.input = CBox{{}, {INT32_MAX, INT32_MAX}};
            return;
        }

        auto RG         = CWLRegionResource::fromResource(region);
        m_pending.input = RG->m_region;
    });

    m_resource->setSetOpaqueRegion([this](CWlSurface* r, wl_resource* region) {
        m_pending.updated.opaque = true;

        if (!region) {
            m_pending.opaque = CBox{{}, {}};
            return;
        }

        auto RG          = CWLRegionResource::fromResource(region);
        m_pending.opaque = RG->m_region;
    });

    m_resource->setFrame([this](CWlSurface* r, uint32_t id) { m_callbacks.emplace_back(makeShared<CWLCallbackResource>(makeShared<CWlCallback>(m_client, 1, id))); });

    m_resource->setOffset([this](CWlSurface* r, int32_t x, int32_t y) {
        m_pending.updated.offset = true;
        m_pending.offset         = {x, y};
    });
}

CWLSurfaceResource::~CWLSurfaceResource() {
    m_events.destroy.emit();
}

void CWLSurfaceResource::destroy() {
    if (m_mapped) {
        m_events.unmap.emit();
        unmap();
    }
    m_events.destroy.emit();
    releaseBuffers(false);
    PROTO::compositor->destroyResource(this);
}

void CWLSurfaceResource::dropPendingBuffer() {
    m_pending.buffer = {};
}

void CWLSurfaceResource::dropCurrentBuffer() {
    m_current.buffer = {};
}

SP<CWLSurfaceResource> CWLSurfaceResource::fromResource(wl_resource* res) {
    auto data = (CWLSurfaceResource*)(((CWlSurface*)wl_resource_get_user_data(res))->data());
    return data ? data->m_self.lock() : nullptr;
}

bool CWLSurfaceResource::good() {
    return m_resource->resource();
}

wl_client* CWLSurfaceResource::client() {
    return m_client;
}

void CWLSurfaceResource::enter(PHLMONITOR monitor) {
    if (std::find(m_enteredOutputs.begin(), m_enteredOutputs.end(), monitor) != m_enteredOutputs.end())
        return;

    if UNLIKELY (!PROTO::outputs.contains(monitor->m_name)) {
        // can happen on unplug/replug
        LOGM(ERR, "enter() called on a non-existent output global");
        return;
    }

    if UNLIKELY (PROTO::outputs.at(monitor->m_name)->isDefunct()) {
        LOGM(ERR, "enter() called on a defunct output global");
        return;
    }

    auto output = PROTO::outputs.at(monitor->m_name)->outputResourceFrom(m_client);

    if UNLIKELY (!output || !output->getResource() || !output->getResource()->resource()) {
        LOGM(ERR, "Cannot enter surface {:x} to {}, client hasn't bound the output", (uintptr_t)this, monitor->m_name);
        return;
    }

    m_enteredOutputs.emplace_back(monitor);

    m_resource->sendEnter(output->getResource().get());
}

void CWLSurfaceResource::leave(PHLMONITOR monitor) {
    if UNLIKELY (std::find(m_enteredOutputs.begin(), m_enteredOutputs.end(), monitor) == m_enteredOutputs.end())
        return;

    auto output = PROTO::outputs.at(monitor->m_name)->outputResourceFrom(m_client);

    if UNLIKELY (!output) {
        LOGM(ERR, "Cannot leave surface {:x} from {}, client hasn't bound the output", (uintptr_t)this, monitor->m_name);
        return;
    }

    std::erase(m_enteredOutputs, monitor);

    m_resource->sendLeave(output->getResource().get());
}

void CWLSurfaceResource::sendPreferredTransform(wl_output_transform t) {
    if (m_resource->version() < 6)
        return;
    m_resource->sendPreferredBufferTransform(t);
}

void CWLSurfaceResource::sendPreferredScale(int32_t scale) {
    if (m_resource->version() < 6)
        return;
    m_resource->sendPreferredBufferScale(scale);
}

void CWLSurfaceResource::frame(const Time::steady_tp& now) {
    if (m_callbacks.empty())
        return;

    for (auto const& c : m_callbacks) {
        c->send(now);
    }

    m_callbacks.clear();
}

void CWLSurfaceResource::resetRole() {
    m_role = makeShared<CDefaultSurfaceRole>();
}

void CWLSurfaceResource::bfHelper(std::vector<SP<CWLSurfaceResource>> const& nodes, std::function<void(SP<CWLSurfaceResource>, const Vector2D&, void*)> fn, void* data) {
    std::vector<SP<CWLSurfaceResource>> nodes2;
    nodes2.reserve(nodes.size() * 2);

    // first, gather all nodes below
    for (auto const& n : nodes) {
        std::erase_if(n->m_subsurfaces, [](const auto& e) { return e.expired(); });
        // subsurfaces is sorted lowest -> highest
        for (auto const& c : n->m_subsurfaces) {
            if (c->m_zIndex >= 0)
                break;
            if (c->m_surface.expired())
                continue;
            nodes2.push_back(c->m_surface.lock());
        }
    }

    if (!nodes2.empty())
        bfHelper(nodes2, fn, data);

    nodes2.clear();

    for (auto const& n : nodes) {
        Vector2D offset = {};
        if (n->m_role->role() == SURFACE_ROLE_SUBSURFACE) {
            auto subsurface = ((CSubsurfaceRole*)n->m_role.get())->m_subsurface.lock();
            offset          = subsurface->posRelativeToParent();
        }

        fn(n, offset, data);
    }

    for (auto const& n : nodes) {
        for (auto const& c : n->m_subsurfaces) {
            if (c->m_zIndex < 0)
                continue;
            if (c->m_surface.expired())
                continue;
            nodes2.push_back(c->m_surface.lock());
        }
    }

    if (!nodes2.empty())
        bfHelper(nodes2, fn, data);
}

void CWLSurfaceResource::breadthfirst(std::function<void(SP<CWLSurfaceResource>, const Vector2D&, void*)> fn, void* data) {
    std::vector<SP<CWLSurfaceResource>> surfs;
    surfs.push_back(m_self.lock());
    bfHelper(surfs, fn, data);
}

SP<CWLSurfaceResource> CWLSurfaceResource::findFirstPreorderHelper(SP<CWLSurfaceResource> root, std::function<bool(SP<CWLSurfaceResource>)> fn) {
    if (fn(root))
        return root;
    for (auto const& sub : root->m_subsurfaces) {
        if (sub.expired() || sub->m_surface.expired())
            continue;
        const auto found = findFirstPreorderHelper(sub->m_surface.lock(), fn);
        if (found)
            return found;
    }
    return nullptr;
}

SP<CWLSurfaceResource> CWLSurfaceResource::findFirstPreorder(std::function<bool(SP<CWLSurfaceResource>)> fn) {
    return findFirstPreorderHelper(m_self.lock(), fn);
}

std::pair<SP<CWLSurfaceResource>, Vector2D> CWLSurfaceResource::at(const Vector2D& localCoords, bool allowsInput) {
    std::vector<std::pair<SP<CWLSurfaceResource>, Vector2D>> surfs;
    breadthfirst([&surfs](SP<CWLSurfaceResource> surf, const Vector2D& offset, void* data) { surfs.emplace_back(std::make_pair<>(surf, offset)); }, &surfs);

    for (auto const& [surf, pos] : surfs | std::views::reverse) {
        if (!allowsInput) {
            const auto BOX = CBox{pos, surf->m_current.size};
            if (BOX.containsPoint(localCoords))
                return {surf, localCoords - pos};
        } else {
            const auto REGION = surf->m_current.input.copy().intersect(CBox{{}, surf->m_current.size}).translate(pos);
            if (REGION.containsPoint(localCoords))
                return {surf, localCoords - pos};
        }
    }

    return {nullptr, {}};
}

uint32_t CWLSurfaceResource::id() {
    return wl_resource_get_id(m_resource->resource());
}

void CWLSurfaceResource::map() {
    if UNLIKELY (m_mapped)
        return;

    m_mapped = true;

    frame(Time::steadyNow());

    m_current.bufferDamage = CBox{{}, {INT32_MAX, INT32_MAX}};
    m_pending.bufferDamage = CBox{{}, {INT32_MAX, INT32_MAX}};
}

void CWLSurfaceResource::unmap() {
    if UNLIKELY (!m_mapped)
        return;

    m_mapped = false;

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
    m_resource->error(code, str);
}

SP<CWlSurface> CWLSurfaceResource::getResource() {
    return m_resource;
}

CBox CWLSurfaceResource::extends() {
    CRegion full = CBox{{}, m_current.size};
    breadthfirst(
        [](SP<CWLSurfaceResource> surf, const Vector2D& offset, void* d) {
            if (surf->m_role->role() != SURFACE_ROLE_SUBSURFACE)
                return;

            ((CRegion*)d)->add(CBox{offset, surf->m_current.size});
        },
        &full);
    return full.getExtents();
}

void CWLSurfaceResource::commitState(SSurfaceState& state) {
    auto lastTexture = m_current.texture;
    m_current.updateFrom(state);

    if (m_current.buffer) {
        if (m_current.buffer->isSynchronous())
            m_current.updateSynchronousTexture(lastTexture);

        // if the surface is a cursor, update the shm buffer
        // TODO: don't update the entire texture
        if (m_role->role() == SURFACE_ROLE_CURSOR)
            updateCursorShm(m_current.accumulateBufferDamage());
    }

    if (m_current.texture)
        m_current.texture->m_transform = wlTransformToHyprutils(m_current.transform);

    if (m_role->role() == SURFACE_ROLE_SUBSURFACE) {
        auto subsurface = ((CSubsurfaceRole*)m_role.get())->m_subsurface.lock();
        if (subsurface->m_sync)
            return;

        m_events.commit.emit();
    } else {
        // send commit to all synced surfaces in this tree.
        breadthfirst(
            [](SP<CWLSurfaceResource> surf, const Vector2D& offset, void* data) {
                if (surf->m_role->role() == SURFACE_ROLE_SUBSURFACE) {
                    auto subsurface = ((CSubsurfaceRole*)surf->m_role.get())->m_subsurface.lock();
                    if (!subsurface->m_sync)
                        return;
                }
                surf->m_events.commit.emit();
            },
            nullptr);
    }

    // release the buffer if it's synchronous (SHM) as updateSynchronousTexture() has copied the buffer data to a GPU tex
    // if it doesn't have a role, we can't release it yet, in case it gets turned into a cursor.
    if (m_current.buffer && m_current.buffer->isSynchronous() && m_role->role() != SURFACE_ROLE_UNASSIGNED)
        dropCurrentBuffer();
}

void CWLSurfaceResource::updateCursorShm(CRegion damage) {
    if (damage.empty())
        return;

    auto buf = m_current.buffer ? m_current.buffer : SP<IHLBuffer>{};

    if UNLIKELY (!buf)
        return;

    auto& shmData  = CCursorSurfaceRole::cursorPixelData(m_self.lock());
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

void CWLSurfaceResource::presentFeedback(const Time::steady_tp& when, PHLMONITOR pMonitor, bool discarded) {
    frame(when);
    auto FEEDBACK = makeShared<CQueuedPresentationData>(m_self.lock());
    FEEDBACK->attachMonitor(pMonitor);
    if (discarded)
        FEEDBACK->discarded();
    else
        FEEDBACK->presented();
    PROTO::presentation->queueData(FEEDBACK);
}

CWLCompositorResource::CWLCompositorResource(SP<CWlCompositor> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CWlCompositor* r) { PROTO::compositor->destroyResource(this); });

    m_resource->setCreateSurface([](CWlCompositor* r, uint32_t id) {
        const auto RESOURCE = PROTO::compositor->m_surfaces.emplace_back(makeShared<CWLSurfaceResource>(makeShared<CWlSurface>(r->client(), r->version(), id)));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::compositor->m_surfaces.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;

        LOGM(LOG, "New wl_surface with id {} at {:x}", id, (uintptr_t)RESOURCE.get());

        PROTO::compositor->m_events.newSurface.emit(RESOURCE);
    });

    m_resource->setCreateRegion([](CWlCompositor* r, uint32_t id) {
        const auto RESOURCE = PROTO::compositor->m_regions.emplace_back(makeShared<CWLRegionResource>(makeShared<CWlRegion>(r->client(), r->version(), id)));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::compositor->m_regions.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;

        LOGM(LOG, "New wl_region with id {} at {:x}", id, (uintptr_t)RESOURCE.get());
    });
}

bool CWLCompositorResource::good() {
    return m_resource->resource();
}

CWLCompositorProtocol::CWLCompositorProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CWLCompositorProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CWLCompositorResource>(makeShared<CWlCompositor>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }
}

void CWLCompositorProtocol::destroyResource(CWLCompositorResource* resource) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == resource; });
}

void CWLCompositorProtocol::destroyResource(CWLSurfaceResource* resource) {
    std::erase_if(m_surfaces, [&](const auto& other) { return other.get() == resource; });
}

void CWLCompositorProtocol::destroyResource(CWLRegionResource* resource) {
    std::erase_if(m_regions, [&](const auto& other) { return other.get() == resource; });
}

void CWLCompositorProtocol::forEachSurface(std::function<void(SP<CWLSurfaceResource>)> fn) {
    for (auto& surf : m_surfaces) {
        fn(surf);
    }
}
