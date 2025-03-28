#include "ToplevelMapping.hpp"
#include "hyprland-toplevel-mapping-v1.hpp"
#include "ForeignToplevelWlr.hpp"
#include "ForeignToplevel.hpp"

CToplevelMappingManager::CToplevelMappingManager(SP<CHyprlandToplevelMappingManagerV1> resource_) : resource(resource_) {
    if UNLIKELY (!resource_->resource())
        return;

    resource->setOnDestroy([this](CHyprlandToplevelMappingManagerV1* h) { PROTO::toplevelMapping->onManagerResourceDestroy(this); });
    resource->setDestroy([this](CHyprlandToplevelMappingManagerV1* h) { PROTO::toplevelMapping->onManagerResourceDestroy(this); });

    resource->setGetWindowForToplevel([this](CHyprlandToplevelMappingManagerV1* mgr, uint32_t handle, wl_resource* toplevel) {
        const auto NEWHANDLE = makeShared<CHyprlandToplevelWindowMappingHandleV1>(resource->client(), resource->version(), handle);

        if UNLIKELY (!NEWHANDLE->resource()) {
            LOGM(ERR, "Couldn't alloc mapping handle! (no memory)");
            resource->noMemory();
            return;
        }

        const auto WINDOW = PROTO::foreignToplevel->windowFromHandleResource(toplevel);
        if (!WINDOW)
            NEWHANDLE->sendFailed();
        else
            NEWHANDLE->sendWindowAddress((uint64_t)WINDOW.get() >> 32 & 0xFFFFFFFF, (uint64_t)WINDOW.get() & 0xFFFFFFFF);
    });
    resource->setGetWindowForToplevelWlr([this](CHyprlandToplevelMappingManagerV1* mgr, uint32_t handle, wl_resource* toplevel) {
        const auto NEWHANDLE = makeShared<CHyprlandToplevelWindowMappingHandleV1>(resource->client(), resource->version(), handle);

        if UNLIKELY (!NEWHANDLE->resource()) {
            LOGM(ERR, "Couldn't alloc mapping handle! (no memory)");
            resource->noMemory();
            return;
        }

        const auto WINDOW = PROTO::foreignToplevelWlr->windowFromHandleResource(toplevel);
        if (!WINDOW)
            NEWHANDLE->sendFailed();
        else
            NEWHANDLE->sendWindowAddress((uint64_t)WINDOW.get() >> 32 & 0xFFFFFFFF, (uint64_t)WINDOW.get() & 0xFFFFFFFF);
    });
}

bool CToplevelMappingManager::good() const {
    return resource->resource();
}

CToplevelMappingProtocol::CToplevelMappingProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {}

void CToplevelMappingProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeUnique<CToplevelMappingManager>(makeShared<CHyprlandToplevelMappingManagerV1>(client, ver, id))).get();

    if UNLIKELY (!RESOURCE->good()) {
        LOGM(ERR, "Couldn't create a toplevel mapping manager");
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }
}

void CToplevelMappingProtocol::onManagerResourceDestroy(CToplevelMappingManager* mgr) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == mgr; });
}