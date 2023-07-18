#include "XDGOutput.hpp"
#include "../Compositor.hpp"

#include "xdg-output-unstable-v1-protocol.h"

#define OUTPUT_MANAGER_VERSION                   3
#define OUTPUT_DONE_DEPRECATED_SINCE_VERSION     3
#define OUTPUT_DESCRIPTION_MUTABLE_SINCE_VERSION 3

static void destroyManagerResource(wl_client* client, wl_resource* resource) {
    ((CXDGOutputProtocol*)wl_resource_get_user_data(resource))->onManagerResourceDestroy(resource);
    // will be destroyed by the destruction of the unique_ptr
}

static void destroyOutputResource(wl_client* client, wl_resource* resource) {
    ((CXDGOutputProtocol*)wl_resource_get_user_data(resource))->onOutputResourceDestroy(resource);
    wl_resource_destroy(resource);
}

static void destroyOutputResourceOnly(wl_resource* resource) {
    ((CXDGOutputProtocol*)wl_resource_get_user_data(resource))->onOutputResourceDestroy(resource);
}

static void getXDGOutput(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* outputResource) {
    ((CXDGOutputProtocol*)wl_resource_get_user_data(resource))->onManagerGetXDGOutput(client, resource, id, outputResource);
}

//

static const struct zxdg_output_manager_v1_interface MANAGER_IMPL = {
    .destroy        = destroyManagerResource,
    .get_xdg_output = getXDGOutput,
};

static const struct zxdg_output_v1_interface OUTPUT_IMPL = {
    .destroy = destroyOutputResource,
};

void CXDGOutputProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagerResources, [&](const auto& other) { return other->resource() == res; });
}

void CXDGOutputProtocol::onOutputResourceDestroy(wl_resource* res) {
    std::erase_if(m_vXDGOutputs, [&](const auto& other) { return !other->resource || other->resource->resource() == res; });
}

void CXDGOutputProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagerResources.emplace_back(std::make_unique<CWaylandResource>(client, &zxdg_output_manager_v1_interface, ver, id, true)).get();

    if (!RESOURCE->good()) {
        Debug::log(LOG, "Couldn't bind XDGOutputMgr");
        return;
    }

    RESOURCE->setImplementation(&MANAGER_IMPL, this, nullptr);
}

CXDGOutputProtocol::CXDGOutputProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    g_pHookSystem->hookDynamic("monitorLayoutChanged", [&](void* self, std::any param) { this->updateAllOutputs(); });
    g_pHookSystem->hookDynamic("configReloaded", [&](void* self, std::any param) { this->updateAllOutputs(); });
    g_pHookSystem->hookDynamic("monitorRemoved", [&](void* self, std::any param) {
        const auto PMONITOR = std::any_cast<CMonitor*>(param);
        std::erase_if(m_vXDGOutputs, [&](const auto& other) { return other->monitor == PMONITOR; });
    });
}

void CXDGOutputProtocol::onManagerGetXDGOutput(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* outputResource) {
    const auto OUTPUT = wlr_output_from_resource(outputResource);

    if (!OUTPUT)
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromOutput(OUTPUT);

    if (!PMONITOR)
        return;

    SXDGOutput* pXDGOutput = m_vXDGOutputs.emplace_back(std::make_unique<SXDGOutput>(PMONITOR)).get();
#ifndef NO_XWAYLAND
    if (g_pXWaylandManager->m_sWLRXWayland->server->client == client)
        pXDGOutput->isXWayland = true;
#endif
    pXDGOutput->client = client;

    pXDGOutput->resource = std::make_unique<CWaylandResource>(client, &zxdg_output_v1_interface, wl_resource_get_version(resource), id);

    if (!pXDGOutput->resource->good()) {
        pXDGOutput->resource.release();
        return;
    }

    pXDGOutput->resource->setImplementation(&OUTPUT_IMPL, this, destroyOutputResourceOnly);
    const auto XDGVER = pXDGOutput->resource->version();

    if (XDGVER >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION)
        zxdg_output_v1_send_name(pXDGOutput->resource->resource(), PMONITOR->szName.c_str());
    if (XDGVER >= ZXDG_OUTPUT_V1_DESCRIPTION_SINCE_VERSION && PMONITOR->output->description)
        zxdg_output_v1_send_description(pXDGOutput->resource->resource(), PMONITOR->output->description);

    updateOutputDetails(pXDGOutput);

    const auto OUTPUTVER = wl_resource_get_version(outputResource);
    if (OUTPUTVER >= WL_OUTPUT_DONE_SINCE_VERSION && XDGVER >= OUTPUT_DONE_DEPRECATED_SINCE_VERSION)
        wl_output_send_done(outputResource);
}

void CXDGOutputProtocol::updateOutputDetails(SXDGOutput* pOutput) {
    static auto* const PXWLFORCESCALEZERO = &g_pConfigManager->getConfigValuePtr("xwayland:force_zero_scaling")->intValue;

    zxdg_output_v1_send_logical_position(pOutput->resource->resource(), pOutput->monitor->vecPosition.x, pOutput->monitor->vecPosition.y);

    if (*PXWLFORCESCALEZERO && pOutput->isXWayland)
        zxdg_output_v1_send_logical_size(pOutput->resource->resource(), pOutput->monitor->vecPixelSize.x, pOutput->monitor->vecPixelSize.y);
    else
        zxdg_output_v1_send_logical_size(pOutput->resource->resource(), pOutput->monitor->vecSize.x, pOutput->monitor->vecSize.y);

    if (wl_resource_get_version(pOutput->resource->resource()) < OUTPUT_DONE_DEPRECATED_SINCE_VERSION)
        zxdg_output_v1_send_done(pOutput->resource->resource());
}

void CXDGOutputProtocol::updateAllOutputs() {
    for (auto& o : m_vXDGOutputs) {
        updateOutputDetails(o.get());

        wlr_output_schedule_done(o->monitor->output);
    }
}