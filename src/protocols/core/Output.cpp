#include "Output.hpp"
#include "Compositor.hpp"
#include "../../Compositor.hpp"
#include "../../helpers/Monitor.hpp"

CWLOutputResource::CWLOutputResource(SP<CWlOutput> resource_, PHLMONITOR pMonitor) : m_monitor(pMonitor), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setData(this);

    m_client = m_resource->client();

    if (!m_monitor)
        return;

    m_resource->setOnDestroy([this](CWlOutput* r) {
        if (m_monitor && PROTO::outputs.contains(m_monitor->m_name))
            PROTO::outputs.at(m_monitor->m_name)->destroyResource(this);
    });
    m_resource->setRelease([this](CWlOutput* r) {
        if (m_monitor && PROTO::outputs.contains(m_monitor->m_name))
            PROTO::outputs.at(m_monitor->m_name)->destroyResource(this);
    });

    if (m_resource->version() >= 4) {
        m_resource->sendName(m_monitor->m_name.c_str());
        m_resource->sendDescription(m_monitor->m_description.c_str());
    }

    updateState();

    PROTO::compositor->forEachSurface([](SP<CWLSurfaceResource> surf) {
        auto HLSurf = CWLSurface::fromResource(surf);

        if (!HLSurf)
            return;

        const auto GEOMETRY = HLSurf->getSurfaceBoxGlobal();

        if (!GEOMETRY.has_value())
            return;

        for (auto& m : g_pCompositor->m_monitors) {
            if (!m->logicalBox().expand(-4).overlaps(*GEOMETRY))
                continue;

            surf->enter(m);
        }
    });
}

SP<CWLOutputResource> CWLOutputResource::fromResource(wl_resource* res) {
    auto data = static_cast<CWLOutputResource*>(static_cast<CWlOutput*>(wl_resource_get_user_data(res))->data());
    return data ? data->m_self.lock() : nullptr;
}

bool CWLOutputResource::good() {
    return m_resource->resource();
}

wl_client* CWLOutputResource::client() {
    return m_client;
}

SP<CWlOutput> CWLOutputResource::getResource() {
    return m_resource;
}

void CWLOutputResource::updateState() {
    if (!m_monitor || (m_owner && m_owner->m_defunct))
        return;

    if (m_resource->version() >= 2)
        m_resource->sendScale(std::ceil(m_monitor->m_scale));

    m_resource->sendMode(WL_OUTPUT_MODE_CURRENT, m_monitor->m_pixelSize.x, m_monitor->m_pixelSize.y, m_monitor->m_refreshRate * 1000.0);

    m_resource->sendGeometry(0, 0, m_monitor->m_output->physicalSize.x, m_monitor->m_output->physicalSize.y, m_monitor->m_output->subpixel, m_monitor->m_output->make.c_str(),
                             m_monitor->m_output->model.c_str(), m_monitor->m_transform);

    if (m_resource->version() >= 2)
        m_resource->sendDone();
}

CWLOutputProtocol::CWLOutputProtocol(const wl_interface* iface, const int& ver, const std::string& name, PHLMONITOR pMonitor) :
    IWaylandProtocol(iface, ver, name), m_monitor(pMonitor), m_name(pMonitor->m_name) {

    m_listeners.modeChanged = m_monitor->m_events.modeChanged.listen([this] {
        for (auto const& o : m_outputs) {
            o->updateState();
        }
    });
}

void CWLOutputProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    if UNLIKELY (m_defunct)
        Debug::log(WARN, "[wl_output] Binding a wl_output that's inert?? Possible client bug.");

    const auto RESOURCE = m_outputs.emplace_back(makeShared<CWLOutputResource>(makeShared<CWlOutput>(client, ver, id), m_monitor.lock()));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_outputs.pop_back();
        return;
    }

    RESOURCE->m_self  = RESOURCE;
    RESOURCE->m_owner = m_self;
    m_events.outputBound.emit(RESOURCE);
}

void CWLOutputProtocol::destroyResource(CWLOutputResource* resource) {
    std::erase_if(m_outputs, [&](const auto& other) { return other.get() == resource; });

    if (m_outputs.empty() && m_defunct)
        PROTO::outputs.erase(m_name);
}

SP<CWLOutputResource> CWLOutputProtocol::outputResourceFrom(wl_client* client) {
    for (auto const& r : m_outputs) {
        if (r->client() != client)
            continue;

        return r;
    }

    return nullptr;
}

void CWLOutputProtocol::remove() {
    if UNLIKELY (m_defunct)
        return;

    m_defunct = true;
    removeGlobal();
}

bool CWLOutputProtocol::isDefunct() {
    return m_defunct;
}

void CWLOutputProtocol::sendDone() {
    if UNLIKELY (m_defunct)
        return;

    for (auto const& r : m_outputs) {
        r->m_resource->sendDone();
    }
}
