#include "PresentationTime.hpp"
#include <algorithm>
#include "../helpers/Monitor.hpp"
#include "../managers/HookSystemManager.hpp"
#include "core/Compositor.hpp"
#include "core/Output.hpp"
#include <aquamarine/output/Output.hpp>

CQueuedPresentationData::CQueuedPresentationData(SP<CWLSurfaceResource> surf) : m_surface(surf) {
    ;
}

void CQueuedPresentationData::setPresentationType(bool zeroCopy_) {
    m_zeroCopy = zeroCopy_;
}

void CQueuedPresentationData::attachMonitor(PHLMONITOR pMonitor_) {
    m_monitor = pMonitor_;
}

void CQueuedPresentationData::presented() {
    m_wasPresented = true;
}

void CQueuedPresentationData::discarded() {
    m_wasPresented = false;
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

void CPresentationFeedback::sendQueued(WP<CQueuedPresentationData> data, const Time::steady_tp& when, uint32_t untilRefreshNs, uint64_t seq, uint32_t reportedFlags) {
    auto client = m_resource->client();

    if LIKELY (PROTO::outputs.contains(data->m_monitor->m_name)) {
        if LIKELY (auto outputResource = PROTO::outputs.at(data->m_monitor->m_name)->outputResourceFrom(client); outputResource)
            m_resource->sendSyncOutput(outputResource->getResource()->resource());
    }

    uint32_t flags = 0;
    if (!data->m_monitor->m_tearingState.activelyTearing)
        flags |= WP_PRESENTATION_FEEDBACK_KIND_VSYNC;
    if (data->m_zeroCopy)
        flags |= WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY;
    if (reportedFlags & Aquamarine::IOutput::AQ_OUTPUT_PRESENT_HW_CLOCK)
        flags |= WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK;
    if (reportedFlags & Aquamarine::IOutput::AQ_OUTPUT_PRESENT_HW_COMPLETION)
        flags |= WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION;

    const auto TIMESPEC = Time::toTimespec(when);

    time_t     tv_sec = 0;
    if (sizeof(time_t) > 4)
        tv_sec = TIMESPEC.tv_sec >> 32;

    uint32_t refreshNs = m_resource->version() == 1 && data->m_monitor->m_vrrActive ? 0 : untilRefreshNs;

    if (data->m_wasPresented)
        m_resource->sendPresented(sc<uint32_t>(tv_sec), sc<uint32_t>(TIMESPEC.tv_sec & 0xFFFFFFFF), sc<uint32_t>(TIMESPEC.tv_nsec), refreshNs, sc<uint32_t>(seq >> 32),
                                  sc<uint32_t>(seq & 0xFFFFFFFF), sc<wpPresentationFeedbackKind>(flags));
    else
        m_resource->sendDiscarded();

    m_done = true;
}

CPresentationProtocol::CPresentationProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P = g_pHookSystem->hookDynamic("monitorRemoved", [this](void* self, SCallbackInfo& info, std::any param) {
        const auto PMONITOR = PHLMONITORREF{std::any_cast<PHLMONITOR>(param)};
        std::erase_if(m_queue, [PMONITOR](const auto& other) { return !other->m_surface || other->m_monitor == PMONITOR; });
    });
}

void CPresentationProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CWpPresentation>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CWpPresentation* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CWpPresentation* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setFeedback([this](CWpPresentation* pMgr, wl_resource* surf, uint32_t id) { this->onGetFeedback(pMgr, surf, id); });
}

void CPresentationProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CPresentationProtocol::destroyResource(CPresentationFeedback* feedback) {
    std::erase_if(m_feedbacks, [&](const auto& other) { return other.get() == feedback; });
}

void CPresentationProtocol::onGetFeedback(CWpPresentation* pMgr, wl_resource* surf, uint32_t id) {
    const auto  CLIENT = pMgr->client();
    const auto& RESOURCE =
        m_feedbacks.emplace_back(makeUnique<CPresentationFeedback>(makeUnique<CWpPresentationFeedback>(CLIENT, pMgr->version(), id), CWLSurfaceResource::fromResource(surf))).get();

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_feedbacks.pop_back();
        return;
    }
}

void CPresentationProtocol::onPresented(PHLMONITOR pMonitor, const Time::steady_tp& when, uint32_t untilRefreshNs, uint64_t seq, uint32_t reportedFlags) {
    for (auto const& feedback : m_feedbacks) {
        if (!feedback->m_surface)
            continue;

        for (auto const& data : m_queue) {
            if (!data->m_surface || data->m_surface != feedback->m_surface || (data->m_monitor && data->m_monitor != pMonitor))
                continue;

            feedback->sendQueued(data, when, untilRefreshNs, seq, reportedFlags);
            feedback->m_done = true;
            break;
        }
    }

    if (m_feedbacks.size() > 10000) {
        LOGM(ERR, "FIXME: presentation has a feedback leak, and has grown to {} pending entries!!! Dropping!!!!!", m_feedbacks.size());

        // Move the elements from the 9000th position to the end of the vector.
        std::vector<UP<CPresentationFeedback>> newFeedbacks;
        newFeedbacks.reserve(m_feedbacks.size() - 9000);

        for (auto it = m_feedbacks.begin() + 9000; it != m_feedbacks.end(); ++it) {
            newFeedbacks.push_back(std::move(*it));
        }

        m_feedbacks = std::move(newFeedbacks);
    }

    std::erase_if(m_feedbacks, [](const auto& other) { return !other->m_surface || other->m_done; });
    std::erase_if(m_queue, [pMonitor](const auto& other) { return !other->m_surface || other->m_monitor == pMonitor || !other->m_monitor || other->m_done; });
}

void CPresentationProtocol::queueData(UP<CQueuedPresentationData>&& data) {
    m_queue.emplace_back(std::move(data));
}
