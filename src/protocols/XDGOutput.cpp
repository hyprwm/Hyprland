#include "XDGOutput.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../xwayland/XWayland.hpp"
#include "core/Output.hpp"

#define OUTPUT_MANAGER_VERSION                   3
#define OUTPUT_DONE_DEPRECATED_SINCE_VERSION     3
#define OUTPUT_DESCRIPTION_MUTABLE_SINCE_VERSION 3
#define OUTPUT_NAME_SINCE_VERSION                2
#define OUTPUT_DESCRIPTION_SINCE_VERSION         2

//

void CXDGOutputProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagerResources, [&](const auto& other) { return other->resource() == res; });
}

void CXDGOutputProtocol::onOutputResourceDestroy(wl_resource* res) {
    std::erase_if(m_vXDGOutputs, [&](const auto& other) { return other->resource->resource() == res; });
}

void CXDGOutputProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagerResources.emplace_back(std::make_unique<CZxdgOutputManagerV1>(client, ver, id)).get();

    if (!RESOURCE->resource()) {
        LOGM(LOG, "Couldn't bind XDGOutputMgr");
        wl_client_post_no_memory(client);
        return;
    }

    RESOURCE->setDestroy([this](CZxdgOutputManagerV1* res) { this->onManagerResourceDestroy(res->resource()); });
    RESOURCE->setOnDestroy([this](CZxdgOutputManagerV1* res) { this->onManagerResourceDestroy(res->resource()); });
    RESOURCE->setGetXdgOutput([this](CZxdgOutputManagerV1* mgr, uint32_t id, wl_resource* output) { this->onManagerGetXDGOutput(mgr, id, output); });
}

CXDGOutputProtocol::CXDGOutputProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P  = g_pHookSystem->hookDynamic("monitorLayoutChanged", [this](void* self, SCallbackInfo& info, std::any param) { this->updateAllOutputs(); });
    static auto P2 = g_pHookSystem->hookDynamic("configReloaded", [this](void* self, SCallbackInfo& info, std::any param) { this->updateAllOutputs(); });
    static auto P3 = g_pHookSystem->hookDynamic("monitorRemoved", [this](void* self, SCallbackInfo& info, std::any param) {
        const auto PMONITOR = std::any_cast<CMonitor*>(param);
        for (auto& o : m_vXDGOutputs) {
            if (o->monitor == PMONITOR)
                o->monitor = nullptr;
        }
    });
}

void CXDGOutputProtocol::onManagerGetXDGOutput(CZxdgOutputManagerV1* mgr, uint32_t id, wl_resource* outputResource) {
    const auto  OUTPUT = CWLOutputResource::fromResource(outputResource);

    const auto  PMONITOR = OUTPUT->monitor.get();

    const auto  CLIENT = mgr->client();

    CXDGOutput* pXDGOutput = m_vXDGOutputs.emplace_back(std::make_unique<CXDGOutput>(makeShared<CZxdgOutputV1>(CLIENT, mgr->version(), id), PMONITOR)).get();
#ifndef NO_XWAYLAND
    if (g_pXWayland && g_pXWayland->pServer && g_pXWayland->pServer->xwaylandClient == CLIENT)
        pXDGOutput->isXWayland = true;
#endif
    pXDGOutput->client = CLIENT;

    if (!pXDGOutput->resource->resource()) {
        m_vXDGOutputs.pop_back();
        mgr->noMemory();
        return;
    }

    if (!PMONITOR)
        return;

    const auto XDGVER = pXDGOutput->resource->version();

    if (XDGVER >= OUTPUT_NAME_SINCE_VERSION)
        pXDGOutput->resource->sendName(PMONITOR->szName.c_str());
    if (XDGVER >= OUTPUT_DESCRIPTION_SINCE_VERSION && !PMONITOR->output->description.empty())
        pXDGOutput->resource->sendDescription(PMONITOR->output->description.c_str());

    pXDGOutput->sendDetails();

    const auto OUTPUTVER = wl_resource_get_version(outputResource);
    if (OUTPUTVER >= WL_OUTPUT_DONE_SINCE_VERSION && XDGVER >= OUTPUT_DONE_DEPRECATED_SINCE_VERSION)
        wl_output_send_done(outputResource);
}

void CXDGOutputProtocol::updateAllOutputs() {
    for (auto& o : m_vXDGOutputs) {

        if (!o->monitor)
            continue;

        o->sendDetails();

        o->monitor->scheduleDone();
    }
}

//

CXDGOutput::CXDGOutput(SP<CZxdgOutputV1> resource_, CMonitor* monitor_) : monitor(monitor_), resource(resource_) {
    if (!resource->resource())
        return;

    resource->setDestroy([](CZxdgOutputV1* pMgr) { PROTO::xdgOutput->onOutputResourceDestroy(pMgr->resource()); });
    resource->setOnDestroy([](CZxdgOutputV1* pMgr) { PROTO::xdgOutput->onOutputResourceDestroy(pMgr->resource()); });
}

void CXDGOutput::sendDetails() {
    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    if (!monitor)
        return;

    const auto POS = isXWayland ? monitor->vecXWaylandPosition : monitor->vecPosition;
    resource->sendLogicalPosition(POS.x, POS.y);

    if (*PXWLFORCESCALEZERO && isXWayland)
        resource->sendLogicalSize(monitor->vecTransformedSize.x, monitor->vecTransformedSize.y);
    else
        resource->sendLogicalSize(monitor->vecSize.x, monitor->vecSize.y);

    if (resource->version() < OUTPUT_DONE_DEPRECATED_SINCE_VERSION)
        resource->sendDone();
}