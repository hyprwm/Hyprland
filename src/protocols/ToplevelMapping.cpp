#include "ToplevelMapping.hpp"
#include "hyprland-toplevel-mapping-v1.hpp"
#include "ForeignToplevelWlr.hpp"
#include "ForeignToplevel.hpp"

CToplevelWindowMappingHandle::CToplevelWindowMappingHandle(SP<CHyprlandToplevelWindowMappingHandleV1> resource_) : resource(resource_) {}

bool CToplevelWindowMappingHandle::good() {
    return resource->resource();
}

CWindowToplevelMappingHandle::CWindowToplevelMappingHandle(SP<CHyprlandWindowToplevelMappingHandleV1> resource_) : resource(resource_) {}

bool CWindowToplevelMappingHandle::good() {
    return resource->resource();
}

CWindowWlrToplevelMappingHandle::CWindowWlrToplevelMappingHandle(SP<CHyprlandWindowWlrToplevelMappingHandleV1> resource_) : resource(resource_) {}

bool CWindowWlrToplevelMappingHandle::good() {
    return resource->resource();
}

CToplevelMappingManager::CToplevelMappingManager(SP<CHyprlandToplevelMappingManagerV1> resource_) : resource(resource_) {
    if UNLIKELY (!resource_->resource())
        return;

    resource->setOnDestroy([this](CHyprlandToplevelMappingManagerV1* h) { PROTO::toplevelMapping->onManagerResourceDestroy(this); });
    resource->setGetWindowForToplevel([this](CHyprlandToplevelMappingManagerV1* mgr, uint32_t handle, wl_resource* toplevel) {
        const auto HANDLE = PROTO::toplevelMapping->m_vToplevelWindowHandles.emplace_back(
            makeShared<CToplevelWindowMappingHandle>(makeShared<CHyprlandToplevelWindowMappingHandleV1>(resource->client(), resource->version(), handle)));

        if UNLIKELY (!HANDLE->good()) {
            LOGM(ERR, "Couldn't alloc mapping handle! (no memory)");
            resource->noMemory();
            PROTO::toplevelMapping->destroyHandle(HANDLE.get());
            return;
        }

        const auto WINDOW = PROTO::foreignToplevel->windowFromHandleResource(toplevel);
        if (!WINDOW)
            HANDLE->resource->sendFailed();
        else
            HANDLE->resource->sendWindowAddress(((uint64_t) WINDOW.get()) >> 32, ((uint64_t) WINDOW.get()) & 0xFFFFFFFF);
    });
    resource->setGetWindowForToplevelWlr([this](CHyprlandToplevelMappingManagerV1* mgr, uint32_t handle, wl_resource* toplevel) {
        const auto HANDLE = PROTO::toplevelMapping->m_vToplevelWindowHandles.emplace_back(
            makeShared<CToplevelWindowMappingHandle>(makeShared<CHyprlandToplevelWindowMappingHandleV1>(resource->client(), resource->version(), handle)));

        if UNLIKELY (!HANDLE->good()) {
            LOGM(ERR, "Couldn't alloc mapping handle! (no memory)");
            resource->noMemory();
            PROTO::toplevelMapping->destroyHandle(HANDLE.get());
            return;
        }

        const auto WINDOW = PROTO::foreignToplevelWlr->windowFromHandleResource(toplevel);
        if (!WINDOW)
            HANDLE->resource->sendFailed();
        else
            HANDLE->resource->sendWindowAddress(((uint64_t) WINDOW.get()) >> 32, ((uint64_t) WINDOW.get()) & 0xFFFFFFFF);
    });
    resource->setGetToplevelForWindow([this](CHyprlandToplevelMappingManagerV1* mgr, uint32_t handle, uint32_t address, uint32_t address_hi) {
        const auto HANDLE = PROTO::toplevelMapping->m_vWindowToplevelHandles.emplace_back(
            makeShared<CWindowToplevelMappingHandle>(makeShared<CHyprlandWindowToplevelMappingHandleV1>(resource->client(), resource->version(), handle)));

        if UNLIKELY (!HANDLE->good()) {
            LOGM(ERR, "Couldn't alloc mapping handle! (no memory)");
            resource->noMemory();
            PROTO::toplevelMapping->destroyHandle(HANDLE.get());
            return;
        }
        const auto FULL_ADDRESS = (((uint64_t) address_hi) << 32) | address;
        // TODO: Get toplevel for window address
    });
    resource->setGetToplevelWlrForWindow([this](CHyprlandToplevelMappingManagerV1* mgr, uint32_t handle, uint32_t address, uint32_t address_hi) {
        const auto HANDLE = PROTO::toplevelMapping->m_vWindowWlrToplevelHandles.emplace_back(
            makeShared<CWindowWlrToplevelMappingHandle>(makeShared<CHyprlandWindowWlrToplevelMappingHandleV1>(resource->client(), resource->version(), handle)));

        if UNLIKELY (!HANDLE->good()) {
            LOGM(ERR, "Couldn't alloc mapping handle! (no memory)");
            resource->noMemory();
            PROTO::toplevelMapping->destroyHandle(HANDLE.get());
            return;
        }
        const auto FULL_ADDRESS = (((uint64_t) address_hi) << 32) | address;
        // TODO: Get wlr toplevel for window address
    });
}

bool CToplevelMappingManager::good() {
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

void CToplevelMappingProtocol::destroyHandle(CToplevelWindowMappingHandle* handle) {
    std::erase_if(m_vToplevelWindowHandles, [&](const auto& other) { return other.get() == handle; });
}

void CToplevelMappingProtocol::destroyHandle(CWindowToplevelMappingHandle* handle) {
    std::erase_if(m_vWindowToplevelHandles, [&](const auto& other) { return other.get() == handle; });
}

void CToplevelMappingProtocol::destroyHandle(CWindowWlrToplevelMappingHandle* handle) {
    std::erase_if(m_vWindowWlrToplevelHandles, [&](const auto& other) { return other.get() == handle; });
}