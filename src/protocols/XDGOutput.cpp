#include "XDGOutput.hpp"
#include "../config/ConfigValue.hpp"
#include "../helpers/Monitor.hpp"
#include "../xwayland/XWayland.hpp"
#include "../managers/HookSystemManager.hpp"
#include "core/Output.hpp"

#define OUTPUT_MANAGER_VERSION                   3
#define OUTPUT_DONE_DEPRECATED_SINCE_VERSION     3
#define OUTPUT_DESCRIPTION_MUTABLE_SINCE_VERSION 3
#define OUTPUT_NAME_SINCE_VERSION                2
#define OUTPUT_DESCRIPTION_SINCE_VERSION         2

//

void CXDGOutputProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managerResources, [&](const auto& other) { return other->resource() == res; });
}

void CXDGOutputProtocol::onOutputResourceDestroy(wl_resource* res) {
    std::erase_if(m_xdgOutputs, [&](const auto& other) { return other->m_resource->resource() == res; });
}

void CXDGOutputProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managerResources.emplace_back(makeUnique<CZxdgOutputManagerV1>(client, ver, id)).get();

    if UNLIKELY (!RESOURCE->resource()) {
        LOGM(LOG, "Couldn't bind XDGOutputMgr");
        wl_client_post_no_memory(client);
        return;
    }

    RESOURCE->setDestroy([this](CZxdgOutputManagerV1* res) { onManagerResourceDestroy(res->resource()); });
    RESOURCE->setOnDestroy([this](CZxdgOutputManagerV1* res) { onManagerResourceDestroy(res->resource()); });
    RESOURCE->setGetXdgOutput([this](CZxdgOutputManagerV1* mgr, uint32_t id, wl_resource* output) { onManagerGetXDGOutput(mgr, id, output); });
}

CXDGOutputProtocol::CXDGOutputProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P  = g_pHookSystem->hookDynamic("monitorLayoutChanged", [this](void* self, SCallbackInfo& info, std::any param) { this->updateAllOutputs(); });
    static auto P2 = g_pHookSystem->hookDynamic("configReloaded", [this](void* self, SCallbackInfo& info, std::any param) { this->updateAllOutputs(); });
}

void CXDGOutputProtocol::onManagerGetXDGOutput(CZxdgOutputManagerV1* mgr, uint32_t id, wl_resource* outputResource) {
    const auto  OUTPUT   = CWLOutputResource::fromResource(outputResource);
    const auto  PMONITOR = OUTPUT->m_monitor.lock();
    const auto  CLIENT   = mgr->client();

    CXDGOutput* pXDGOutput = m_xdgOutputs.emplace_back(makeUnique<CXDGOutput>(makeShared<CZxdgOutputV1>(CLIENT, mgr->version(), id), PMONITOR)).get();
#ifndef NO_XWAYLAND
    if (g_pXWayland && g_pXWayland->m_server && g_pXWayland->m_server->m_xwaylandClient == CLIENT)
        pXDGOutput->m_isXWayland = true;
#endif
    pXDGOutput->m_client = CLIENT;

    pXDGOutput->m_outputProto = OUTPUT->m_owner;

    if UNLIKELY (!pXDGOutput->m_resource->resource()) {
        m_xdgOutputs.pop_back();
        mgr->noMemory();
        return;
    }

    if UNLIKELY (!PMONITOR) {
        LOGM(ERR, "New xdg_output from client {:x} ({}) has no CMonitor?!", (uintptr_t)CLIENT, pXDGOutput->m_isXWayland ? "xwayland" : "not xwayland");
        return;
    }

    LOGM(LOG, "New xdg_output for {}: client {:x} ({})", PMONITOR->m_name, (uintptr_t)CLIENT, pXDGOutput->m_isXWayland ? "xwayland" : "not xwayland");

    const auto XDGVER = pXDGOutput->m_resource->version();

    if (XDGVER >= OUTPUT_NAME_SINCE_VERSION)
        pXDGOutput->m_resource->sendName(PMONITOR->m_name.c_str());
    if (XDGVER >= OUTPUT_DESCRIPTION_SINCE_VERSION && !PMONITOR->m_output->description.empty())
        pXDGOutput->m_resource->sendDescription(PMONITOR->m_output->description.c_str());

    pXDGOutput->sendDetails();

    const auto OUTPUTVER = wl_resource_get_version(outputResource);
    if (OUTPUTVER >= WL_OUTPUT_DONE_SINCE_VERSION && XDGVER >= OUTPUT_DONE_DEPRECATED_SINCE_VERSION)
        wl_output_send_done(outputResource);
}

void CXDGOutputProtocol::updateAllOutputs() {
    LOGM(LOG, "updating all xdg_output heads");

    for (auto const& o : m_xdgOutputs) {
        if (!o->m_monitor)
            continue;

        o->sendDetails();

        o->m_monitor->scheduleDone();
    }
}

//

CXDGOutput::CXDGOutput(SP<CZxdgOutputV1> resource_, PHLMONITOR monitor_) : m_monitor(monitor_), m_resource(resource_) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setDestroy([](CZxdgOutputV1* pMgr) { PROTO::xdgOutput->onOutputResourceDestroy(pMgr->resource()); });
    m_resource->setOnDestroy([](CZxdgOutputV1* pMgr) { PROTO::xdgOutput->onOutputResourceDestroy(pMgr->resource()); });
}

void CXDGOutput::sendDetails() {
    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    if UNLIKELY (!m_monitor || !m_outputProto || m_outputProto->isDefunct())
        return;

    const auto POS = m_isXWayland ? m_monitor->m_xwaylandPosition : m_monitor->m_position;
    m_resource->sendLogicalPosition(POS.x, POS.y);

    if (*PXWLFORCESCALEZERO && m_isXWayland)
        m_resource->sendLogicalSize(m_monitor->m_transformedSize.x, m_monitor->m_transformedSize.y);
    else
        m_resource->sendLogicalSize(m_monitor->m_size.x, m_monitor->m_size.y);

    if (m_resource->version() < OUTPUT_DONE_DEPRECATED_SINCE_VERSION)
        m_resource->sendDone();
}
