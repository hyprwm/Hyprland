#include "ForeignToplevel.hpp"
#include "../Compositor.hpp"

CForeignToplevelHandle::CForeignToplevelHandle(SP<CExtForeignToplevelHandleV1> resource_, PHLWINDOW pWindow_) : resource(resource_), pWindow(pWindow_) {
    if (!resource_->resource())
        return;

    resource->setOnDestroy([this](CExtForeignToplevelHandleV1* h) { PROTO::foreignToplevel->destroyHandle(this); });
    resource->setDestroy([this](CExtForeignToplevelHandleV1* h) { PROTO::foreignToplevel->destroyHandle(this); });
}

bool CForeignToplevelHandle::good() {
    return resource->resource();
}

PHLWINDOW CForeignToplevelHandle::window() {
    return pWindow.lock();
}

CForeignToplevelList::CForeignToplevelList(SP<CExtForeignToplevelListV1> resource_) : resource(resource_) {
    if (!resource_->resource())
        return;

    resource->setOnDestroy([this](CExtForeignToplevelListV1* h) { PROTO::foreignToplevel->onManagerResourceDestroy(this); });
    resource->setDestroy([this](CExtForeignToplevelListV1* h) { PROTO::foreignToplevel->onManagerResourceDestroy(this); });

    resource->setStop([this](CExtForeignToplevelListV1* h) {
        resource->sendFinished();
        finished = true;
        LOGM(LOG, "CForeignToplevelList: finished");
    });

    for (auto& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped || w->m_bFadingOut)
            continue;

        onMap(w);
    }
}

void CForeignToplevelList::onMap(PHLWINDOW pWindow) {
    if (finished)
        return;

    const auto NEWHANDLE = PROTO::foreignToplevel->m_vHandles.emplace_back(
        makeShared<CForeignToplevelHandle>(makeShared<CExtForeignToplevelHandleV1>(resource->client(), resource->version(), 0), pWindow));

    if (!NEWHANDLE->good()) {
        LOGM(ERR, "Couldn't create a foreign handle");
        resource->noMemory();
        PROTO::foreignToplevel->m_vHandles.pop_back();
        return;
    }

    const auto IDENTIFIER = std::format("{:08x}->{:016x}", static_cast<uint32_t>((uintptr_t)this & 0xFFFFFFFF), (uintptr_t)pWindow.get());

    LOGM(LOG, "Newly mapped window gets an identifier of {}", IDENTIFIER);
    resource->sendToplevel(NEWHANDLE->resource.get());
    NEWHANDLE->resource->sendIdentifier(IDENTIFIER.c_str());
    NEWHANDLE->resource->sendAppId(pWindow->m_szInitialClass.c_str());
    NEWHANDLE->resource->sendTitle(pWindow->m_szInitialTitle.c_str());
    NEWHANDLE->resource->sendDone();

    handles.push_back(NEWHANDLE);
}

SP<CForeignToplevelHandle> CForeignToplevelList::handleForWindow(PHLWINDOW pWindow) {
    std::erase_if(handles, [](const auto& wp) { return wp.expired(); });
    const auto IT = std::find_if(handles.begin(), handles.end(), [pWindow](const auto& h) { return h->window() == pWindow; });
    return IT == handles.end() ? SP<CForeignToplevelHandle>{} : IT->lock();
}

void CForeignToplevelList::onTitle(PHLWINDOW pWindow) {
    if (finished)
        return;

    const auto H = handleForWindow(pWindow);
    if (!H || H->closed)
        return;

    H->resource->sendTitle(pWindow->m_szTitle.c_str());
    H->resource->sendDone();
}

void CForeignToplevelList::onClass(PHLWINDOW pWindow) {
    if (finished)
        return;

    const auto H = handleForWindow(pWindow);
    if (!H || H->closed)
        return;

    H->resource->sendAppId(pWindow->m_szClass.c_str());
    H->resource->sendDone();
}

void CForeignToplevelList::onUnmap(PHLWINDOW pWindow) {
    if (finished)
        return;

    const auto H = handleForWindow(pWindow);
    if (!H)
        return;

    H->resource->sendClosed();
    H->closed = true;
}

bool CForeignToplevelList::good() {
    return resource->resource();
}

CForeignToplevelProtocol::CForeignToplevelProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P = g_pHookSystem->hookDynamic("openWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        for (auto& m : m_vManagers) {
            m->onMap(std::any_cast<PHLWINDOW>(data));
        }
    });

    static auto P1 = g_pHookSystem->hookDynamic("closeWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        for (auto& m : m_vManagers) {
            m->onUnmap(std::any_cast<PHLWINDOW>(data));
        }
    });

    static auto P2 = g_pHookSystem->hookDynamic("windowTitle", [this](void* self, SCallbackInfo& info, std::any data) {
        for (auto& m : m_vManagers) {
            m->onTitle(std::any_cast<PHLWINDOW>(data));
        }
    });
}

void CForeignToplevelProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CForeignToplevelList>(makeShared<CExtForeignToplevelListV1>(client, ver, id))).get();

    if (!RESOURCE->good()) {
        LOGM(ERR, "Couldn't create a foreign list");
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }
}

void CForeignToplevelProtocol::onManagerResourceDestroy(CForeignToplevelList* mgr) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == mgr; });
}

void CForeignToplevelProtocol::destroyHandle(CForeignToplevelHandle* handle) {
    std::erase_if(m_vHandles, [&](const auto& other) { return other.get() == handle; });
}
