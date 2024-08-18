#include "OutputPower.hpp"
#include "../Compositor.hpp"
#include "core/Output.hpp"

COutputPower::COutputPower(SP<CZwlrOutputPowerV1> resource_, CMonitor* pMonitor_) : resource(resource_), pMonitor(pMonitor_) {
    if (!resource->resource())
        return;

    resource->setDestroy([this](CZwlrOutputPowerV1* r) { PROTO::outputPower->destroyOutputPower(this); });
    resource->setOnDestroy([this](CZwlrOutputPowerV1* r) { PROTO::outputPower->destroyOutputPower(this); });

    resource->setSetMode([this](CZwlrOutputPowerV1* r, zwlrOutputPowerV1Mode mode) {
        if (!pMonitor)
            return;

        pMonitor->dpmsStatus = mode == ZWLR_OUTPUT_POWER_V1_MODE_ON;

        pMonitor->output->state->setEnabled(mode == ZWLR_OUTPUT_POWER_V1_MODE_ON);

        if (!pMonitor->state.commit())
            LOGM(ERR, "Couldn't set dpms to {} for {}", pMonitor->dpmsStatus, pMonitor->szName);
    });

    resource->sendMode(pMonitor->dpmsStatus ? ZWLR_OUTPUT_POWER_V1_MODE_ON : ZWLR_OUTPUT_POWER_V1_MODE_OFF);

    listeners.monitorDestroy = pMonitor->events.destroy.registerListener([this](std::any v) {
        pMonitor = nullptr;
        resource->sendFailed();
    });

    listeners.monitorDpms = pMonitor->events.dpmsChanged.registerListener(
        [this](std::any v) { resource->sendMode(pMonitor->dpmsStatus ? ZWLR_OUTPUT_POWER_V1_MODE_ON : ZWLR_OUTPUT_POWER_V1_MODE_OFF); });
    listeners.monitorState = pMonitor->events.modeChanged.registerListener(
        [this](std::any v) { resource->sendMode(pMonitor->dpmsStatus ? ZWLR_OUTPUT_POWER_V1_MODE_ON : ZWLR_OUTPUT_POWER_V1_MODE_OFF); });
}

bool COutputPower::good() {
    return resource->resource();
}

COutputPowerProtocol::COutputPowerProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void COutputPowerProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CZwlrOutputPowerManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwlrOutputPowerManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwlrOutputPowerManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetOutputPower([this](CZwlrOutputPowerManagerV1* hiThereFriend, uint32_t id, wl_resource* output) { this->onGetOutputPower(hiThereFriend, id, output); });
}

void COutputPowerProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void COutputPowerProtocol::destroyOutputPower(COutputPower* power) {
    std::erase_if(m_vOutputPowers, [&](const auto& other) { return other.get() == power; });
}

void COutputPowerProtocol::onGetOutputPower(CZwlrOutputPowerManagerV1* pMgr, uint32_t id, wl_resource* output) {

    const auto OUTPUT = CWLOutputResource::fromResource(output);

    if (!OUTPUT) {
        pMgr->error(0, "Invalid output resource");
        return;
    }

    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_vOutputPowers.emplace_back(std::make_unique<COutputPower>(makeShared<CZwlrOutputPowerV1>(CLIENT, pMgr->version(), id), OUTPUT->monitor.get())).get();

    if (!RESOURCE->good()) {
        pMgr->noMemory();
        m_vOutputPowers.pop_back();
        return;
    }
}
