#include "Output.hpp"
#include "../../helpers/Monitor.hpp"

CWLOutputResource::CWLOutputResource(SP<CWlOutput> resource_, SP<CMonitor> pMonitor) : monitor(pMonitor), resource(resource_) {
    if (!good())
        return;

    resource->setData(this);

    pClient = resource->client();

    if (!monitor)
        return;

    resource->setOnDestroy([this](CWlOutput* r) {
        if (monitor && PROTO::outputs.contains(monitor->szName))
            PROTO::outputs.at(monitor->szName)->destroyResource(this);
    });
    resource->setRelease([this](CWlOutput* r) {
        if (monitor && PROTO::outputs.contains(monitor->szName))
            PROTO::outputs.at(monitor->szName)->destroyResource(this);
    });

    resource->sendGeometry(0, 0, monitor->output->phys_width, monitor->output->phys_height, monitor->output->subpixel, monitor->output->make ? monitor->output->make : "null",
                           monitor->output->model ? monitor->output->model : "null", monitor->transform);
    if (resource->version() >= 4) {
        resource->sendName(monitor->szName.c_str());
        resource->sendDescription(monitor->szDescription.c_str());
    }

    updateState();
}

SP<CWLOutputResource> CWLOutputResource::fromResource(wl_resource* res) {
    auto data = (CWLOutputResource*)(((CWlOutput*)wl_resource_get_user_data(res))->data());
    return data ? data->self.lock() : nullptr;
}

bool CWLOutputResource::good() {
    return resource->resource();
}

wl_client* CWLOutputResource::client() {
    return pClient;
}

SP<CWlOutput> CWLOutputResource::getResource() {
    return resource;
}

void CWLOutputResource::updateState() {
    if (!monitor)
        return;

    if (resource->version() >= 2)
        resource->sendScale(std::ceil(monitor->scale));

    resource->sendMode((wl_output_mode)(WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED), monitor->vecPixelSize.x, monitor->vecPixelSize.y, monitor->refreshRate * 1000.0);

    if (resource->version() >= 2)
        resource->sendDone();
}

CWLOutputProtocol::CWLOutputProtocol(const wl_interface* iface, const int& ver, const std::string& name, SP<CMonitor> pMonitor) :
    IWaylandProtocol(iface, ver, name), monitor(pMonitor), szName(pMonitor->szName) {

    listeners.modeChanged = monitor->events.modeChanged.registerListener([this](std::any d) {
        for (auto& o : m_vOutputs) {
            o->updateState();
        }
    });
}

void CWLOutputProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    if (defunct)
        Debug::log(WARN, "[wl_output] Binding a wl_output that's inert?? Possible client bug.");

    const auto RESOURCE = m_vOutputs.emplace_back(makeShared<CWLOutputResource>(makeShared<CWlOutput>(client, ver, id), monitor.lock()));

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vOutputs.pop_back();
        return;
    }

    RESOURCE->self = RESOURCE;
}

void CWLOutputProtocol::destroyResource(CWLOutputResource* resource) {
    std::erase_if(m_vOutputs, [&](const auto& other) { return other.get() == resource; });

    if (m_vOutputs.empty() && defunct)
        PROTO::outputs.erase(szName);
}

SP<CWLOutputResource> CWLOutputProtocol::outputResourceFrom(wl_client* client) {
    for (auto& r : m_vOutputs) {
        if (r->client() != client)
            continue;

        return r;
    }

    return nullptr;
}

void CWLOutputProtocol::remove() {
    if (defunct)
        return;

    defunct = true;
    removeGlobal();
}

bool CWLOutputProtocol::isDefunct() {
    return defunct;
}
