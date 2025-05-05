#include "OutputPower.hpp"
#include "core/Output.hpp"
#include "../helpers/Monitor.hpp"

COutputPower::COutputPower(SP<CZwlrOutputPowerV1> resource_, PHLMONITOR pMonitor_) : m_resource(resource_), m_monitor(pMonitor_) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setDestroy([this](CZwlrOutputPowerV1* r) { PROTO::outputPower->destroyOutputPower(this); });
    m_resource->setOnDestroy([this](CZwlrOutputPowerV1* r) { PROTO::outputPower->destroyOutputPower(this); });

    m_resource->setSetMode([this](CZwlrOutputPowerV1* r, zwlrOutputPowerV1Mode mode) {
        if (!m_monitor)
            return;

        m_monitor->m_dpmsStatus = mode == ZWLR_OUTPUT_POWER_V1_MODE_ON;

        m_monitor->m_output->state->setEnabled(mode == ZWLR_OUTPUT_POWER_V1_MODE_ON);

        if (!m_monitor->m_state.commit())
            LOGM(ERR, "Couldn't set dpms to {} for {}", m_monitor->m_dpmsStatus, m_monitor->m_name);
    });

    m_resource->sendMode(m_monitor->m_dpmsStatus ? ZWLR_OUTPUT_POWER_V1_MODE_ON : ZWLR_OUTPUT_POWER_V1_MODE_OFF);

    m_listeners.monitorDestroy = m_monitor->m_events.destroy.registerListener([this](std::any v) {
        m_monitor.reset();
        m_resource->sendFailed();
    });

    m_listeners.monitorDpms = m_monitor->m_events.dpmsChanged.registerListener(
        [this](std::any v) { m_resource->sendMode(m_monitor->m_dpmsStatus ? ZWLR_OUTPUT_POWER_V1_MODE_ON : ZWLR_OUTPUT_POWER_V1_MODE_OFF); });
    m_listeners.monitorState = m_monitor->m_events.modeChanged.registerListener(
        [this](std::any v) { m_resource->sendMode(m_monitor->m_dpmsStatus ? ZWLR_OUTPUT_POWER_V1_MODE_ON : ZWLR_OUTPUT_POWER_V1_MODE_OFF); });
}

bool COutputPower::good() {
    return m_resource->resource();
}

COutputPowerProtocol::COutputPowerProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void COutputPowerProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CZwlrOutputPowerManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwlrOutputPowerManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwlrOutputPowerManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetOutputPower([this](CZwlrOutputPowerManagerV1* hiThereFriend, uint32_t id, wl_resource* output) { this->onGetOutputPower(hiThereFriend, id, output); });
}

void COutputPowerProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void COutputPowerProtocol::destroyOutputPower(COutputPower* power) {
    std::erase_if(m_outputPowers, [&](const auto& other) { return other.get() == power; });
}

void COutputPowerProtocol::onGetOutputPower(CZwlrOutputPowerManagerV1* pMgr, uint32_t id, wl_resource* output) {

    const auto OUTPUT = CWLOutputResource::fromResource(output);

    if UNLIKELY (!OUTPUT) {
        pMgr->error(0, "Invalid output resource");
        return;
    }

    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_outputPowers.emplace_back(makeUnique<COutputPower>(makeShared<CZwlrOutputPowerV1>(CLIENT, pMgr->version(), id), OUTPUT->m_monitor.lock())).get();

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_outputPowers.pop_back();
        return;
    }
}
