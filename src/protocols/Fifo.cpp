#include "Fifo.hpp"
#include "core/Compositor.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../helpers/Monitor.hpp"

CFifoResource::CFifoResource(UP<CWpFifoV1>&& resource_, SP<CWLSurfaceResource> surface) : m_resource(std::move(resource_)), m_surface(surface) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setDestroy([this](CWpFifoV1* r) { PROTO::fifo->destroyResource(this); });
    m_resource->setOnDestroy([this](CWpFifoV1* r) { PROTO::fifo->destroyResource(this); });

    m_resource->setSetBarrier([this](CWpFifoV1* r) {
        if (!m_surface) {
            r->error(WP_FIFO_V1_ERROR_SURFACE_DESTROYED, "Surface was gone");
            return;
        }

        m_pending.barrierSet = true;
    });

    m_resource->setWaitBarrier([this](CWpFifoV1* r) {
        if (!m_surface) {
            r->error(WP_FIFO_V1_ERROR_SURFACE_DESTROYED, "Surface was gone");
            return;
        }

        if (!m_pending.barrierSet)
            return;

        m_pending.surfaceLocked = true;
    });

    m_listeners.surfacePrecommit = m_surface->m_events.precommit.listen([this]() {
        m_current = m_pending;

        if (!m_current.surfaceLocked)
            return;

        m_current.lock = CSurfaceScopeLock::create(m_surface->m_pending.lock);
    });
}

CFifoResource::~CFifoResource() {
    ;
}

bool CFifoResource::good() {
    return m_resource->resource();
}

void CFifoResource::presented() {
    // reset barrier if the app did setBarrier then we had a presentation and then
    // it did wait because it's already past
    if (m_current.barrierSet && !m_current.surfaceLocked)
        m_current.barrierSet = false;

    if (m_current.surfaceLocked && m_surface) {
        m_current.lock.reset();
        m_current.surfaceLocked = false;
        m_current.barrierSet    = false;
    }
}

CFifoManagerResource::CFifoManagerResource(UP<CWpFifoManagerV1>&& resource_) : m_resource(std::move(resource_)) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setDestroy([this](CWpFifoManagerV1* r) { PROTO::fifo->destroyResource(this); });
    m_resource->setOnDestroy([this](CWpFifoManagerV1* r) { PROTO::fifo->destroyResource(this); });

    m_resource->setGetFifo([](CWpFifoManagerV1* r, uint32_t id, wl_resource* surfResource) {
        if (!surfResource) {
            r->error(-1, "No resource for fifo");
            return;
        }

        auto surf = CWLSurfaceResource::fromResource(surfResource);

        if (!surf) {
            r->error(-1, "No surface for fifo");
            return;
        }

        for (const auto& fifo : PROTO::fifo->m_fifos) {
            if (fifo->m_surface == surf) {
                r->error(WP_FIFO_MANAGER_V1_ERROR_ALREADY_EXISTS, "Surface already has a fifo");
                return;
            }
        }

        const auto CLIENT   = r->client();
        const auto RESOURCE = PROTO::fifo->m_fifos.emplace_back(makeUnique<CFifoResource>(makeUnique<CWpFifoV1>(CLIENT, r->version(), id), surf)).get();

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::fifo->m_fifos.pop_back();
            return;
        }
    });
}

CFifoManagerResource::~CFifoManagerResource() {
    ;
}

bool CFifoManagerResource::good() {
    return m_resource->resource();
}

CFifoProtocol::CFifoProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P = g_pHookSystem->hookDynamic("monitorAdded", [this](void* self, SCallbackInfo& info, std::any param) {
        auto M = std::any_cast<PHLMONITOR>(param);

        M->m_events.presented.listenStatic([this, m = PHLMONITORREF{M}]() {
            if (!m || !PROTO::fifo)
                return;

            onMonitorPresent(m.lock());
        });
    });
}

void CFifoProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CFifoManagerResource>(makeUnique<CWpFifoManagerV1>(client, ver, id))).get();

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }
}

void CFifoProtocol::destroyResource(CFifoManagerResource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == res; });
}

void CFifoProtocol::destroyResource(CFifoResource* res) {
    std::erase_if(m_fifos, [&](const auto& other) { return other.get() == res; });
}

void CFifoProtocol::onMonitorPresent(PHLMONITOR m) {
    for (const auto& fifo : m_fifos) {
        if (!fifo->m_surface)
            continue;

        // Signal all surfaces
        // TODO: should we wait for the correct monitor(s)?

        fifo->presented();
    }
}
