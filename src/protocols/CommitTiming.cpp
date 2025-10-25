#include "CommitTiming.hpp"
#include "core/Compositor.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../managers/eventLoop/EventLoopTimer.hpp"

CCommitTimerResource::CCommitTimerResource(UP<CWpCommitTimerV1>&& resource_, SP<CWLSurfaceResource> surface) : m_resource(std::move(resource_)), m_surface(surface) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setDestroy([this](CWpCommitTimerV1* r) { PROTO::commitTiming->destroyResource(this); });
    m_resource->setOnDestroy([this](CWpCommitTimerV1* r) { PROTO::commitTiming->destroyResource(this); });

    m_resource->setSetTimestamp([this](CWpCommitTimerV1* r, uint32_t tvHi, uint32_t tvLo, uint32_t tvNsec) {
        if (!m_surface) {
            r->error(WP_COMMIT_TIMER_V1_ERROR_SURFACE_DESTROYED, "Surface was gone");
            return;
        }

        if (m_timerPresent) {
            r->error(WP_COMMIT_TIMER_V1_ERROR_TIMESTAMP_EXISTS, "Timestamp is already set");
            return;
        }

        timespec ts;
        ts.tv_sec  = (((uint64_t)tvHi) << 32) | (uint64_t)tvLo;
        ts.tv_nsec = tvNsec;

        const auto TIME     = Time::fromTimespec(&ts);
        const auto TIME_NOW = Time::steadyNow();

        if (TIME_NOW > TIME) {
            // TODO: should we err here?
            // for now just do nothing I guess, thats some lag.
            return;
        }

        m_timerPresent = true;

        // FIXME: this doesn't *exactly* guarantee we wont fire a few dozen ns before
        // the desired time...
        m_timers.emplace_back(makeShared<STimerLock>(STimerLock{
            .timer = makeShared<CEventLoopTimer>(
                TIME - TIME_NOW,
                [this](SP<CEventLoopTimer> self, void* data) {
                    // unlock the state if applicable
                    std::erase_if(m_timers, [self](const auto& e) { return e->timer == self; });
                },
                nullptr),
            .lock = CSurfaceScopeLock::create(m_surface->m_pending.lock),
        }));
    });

    m_listeners.surfacePrecommit = m_surface->m_events.precommit.listen([this] { m_timerPresent = false; });
}

CCommitTimerResource::~CCommitTimerResource() {
    ;
}

bool CCommitTimerResource::good() {
    return m_resource->resource();
}

CCommitTimingManagerResource::CCommitTimingManagerResource(UP<CWpCommitTimingManagerV1>&& resource_) : m_resource(std::move(resource_)) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setDestroy([this](CWpCommitTimingManagerV1* r) { PROTO::commitTiming->destroyResource(this); });
    m_resource->setOnDestroy([this](CWpCommitTimingManagerV1* r) { PROTO::commitTiming->destroyResource(this); });

    m_resource->setGetTimer([](CWpCommitTimingManagerV1* r, uint32_t id, wl_resource* surfResource) {
        if (!surfResource) {
            r->error(-1, "No resource for commit timing");
            return;
        }

        auto surf = CWLSurfaceResource::fromResource(surfResource);

        if (!surf) {
            r->error(-1, "No surface for commit timing");
            return;
        }

        for (const auto& timer : PROTO::commitTiming->m_timers) {
            if (timer->m_surface == surf) {
                r->error(WP_COMMIT_TIMING_MANAGER_V1_ERROR_COMMIT_TIMER_EXISTS, "Surface already has a commit timing");
                return;
            }
        }

        const auto CLIENT   = r->client();
        const auto RESOURCE = PROTO::commitTiming->m_timers.emplace_back(makeUnique<CCommitTimerResource>(makeUnique<CWpCommitTimerV1>(CLIENT, r->version(), id), surf)).get();

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::commitTiming->m_timers.pop_back();
            return;
        }

        RESOURCE->m_self = PROTO::commitTiming->m_timers.back();
    });
}

CCommitTimingManagerResource::~CCommitTimingManagerResource() {
    ;
}

bool CCommitTimingManagerResource::good() {
    return m_resource->resource();
}

CCommitTimingProtocol::CCommitTimingProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CCommitTimingProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CCommitTimingManagerResource>(makeUnique<CWpCommitTimingManagerV1>(client, ver, id))).get();

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }
}

void CCommitTimingProtocol::destroyResource(CCommitTimingManagerResource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == res; });
}

void CCommitTimingProtocol::destroyResource(CCommitTimerResource* res) {
    std::erase_if(m_timers, [&](const auto& other) { return other.get() == res; });
}
