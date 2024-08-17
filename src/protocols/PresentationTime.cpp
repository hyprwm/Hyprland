#include "PresentationTime.hpp"
#include <algorithm>
#include "../helpers/Monitor.hpp"
#include "../managers/HookSystemManager.hpp"
#include "core/Compositor.hpp"
#include "core/Output.hpp"
#include <aquamarine/output/Output.hpp>

CQueuedPresentationData::CQueuedPresentationData(SP<CWLSurfaceResource> surf) : surface(surf) {
    ;
}

void CQueuedPresentationData::setPresentationType(bool zeroCopy_) {
    zeroCopy = zeroCopy_;
}

void CQueuedPresentationData::attachMonitor(CMonitor* pMonitor_) {
    pMonitor = pMonitor_;
}

void CQueuedPresentationData::presented() {
    wasPresented = true;
}

void CQueuedPresentationData::discarded() {
    wasPresented = false;
}

CPresentationFeedback::CPresentationFeedback(SP<CWpPresentationFeedback> resource_, SP<CWLSurfaceResource> surf) : resource(resource_), surface(surf) {
    if (!good())
        return;

    resource->setOnDestroy([this](CWpPresentationFeedback* pMgr) {
        if (!done) // if it's done, it's probably already destroyed. If not, it will be in a sec.
            PROTO::presentation->destroyResource(this);
    });
}

bool CPresentationFeedback::good() {
    return resource->resource();
}

void CPresentationFeedback::sendQueued(SP<CQueuedPresentationData> data, timespec* when, uint32_t untilRefreshNs, uint64_t seq, uint32_t reportedFlags) {
    auto client = resource->client();

    if (PROTO::outputs.contains(data->pMonitor->szName)) {
        if (auto outputResource = PROTO::outputs.at(data->pMonitor->szName)->outputResourceFrom(client); outputResource)
            resource->sendSyncOutput(outputResource->getResource()->resource());
    }

    uint32_t flags = 0;
    if (!data->pMonitor->tearingState.activelyTearing)
        flags |= WP_PRESENTATION_FEEDBACK_KIND_VSYNC;
    if (data->zeroCopy)
        flags |= WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY;
    if (reportedFlags & Aquamarine::IOutput::AQ_OUTPUT_PRESENT_HW_CLOCK)
        flags |= WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK;
    if (reportedFlags & Aquamarine::IOutput::AQ_OUTPUT_PRESENT_HW_COMPLETION)
        flags |= WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION;

    if (data->wasPresented)
        resource->sendPresented((uint32_t)(when->tv_sec >> 32), (uint32_t)(when->tv_sec & 0xFFFFFFFF), (uint32_t)(when->tv_nsec), untilRefreshNs, (uint32_t)(seq >> 32),
                                (uint32_t)(seq & 0xFFFFFFFF), (wpPresentationFeedbackKind)flags);
    else
        resource->sendDiscarded();
}

CPresentationProtocol::CPresentationProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P = g_pHookSystem->hookDynamic("monitorRemoved", [this](void* self, SCallbackInfo& info, std::any param) {
        const auto PMONITOR = std::any_cast<CMonitor*>(param);
        std::erase_if(m_vQueue, [PMONITOR](const auto& other) { return !other->surface || other->pMonitor == PMONITOR; });
    });
}

void CPresentationProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CWpPresentation>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CWpPresentation* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CWpPresentation* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setFeedback([this](CWpPresentation* pMgr, wl_resource* surf, uint32_t id) { this->onGetFeedback(pMgr, surf, id); });
}

void CPresentationProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CPresentationProtocol::destroyResource(CPresentationFeedback* feedback) {
    std::erase_if(m_vFeedbacks, [&](const auto& other) { return other.get() == feedback; });
}

void CPresentationProtocol::onGetFeedback(CWpPresentation* pMgr, wl_resource* surf, uint32_t id) {
    const auto CLIENT = pMgr->client();
    const auto RESOURCE =
        m_vFeedbacks.emplace_back(makeShared<CPresentationFeedback>(makeShared<CWpPresentationFeedback>(CLIENT, pMgr->version(), id), CWLSurfaceResource::fromResource(surf)))
            .get();

    if (!RESOURCE->good()) {
        pMgr->noMemory();
        m_vFeedbacks.pop_back();
        return;
    }
}

void CPresentationProtocol::onPresented(CMonitor* pMonitor, timespec* when, uint32_t untilRefreshNs, uint64_t seq, uint32_t reportedFlags) {
    timespec  now;
    timespec* presentedAt = when;
    if (!presentedAt) {
        // just put the current time, we don't have anything better
        clock_gettime(CLOCK_MONOTONIC, &now);
        when = &now;
    }

    for (auto& feedback : m_vFeedbacks) {
        if (!feedback->surface)
            continue;

        for (auto& data : m_vQueue) {
            if (!data->surface || data->surface != feedback->surface)
                continue;

            feedback->sendQueued(data, when, untilRefreshNs, seq, reportedFlags);
            feedback->done = true;
            break;
        }
    }

    std::erase_if(m_vFeedbacks, [](const auto& other) { return !other->surface || other->done; });
    std::erase_if(m_vQueue, [pMonitor](const auto& other) { return !other->surface || other->pMonitor == pMonitor || !other->pMonitor; });
}

void CPresentationProtocol::queueData(SP<CQueuedPresentationData> data) {
    m_vQueue.emplace_back(data);
}
