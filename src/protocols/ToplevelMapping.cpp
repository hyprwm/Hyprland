#include "ToplevelMapping.hpp"
#include "hyprland-toplevel-mapping-v1.hpp"
#include "ForeignToplevelWlr.hpp"
#include "ForeignToplevel.hpp"

CToplevelWindowMappingHandle::CToplevelWindowMappingHandle(SP<CHyprlandToplevelWindowMappingHandleV1> resource_) : m_resource(resource_) {}

CToplevelMappingManager::CToplevelMappingManager(SP<CHyprlandToplevelMappingManagerV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!resource_->resource())
        return;

    m_resource->setOnDestroy([this](CHyprlandToplevelMappingManagerV1* h) { PROTO::toplevelMapping->onManagerResourceDestroy(this); });
    m_resource->setDestroy([this](CHyprlandToplevelMappingManagerV1* h) { PROTO::toplevelMapping->onManagerResourceDestroy(this); });

    m_resource->setGetWindowForToplevel([this](CHyprlandToplevelMappingManagerV1* mgr, uint32_t handle, wl_resource* toplevel) {
        const auto NEWHANDLE = PROTO::toplevelMapping->m_handles.emplace_back(
            makeShared<CToplevelWindowMappingHandle>(makeShared<CHyprlandToplevelWindowMappingHandleV1>(m_resource->client(), m_resource->version(), handle)));

        if UNLIKELY (!NEWHANDLE->m_resource->resource()) {
            LOGM(ERR, "Couldn't alloc mapping handle! (no memory)");
            m_resource->noMemory();
            return;
        }

        NEWHANDLE->m_resource->setOnDestroy([](CHyprlandToplevelWindowMappingHandleV1* h) { PROTO::toplevelMapping->destroyHandle(h); });
        NEWHANDLE->m_resource->setDestroy([](CHyprlandToplevelWindowMappingHandleV1* h) { PROTO::toplevelMapping->destroyHandle(h); });

        const auto WINDOW = PROTO::foreignToplevel->windowFromHandleResource(toplevel);
        if (!WINDOW)
            NEWHANDLE->m_resource->sendFailed();
        else
            NEWHANDLE->m_resource->sendWindowAddress((uint64_t)WINDOW.get() >> 32 & 0xFFFFFFFF, (uint64_t)WINDOW.get() & 0xFFFFFFFF);
    });
    m_resource->setGetWindowForToplevelWlr([this](CHyprlandToplevelMappingManagerV1* mgr, uint32_t handle, wl_resource* toplevel) {
        const auto NEWHANDLE = PROTO::toplevelMapping->m_handles.emplace_back(
            makeShared<CToplevelWindowMappingHandle>(makeShared<CHyprlandToplevelWindowMappingHandleV1>(m_resource->client(), m_resource->version(), handle)));

        if UNLIKELY (!NEWHANDLE->m_resource->resource()) {
            LOGM(ERR, "Couldn't alloc mapping handle! (no memory)");
            m_resource->noMemory();
            return;
        }

        NEWHANDLE->m_resource->setOnDestroy([](CHyprlandToplevelWindowMappingHandleV1* h) { PROTO::toplevelMapping->destroyHandle(h); });
        NEWHANDLE->m_resource->setDestroy([](CHyprlandToplevelWindowMappingHandleV1* h) { PROTO::toplevelMapping->destroyHandle(h); });

        const auto WINDOW = PROTO::foreignToplevelWlr->windowFromHandleResource(toplevel);
        if (!WINDOW)
            NEWHANDLE->m_resource->sendFailed();
        else
            NEWHANDLE->m_resource->sendWindowAddress((uint64_t)WINDOW.get() >> 32 & 0xFFFFFFFF, (uint64_t)WINDOW.get() & 0xFFFFFFFF);
    });
}

bool CToplevelMappingManager::good() const {
    return m_resource->resource();
}

CToplevelMappingProtocol::CToplevelMappingProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {}

void CToplevelMappingProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CToplevelMappingManager>(makeShared<CHyprlandToplevelMappingManagerV1>(client, ver, id))).get();

    if UNLIKELY (!RESOURCE->good()) {
        LOGM(ERR, "Couldn't create a toplevel mapping manager");
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }
}

void CToplevelMappingProtocol::onManagerResourceDestroy(CToplevelMappingManager* mgr) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == mgr; });
}

void CToplevelMappingProtocol::destroyHandle(CHyprlandToplevelWindowMappingHandleV1* handle) {
    std::erase_if(m_handles, [&](const auto& other) { return other->m_resource.get() == handle; });
}