#include "XWaylandShell.hpp"
#include "core/Compositor.hpp"
#include <algorithm>

CXWaylandSurfaceResource::CXWaylandSurfaceResource(SP<CXwaylandSurfaceV1> resource_, SP<CWLSurfaceResource> surface_) : surface(surface_), resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CXwaylandSurfaceV1* r) {
        events.destroy.emit();
        PROTO::xwaylandShell->destroyResource(this);
    });
    resource->setOnDestroy([this](CXwaylandSurfaceV1* r) {
        events.destroy.emit();
        PROTO::xwaylandShell->destroyResource(this);
    });

    pClient = resource->client();

    resource->setSetSerial([this](CXwaylandSurfaceV1* r, uint32_t lo, uint32_t hi) {
        serial = (((uint64_t)hi) << 32) + lo;
        PROTO::xwaylandShell->events.newSurface.emit(self.lock());
    });
}

CXWaylandSurfaceResource::~CXWaylandSurfaceResource() {
    events.destroy.emit();
}

bool CXWaylandSurfaceResource::good() {
    return resource->resource();
}

wl_client* CXWaylandSurfaceResource::client() {
    return pClient;
}

CXWaylandShellResource::CXWaylandShellResource(SP<CXwaylandShellV1> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CXwaylandShellV1* r) { PROTO::xwaylandShell->destroyResource(this); });
    resource->setOnDestroy([this](CXwaylandShellV1* r) { PROTO::xwaylandShell->destroyResource(this); });

    resource->setGetXwaylandSurface([this](CXwaylandShellV1* r, uint32_t id, wl_resource* surface) {
        const auto RESOURCE = PROTO::xwaylandShell->m_vSurfaces.emplace_back(
            makeShared<CXWaylandSurfaceResource>(makeShared<CXwaylandSurfaceV1>(r->client(), r->version(), id), CWLSurfaceResource::fromResource(surface)));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::xwaylandShell->m_vSurfaces.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;
    });
}

bool CXWaylandShellResource::good() {
    return resource->resource();
}

CXWaylandShellProtocol::CXWaylandShellProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CXWaylandShellProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CXWaylandShellResource>(makeShared<CXwaylandShellV1>(client, ver, id)));

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }
}

void CXWaylandShellProtocol::destroyResource(CXWaylandShellResource* resource) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == resource; });
}

void CXWaylandShellProtocol::destroyResource(CXWaylandSurfaceResource* resource) {
    std::erase_if(m_vSurfaces, [&](const auto& other) { return other.get() == resource; });
}
