#include "CommitTiming.hpp"
#include "core/Compositor.hpp"
#include "../output/Monitor.hpp"
#include "../event/EventBus.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../managers/eventLoop/EventLoopTimer.hpp"
#include <algorithm>

CCommitTimerResource::CCommitTimerResource(UP<CWpCommitTimerV1>&& resource_, SP<CWLSurfaceResource> surface) : m_resource(std::move(resource_)), m_surface(surface) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setData(this);
    m_resource->setDestroy([this](CWpCommitTimerV1* r) { PROTO::commitTiming->destroyResource(this); });
    m_resource->setOnDestroy([this](CWpCommitTimerV1* r) { PROTO::commitTiming->destroyResource(this); });

    m_resource->setSetTimestamp([this](CWpCommitTimerV1* r, uint32_t tvHi, uint32_t tvLo, uint32_t tvNsec) {
        if (!m_surface) {
            r->error(WP_COMMIT_TIMER_V1_ERROR_SURFACE_DESTROYED, "Surface was gone");
            return;
        }

        if (m_surface->m_pending.pendingTimeout.has_value()) {
            r->error(WP_COMMIT_TIMER_V1_ERROR_TIMESTAMP_EXISTS, "Timestamp is already set");
            return;
        }

        const auto delay = Time::till({.tv_sec = (((uint64_t)tvHi) << 32) | (uint64_t)tvLo, .tv_nsec = tvNsec});

        if (delay.count() <= 0) {
            m_surface->m_pending.pendingTimeout.reset();
        } else
            m_surface->m_pending.pendingTimeout = delay;
    });

    m_listeners.surfaceStateCommit = m_surface->m_events.stateCommit2.listen([this](auto state) {
        if (!state || !state->pendingTimeout.has_value() || !m_surface || m_surface->isTearing())
            return;

        m_surface->m_stateQueue.lock(state, LOCK_REASON_TIMER);

        // record the absolute target so onMonitorPresent can release the lock on the first vblank at or after
        // it. Otherwise the wall clock timer releases it at the target, missing that vblank and delaying
        // presentation by one refresh cycle.
        std::erase_if(m_pendingTimedStates, [](const WP<SSurfaceState>& ws) { return !ws; });
        state->commitTimingTarget = Time::steadyNow() + *state->pendingTimeout;
        m_pendingTimedStates.emplace_back(state);

        if (!state->timer) {
            state->timer = makeShared<CEventLoopTimer>(
                state->pendingTimeout,
                [surface = m_surface, state](SP<CEventLoopTimer> self, void* data) {
                    if (!surface || !state)
                        return;

                    surface->m_stateQueue.unlock(state, LOCK_REASON_TIMER);
                },
                nullptr);
            g_pEventLoopManager->addTimer(state->timer);
        } else
            state->timer->updateTimeout(state->pendingTimeout);

        state->pendingTimeout.reset();
    });
}

void CCommitTimerResource::releaseDueStates(const Time::steady_tp& upcomingFlip) {
    if (!m_surface)
        return;

    std::erase_if(m_pendingTimedStates, [this, &upcomingFlip](const WP<SSurfaceState>& ws) {
        if (!ws)
            return true;

        if (!ws->commitTimingTarget.has_value() || *ws->commitTimingTarget > upcomingFlip)
            return false; // not due yet, keep waiting

        m_surface->m_stateQueue.unlock(ws, LOCK_REASON_TIMER);
        return true;
    });
}

bool CCommitTimerResource::good() {
    return m_resource->resource();
}

CCommitTimingManagerResource::CCommitTimingManagerResource(UP<CWpCommitTimingManagerV1>&& resource_) : m_resource(std::move(resource_)) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setData(this);
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

        if (surf->m_commitTimer) {
            r->error(WP_COMMIT_TIMING_MANAGER_V1_ERROR_COMMIT_TIMER_EXISTS, "Surface already has a commit timing");
            return;
        }

        const auto& RESOURCE = PROTO::commitTiming->m_timers.emplace_back(makeUnique<CCommitTimerResource>(makeUnique<CWpCommitTimerV1>(r->client(), r->version(), id), surf));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::commitTiming->m_timers.pop_back();
            return;
        }

        surf->m_commitTimer = RESOURCE;
    });
}

CCommitTimingManagerResource::~CCommitTimingManagerResource() {
    ;
}

bool CCommitTimingManagerResource::good() {
    return m_resource->resource();
}

CCommitTimingProtocol::CCommitTimingProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P = Event::bus()->m_events.monitor.added.listen([this](PHLMONITOR M) {
        M->m_events.presented.listenStatic([this, m = PHLMONITORREF{M}](const Time::steady_tp& presentTime) {
            if (!m || !PROTO::commitTiming)
                return;

            onMonitorPresent(m.lock(), presentTime);
        });
    });
}

void CCommitTimingProtocol::onMonitorPresent(PHLMONITOR m, const Time::steady_tp& presentTime) {
    if (!m || m->m_refreshRate <= 0.F)
        return;

    const auto UPCOMING_FLIP = presentTime + std::chrono::nanoseconds(static_cast<int64_t>(1'000'000'000.0 / m->m_refreshRate));
    for (const auto& timer : m_timers) {
        if (!timer->m_surface)
            continue;

        const auto& OUTPUTS = timer->m_surface->m_enteredOutputs;
        if (std::ranges::none_of(OUTPUTS, [&m](const auto& mon) { return mon == m; }))
            continue;

        timer->releaseDueStates(UPCOMING_FLIP);
    }
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
