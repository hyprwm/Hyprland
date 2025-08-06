#include "XWaylandShell.hpp"
#include "core/Compositor.hpp"
#include <algorithm>

CXWaylandSurfaceResource::CXWaylandSurfaceResource(SP<CXwaylandSurfaceV1> resource_, SP<CWLSurfaceResource> surface_) : m_surface(surface_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CXwaylandSurfaceV1* r) {
        events.destroy.emit();
        PROTO::xwaylandShell->destroyResource(this);
    });
    m_resource->setOnDestroy([this](CXwaylandSurfaceV1* r) {
        events.destroy.emit();
        PROTO::xwaylandShell->destroyResource(this);
    });

    m_client = m_resource->client();

    m_resource->setSetSerial([this](CXwaylandSurfaceV1* r, uint32_t lo, uint32_t hi) {
        m_serial = (static_cast<uint64_t>(hi) << 32) + lo;
        PROTO::xwaylandShell->m_events.newSurface.emit(m_self.lock());
    });
}

CXWaylandSurfaceResource::~CXWaylandSurfaceResource() {
    events.destroy.emit();
}

bool CXWaylandSurfaceResource::good() {
    return m_resource->resource();
}

wl_client* CXWaylandSurfaceResource::client() {
    return m_client;
}

CXWaylandShellResource::CXWaylandShellResource(SP<CXwaylandShellV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CXwaylandShellV1* r) { PROTO::xwaylandShell->destroyResource(this); });
    m_resource->setOnDestroy([this](CXwaylandShellV1* r) { PROTO::xwaylandShell->destroyResource(this); });

    m_resource->setGetXwaylandSurface([](CXwaylandShellV1* r, uint32_t id, wl_resource* surface) {
        const auto RESOURCE = PROTO::xwaylandShell->m_surfaces.emplace_back(
            makeShared<CXWaylandSurfaceResource>(makeShared<CXwaylandSurfaceV1>(r->client(), r->version(), id), CWLSurfaceResource::fromResource(surface)));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::xwaylandShell->m_surfaces.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;
    });
}

bool CXWaylandShellResource::good() {
    return m_resource->resource();
}

CXWaylandShellProtocol::CXWaylandShellProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CXWaylandShellProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CXWaylandShellResource>(makeShared<CXwaylandShellV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }
}

void CXWaylandShellProtocol::destroyResource(CXWaylandShellResource* resource) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == resource; });
}

void CXWaylandShellProtocol::destroyResource(CXWaylandSurfaceResource* resource) {
    std::erase_if(m_surfaces, [&](const auto& other) { return other.get() == resource; });
}
