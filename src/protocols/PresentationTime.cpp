#include "PresentationTime.hpp"
#include <algorithm>
#include <ranges>
#include "../output/Monitor.hpp"
#include "../event/EventBus.hpp"
#include "core/Compositor.hpp"
#include "core/Output.hpp"
#include <aquamarine/output/Output.hpp>

CQueuedPresentationData::CQueuedPresentationData(SP<CWLSurfaceResource> surf, uint64_t commitSeq) : m_commitSeq(commitSeq), m_surface(surf) {
    ;
}

void CQueuedPresentationData::setPresentationType(bool zeroCopy_) {
    m_zeroCopy = zeroCopy_;
}

void CQueuedPresentationData::attachMonitor(PHLMONITOR pMonitor_) {
    if (!pMonitor_)
        return;

    if (std::ranges::contains(m_monitors, pMonitor_))
        return;

    m_monitors.emplace_back(pMonitor_);
}

void CQueuedPresentationData::addFeedbacks(std::vector<WP<CPresentationFeedback>>&& feedbacks) {
    m_feedbacks.insert(m_feedbacks.end(), std::make_move_iterator(feedbacks.begin()), std::make_move_iterator(feedbacks.end()));
    std::erase_if(m_feedbacks, [](const auto& feedback) { return !feedback; });
    feedbacks.clear();
}

void CQueuedPresentationData::presented() {
    m_wasPresented = true;
}

void CQueuedPresentationData::discarded() {
    m_wasPresented = false;
}

bool CQueuedPresentationData::hasMonitor(PHLMONITOR pMonitor) const {
    return std::ranges::contains(m_monitors, pMonitor);
}

void CQueuedPresentationData::detachMonitor(PHLMONITOR pMonitor) {
    std::erase_if(m_monitors, [pMonitor](const auto& monitor) { return !monitor || monitor == pMonitor; });
}

bool CQueuedPresentationData::hasMonitors() const {
    return std::ranges::any_of(m_monitors, [](const auto& monitor) { return !!monitor; });
}

CPresentationFeedback::CPresentationFeedback(UP<CWpPresentationFeedback>&& resource_, SP<CWLSurfaceResource> surf) : m_resource(std::move(resource_)), m_surface(surf) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CWpPresentationFeedback* pMgr) {
        if (!m_done) // if it's done, it's probably already destroyed. If not, it will be in a sec.
            PROTO::presentation->destroyResource(this);
    });
}

bool CPresentationFeedback::good() {
    return m_resource->resource();
}

void CPresentationFeedback::sendQueued(const CQueuedPresentationData& data, PHLMONITOR pMonitor, const timespec& when, uint32_t untilRefreshNs, uint64_t seq,
                                       uint32_t reportedFlags) {
    if (m_done || !good())
        return;

    auto client = m_resource->client();

    if (data.m_wasPresented) {
        for (const auto& monitor : data.m_monitors) {
            if (!monitor || !PROTO::outputs.contains(monitor->m_name))
                continue;

            if (auto outputResources = PROTO::outputs.at(monitor->m_name)->outputResourcesFrom(client); !outputResources.empty()) {
                for (const auto& r : outputResources) {
                    m_resource->sendSyncOutput(r->getResource()->resource());
                }
            }
        }
    }

    if (data.m_wasPresented && pMonitor) {
        uint32_t flags = 0;
        if (!pMonitor->m_tearingState.activelyTearing)
            flags |= WP_PRESENTATION_FEEDBACK_KIND_VSYNC;
        if (data.m_zeroCopy)
            flags |= WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY;
        if (reportedFlags & Aquamarine::IOutput::AQ_OUTPUT_PRESENT_HW_CLOCK)
            flags |= WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK;
        if (reportedFlags & Aquamarine::IOutput::AQ_OUTPUT_PRESENT_HW_COMPLETION)
            flags |= WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION;

        time_t tv_sec = 0;
        if (sizeof(time_t) > 4)
            tv_sec = when.tv_sec >> 32;

        uint32_t refreshNs = m_resource->version() == 1 && pMonitor->m_vrrActive && pMonitor->m_output->vrrCapable ? 0 : untilRefreshNs;

        m_resource->sendPresented(sc<uint32_t>(tv_sec), sc<uint32_t>(when.tv_sec & 0xFFFFFFFF), sc<uint32_t>(when.tv_nsec), refreshNs, sc<uint32_t>(seq >> 32),
                                  sc<uint32_t>(seq & 0xFFFFFFFF), sc<wpPresentationFeedbackKind>(flags));
    } else
        sendDiscarded();

    m_done = true;
}

void CPresentationFeedback::sendDiscarded() {
    if (m_done || !good())
        return;

    m_resource->sendDiscarded();
    m_done = true;
}

CPresentationProtocol::CPresentationProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P = Event::bus()->m_events.monitor.removed.listen([this](PHLMONITOR mon) { discardPresentedForMonitor(mon); });
}

void CPresentationProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CWpPresentation>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CWpPresentation* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CWpPresentation* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setFeedback([this](CWpPresentation* pMgr, wl_resource* surf, uint32_t id) { this->onGetFeedback(pMgr, surf, id); });
    RESOURCE->sendClockId(CLOCK_MONOTONIC);
}

void CPresentationProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CPresentationProtocol::destroyResource(CPresentationFeedback* feedback) {
    std::erase_if(m_feedbacks, [&](const auto& other) { return other.get() == feedback; });
}

void CPresentationProtocol::onGetFeedback(CWpPresentation* pMgr, wl_resource* surf, uint32_t id) {
    const auto  CLIENT   = pMgr->client();
    const auto  SURFACE  = CWLSurfaceResource::fromResource(surf);
    const auto& RESOURCE = m_feedbacks.emplace_back(makeUnique<CPresentationFeedback>(makeUnique<CWpPresentationFeedback>(CLIENT, pMgr->version(), id), SURFACE)).get();

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_feedbacks.pop_back();
        return;
    }

    if (SURFACE)
        SURFACE->queuePresentationFeedback(m_feedbacks.back());
    else
        RESOURCE->sendDiscarded();
}

void CPresentationProtocol::onPresented(PHLMONITOR pMonitor, const timespec& when, uint32_t untilRefreshNs, uint64_t seq, uint32_t reportedFlags) {
    for (auto const& data : m_queue) {
        if (!data->m_surface || !data->hasMonitor(pMonitor))
            continue;

        for (auto const& feedback : data->m_feedbacks) {
            if (!feedback)
                continue;

            feedback->sendQueued(*data, pMonitor, when, untilRefreshNs, seq, reportedFlags);
        }

        data->m_done = true;
    }

    if (m_feedbacks.size() > 10000) {
        LOGM(Log::ERR, "FIXME: presentation has a feedback leak, and has grown to {} pending entries!!! Dropping!!!!!", m_feedbacks.size());

        // Move the elements from the 9000th position to the end of the vector.
        std::vector<UP<CPresentationFeedback>> newFeedbacks;
        newFeedbacks.reserve(m_feedbacks.size() - 9000);

        for (auto it = m_feedbacks.begin() + 9000; it != m_feedbacks.end(); ++it) {
            newFeedbacks.push_back(std::move(*it));
        }

        m_feedbacks = std::move(newFeedbacks);
    }

    std::erase_if(m_feedbacks, [](const auto& other) { return !other->m_surface || other->m_done; });
    std::erase_if(m_queue, [](const auto& other) { return !other->m_surface || other->m_done; });
}

void CPresentationProtocol::queueData(UP<CQueuedPresentationData>&& data) {
    m_queue.emplace_back(std::move(data));
}

bool CPresentationProtocol::queueData(WP<CWLSurfaceResource> surf, uint64_t commitSeq, std::vector<WP<CPresentationFeedback>>&& feedbacks, PHLMONITOR pMonitor, bool zeroCopy) {
    std::erase_if(feedbacks, [](const auto& feedback) { return !feedback; });

    // A newer commit rendered for the same output supersedes an older commit
    // that has only been queued for that output's next presentation event.
    // This can happen when FIFO unlocks multiple surface states in one frame.
    for (const auto& data : m_queue) {
        if (!data->m_surface || data->m_surface != surf || data->m_commitSeq >= commitSeq || data->m_done || !data->hasMonitor(pMonitor))
            continue;

        data->detachMonitor(pMonitor);
        if (data->hasMonitors())
            continue;

        for (const auto& feedback : data->m_feedbacks) {
            if (feedback)
                feedback->sendDiscarded();
        }

        data->m_done = true;
    }

    std::erase_if(m_queue, [](const auto& data) { return !data->m_surface || data->m_done; });
    std::erase_if(m_feedbacks, [](const auto& feedback) { return !feedback->m_surface || feedback->m_done; });

    if (!surf || feedbacks.empty())
        return addPresentedMonitor(surf, commitSeq, pMonitor, zeroCopy);

    auto data = makeUnique<CQueuedPresentationData>(surf.lock(), commitSeq);
    data->addFeedbacks(std::move(feedbacks));
    data->presented();
    data->attachMonitor(pMonitor);
    data->setPresentationType(zeroCopy);
    queueData(std::move(data));
    return true;
}

bool CPresentationProtocol::addPresentedMonitor(WP<CWLSurfaceResource> surf, uint64_t commitSeq, PHLMONITOR pMonitor, bool zeroCopy) {
    for (const auto& data : m_queue) {
        if (!data->m_surface || data->m_surface != surf || data->m_commitSeq != commitSeq || data->m_done)
            continue;

        data->presented();
        data->attachMonitor(pMonitor);
        if (zeroCopy)
            data->setPresentationType(true);
        return true;
    }

    return false;
}

void CPresentationProtocol::discardFeedbacks(std::vector<WP<CPresentationFeedback>>& feedbacks) {
    for (const auto& feedback : feedbacks) {
        if (!feedback)
            continue;

        feedback->sendDiscarded();
    }

    feedbacks.clear();
    std::erase_if(m_feedbacks, [](const auto& other) { return !other->m_surface || other->m_done; });
}

void CPresentationProtocol::discardSurface(WP<CWLSurfaceResource> surf) {
    for (const auto& feedback : m_feedbacks) {
        if (!feedback->m_surface || feedback->m_surface != surf)
            continue;

        feedback->sendDiscarded();
    }

    std::erase_if(m_queue, [surf](const auto& data) { return !data->m_surface || data->m_surface == surf; });
    std::erase_if(m_feedbacks, [](const auto& other) { return !other->m_surface || other->m_done; });
}

void CPresentationProtocol::discardPresentedForMonitor(PHLMONITOR pMonitor) {
    for (const auto& data : m_queue) {
        if (!data->hasMonitor(pMonitor))
            continue;

        data->detachMonitor(pMonitor);
        if (data->hasMonitors())
            continue;

        for (const auto& feedback : data->m_feedbacks) {
            if (feedback)
                feedback->sendDiscarded();
        }

        data->m_done = true;
    }

    std::erase_if(m_queue, [](const auto& other) { return !other->m_surface || other->m_done; });
    std::erase_if(m_feedbacks, [](const auto& other) { return !other->m_surface || other->m_done; });
}

bool CPresentationProtocol::hasPendingFeedbacks() const {
    return !m_feedbacks.empty();
}
