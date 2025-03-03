#include "Output.hpp"
#include "Compositor.hpp"
#include "../../Compositor.hpp"
#include "../../helpers/Monitor.hpp"

CWLOutputResource::CWLOutputResource(SP<CWlOutput> resource_, PHLMONITOR pMonitor) : monitor(pMonitor), resource(resource_) {
    if UNLIKELY (!good())
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

    if (resource->version() >= 4) {
        resource->sendName(monitor->szName.c_str());
        resource->sendDescription(monitor->szDescription.c_str());
    }

    updateState();

    PROTO::compositor->forEachSurface([](SP<CWLSurfaceResource> surf) {
        auto HLSurf = CWLSurface::fromResource(surf);

        if (!HLSurf)
            return;

        const auto GEOMETRY = HLSurf->getSurfaceBoxGlobal();

        if (!GEOMETRY.has_value())
            return;

        for (auto& m : g_pCompositor->m_vMonitors) {
            if (!m->logicalBox().expand(-4).overlaps(*GEOMETRY))
                continue;

            surf->enter(m);
        }
    });
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
    if (!monitor || (owner && owner->defunct))
        return;

    if (resource->version() >= 2)
        resource->sendScale(std::ceil(monitor->scale));

    resource->sendMode((wl_output_mode)(WL_OUTPUT_MODE_CURRENT), monitor->vecPixelSize.x, monitor->vecPixelSize.y, monitor->refreshRate * 1000.0);

    resource->sendGeometry(0, 0, monitor->output->physicalSize.x, monitor->output->physicalSize.y, (wl_output_subpixel)monitor->output->subpixel, monitor->output->make.c_str(),
                           monitor->output->model.c_str(), monitor->transform);

    if (resource->version() >= 2)
        resource->sendDone();
}

CWLOutputProtocol::CWLOutputProtocol(const wl_interface* iface, const int& ver, const std::string& name, PHLMONITOR pMonitor) :
    IWaylandProtocol(iface, ver, name), monitor(pMonitor), szName(pMonitor->szName) {

    m_listeners.modeChanged = monitor->events.modeChanged.registerListener([this](std::any d) {
        for (auto const& o : m_vOutputs) {
            o->updateState();
        }
    });
}

void CWLOutputProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    if UNLIKELY (defunct)
        NDebug::log(WARN, "[wl_output] Binding a wl_output that's inert?? Possible client bug.");

    const auto RESOURCE = m_vOutputs.emplace_back(makeShared<CWLOutputResource>(makeShared<CWlOutput>(client, ver, id), monitor.lock()));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vOutputs.pop_back();
        return;
    }

    RESOURCE->self  = RESOURCE;
    RESOURCE->owner = self;
}

void CWLOutputProtocol::destroyResource(CWLOutputResource* resource) {
    std::erase_if(m_vOutputs, [&](const auto& other) { return other.get() == resource; });

    if (m_vOutputs.empty() && defunct)
        PROTO::outputs.erase(szName);
}

SP<CWLOutputResource> CWLOutputProtocol::outputResourceFrom(wl_client* client) {
    for (auto const& r : m_vOutputs) {
        if (r->client() != client)
            continue;

        return r;
    }

    return nullptr;
}

void CWLOutputProtocol::remove() {
    if UNLIKELY (defunct)
        return;

    defunct = true;
    removeGlobal();
}

bool CWLOutputProtocol::isDefunct() {
    return defunct;
}

void CWLOutputProtocol::sendDone() {
    if UNLIKELY (defunct)
        return;

    for (auto const& r : m_vOutputs) {
        r->resource->sendDone();
    }
}
