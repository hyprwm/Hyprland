#include "XDGTag.hpp"
#include "XDGShell.hpp"

CXDGToplevelTagManagerResource::CXDGToplevelTagManagerResource(UP<CXdgToplevelTagManagerV1>&& resource) : m_resource(std::move(resource)) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CXdgToplevelTagManagerV1* r) { PROTO::xdgTag->destroyResource(this); });
    m_resource->setOnDestroy([this](CXdgToplevelTagManagerV1* r) { PROTO::xdgTag->destroyResource(this); });

    m_resource->setSetToplevelTag([](CXdgToplevelTagManagerV1* r, wl_resource* toplevel, const char* tag) {
        auto TOPLEVEL = CXDGToplevelResource::fromResource(toplevel);

        if (!TOPLEVEL) {
            r->error(-1, "Invalid toplevel handle");
            return;
        }

        TOPLEVEL->m_toplevelTag = tag;
    });

    m_resource->setSetToplevelDescription([](CXdgToplevelTagManagerV1* r, wl_resource* toplevel, const char* description) {
        auto TOPLEVEL = CXDGToplevelResource::fromResource(toplevel);

        if (!TOPLEVEL) {
            r->error(-1, "Invalid toplevel handle");
            return;
        }

        TOPLEVEL->m_toplevelDescription = description;
    });
}

bool CXDGToplevelTagManagerResource::good() {
    return m_resource->resource();
}

CXDGToplevelTagProtocol::CXDGToplevelTagProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CXDGToplevelTagProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE =
        WP<CXDGToplevelTagManagerResource>{m_vManagers.emplace_back(makeUnique<CXDGToplevelTagManagerResource>(makeUnique<CXdgToplevelTagManagerV1>(client, ver, id)))};

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        return;
    }
}

void CXDGToplevelTagProtocol::destroyResource(CXDGToplevelTagManagerResource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == res; });
}
