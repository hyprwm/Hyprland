#include "ForeignToplevel.hpp"
#include "../Compositor.hpp"
#include "../managers/HookSystemManager.hpp"

CForeignToplevelHandle::CForeignToplevelHandle(SP<CExtForeignToplevelHandleV1> resource_, PHLWINDOW pWindow_) : m_resource(resource_), m_window(pWindow_) {
    if UNLIKELY (!resource_->resource())
        return;

    m_resource->setData(this);

    m_resource->setOnDestroy([this](CExtForeignToplevelHandleV1* h) { PROTO::foreignToplevel->destroyHandle(this); });
    m_resource->setDestroy([this](CExtForeignToplevelHandleV1* h) { PROTO::foreignToplevel->destroyHandle(this); });
}

bool CForeignToplevelHandle::good() {
    return m_resource->resource();
}

PHLWINDOW CForeignToplevelHandle::window() {
    return m_window.lock();
}

CForeignToplevelList::CForeignToplevelList(SP<CExtForeignToplevelListV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!resource_->resource())
        return;

    m_resource->setOnDestroy([this](CExtForeignToplevelListV1* h) { PROTO::foreignToplevel->onManagerResourceDestroy(this); });
    m_resource->setDestroy([this](CExtForeignToplevelListV1* h) { PROTO::foreignToplevel->onManagerResourceDestroy(this); });

    m_resource->setStop([this](CExtForeignToplevelListV1* h) {
        m_resource->sendFinished();
        m_finished = true;
        LOGM(LOG, "CForeignToplevelList: finished");
    });

    for (auto const& w : g_pCompositor->m_windows) {
        if (!PROTO::foreignToplevel->windowValidForForeign(w))
            return;

        onMap(w);
    }
}

void CForeignToplevelList::onMap(PHLWINDOW pWindow) {
    if UNLIKELY (m_finished)
        return;

    const auto NEWHANDLE = PROTO::foreignToplevel->m_handles.emplace_back(
        makeShared<CForeignToplevelHandle>(makeShared<CExtForeignToplevelHandleV1>(m_resource->client(), m_resource->version(), 0), pWindow));

    if (!NEWHANDLE->good()) {
        LOGM(ERR, "Couldn't create a foreign handle");
        m_resource->noMemory();
        PROTO::foreignToplevel->m_handles.pop_back();
        return;
    }

    const auto IDENTIFIER = std::format("{:08x}->{:016x}", static_cast<uint32_t>((uintptr_t)this & 0xFFFFFFFF), (uintptr_t)pWindow.get());

    LOGM(LOG, "Newly mapped window gets an identifier of {}", IDENTIFIER);
    m_resource->sendToplevel(NEWHANDLE->m_resource.get());
    NEWHANDLE->m_resource->sendIdentifier(IDENTIFIER.c_str());
    NEWHANDLE->m_resource->sendAppId(pWindow->m_initialClass.c_str());
    NEWHANDLE->m_resource->sendTitle(pWindow->m_initialTitle.c_str());
    NEWHANDLE->m_resource->sendDone();

    m_handles.push_back(NEWHANDLE);
}

SP<CForeignToplevelHandle> CForeignToplevelList::handleForWindow(PHLWINDOW pWindow) {
    std::erase_if(m_handles, [](const auto& wp) { return wp.expired(); });
    const auto IT = std::ranges::find_if(m_handles, [pWindow](const auto& h) { return h->window() == pWindow; });
    return IT == m_handles.end() ? SP<CForeignToplevelHandle>{} : IT->lock();
}

void CForeignToplevelList::onTitle(PHLWINDOW pWindow) {
    if UNLIKELY (m_finished)
        return;

    const auto H = handleForWindow(pWindow);
    if UNLIKELY (!H || H->m_closed)
        return;

    H->m_resource->sendTitle(pWindow->m_title.c_str());
    H->m_resource->sendDone();
}

void CForeignToplevelList::onClass(PHLWINDOW pWindow) {
    if UNLIKELY (m_finished)
        return;

    const auto H = handleForWindow(pWindow);
    if UNLIKELY (!H || H->m_closed)
        return;

    H->m_resource->sendAppId(pWindow->m_class.c_str());
    H->m_resource->sendDone();
}

void CForeignToplevelList::onUnmap(PHLWINDOW pWindow) {
    if UNLIKELY (m_finished)
        return;

    const auto H = handleForWindow(pWindow);
    if UNLIKELY (!H)
        return;

    H->m_resource->sendClosed();
    H->m_closed = true;
}

bool CForeignToplevelList::good() {
    return m_resource->resource();
}

CForeignToplevelProtocol::CForeignToplevelProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P = g_pHookSystem->hookDynamic("openWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        auto window = std::any_cast<PHLWINDOW>(data);

        if (!windowValidForForeign(window))
            return;

        for (auto const& m : m_managers) {
            m->onMap(window);
        }
    });

    static auto P1 = g_pHookSystem->hookDynamic("closeWindow", [this](void* self, SCallbackInfo& info, std::any data) {
        auto window = std::any_cast<PHLWINDOW>(data);

        if (!windowValidForForeign(window))
            return;

        for (auto const& m : m_managers) {
            m->onUnmap(window);
        }
    });

    static auto P2 = g_pHookSystem->hookDynamic("windowTitle", [this](void* self, SCallbackInfo& info, std::any data) {
        auto window = std::any_cast<PHLWINDOW>(data);

        if (!windowValidForForeign(window))
            return;

        for (auto const& m : m_managers) {
            m->onTitle(window);
        }
    });
}

void CForeignToplevelProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CForeignToplevelList>(makeShared<CExtForeignToplevelListV1>(client, ver, id))).get();

    if UNLIKELY (!RESOURCE->good()) {
        LOGM(ERR, "Couldn't create a foreign list");
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }
}

void CForeignToplevelProtocol::onManagerResourceDestroy(CForeignToplevelList* mgr) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == mgr; });
}

void CForeignToplevelProtocol::destroyHandle(CForeignToplevelHandle* handle) {
    std::erase_if(m_handles, [&](const auto& other) { return other.get() == handle; });
}

bool CForeignToplevelProtocol::windowValidForForeign(PHLWINDOW pWindow) {
    return validMapped(pWindow) && !pWindow->isX11OverrideRedirect();
}

PHLWINDOW CForeignToplevelProtocol::windowFromHandleResource(wl_resource* res) {
    auto data = (CForeignToplevelHandle*)(((CExtForeignToplevelHandleV1*)wl_resource_get_user_data(res))->data());
    return data ? data->window() : nullptr;
}
