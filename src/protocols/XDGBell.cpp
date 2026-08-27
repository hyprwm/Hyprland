#include "XDGBell.hpp"
#include "../bell/BellPlayer.hpp"
#include "./core/Compositor.hpp"
#include "../desktop/state/ViewState.hpp"
#include "../desktop/state/ViewQuery.hpp"
#include "../event/EventBus.hpp"
#include "core/Compositor.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../ipc/s2/S2.hpp"
#include <format>

CXDGSystemBellManagerResource::CXDGSystemBellManagerResource(UP<CXdgSystemBellV1>&& resource) : m_resource(std::move(resource)) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CXdgSystemBellV1*) { PROTO::xdgBell->destroyResource(this); });
    m_resource->setOnDestroy([this](CXdgSystemBellV1*) { PROTO::xdgBell->destroyResource(this); });

    m_resource->setRing([](CXdgSystemBellV1*, wl_resource* surface) {
        const auto           WINDOW = Desktop::viewState()->query().surface(CWLSurfaceResource::fromResource(surface)).type(Desktop::View::VIEW_TYPE_WINDOW).runWindow();

        Event::SCallbackInfo info;
        Event::bus()->m_events.window.bell.emit(WINDOW, info);
        if (info.cancelled)
            return;

        IPC::Socket2::sock()->postEvent({.event = "bell", .data = WINDOW ? std::format("{:x}", rc<uintptr_t>(WINDOW.get())) : ""});
        Bell::player()->play();
    });
}

bool CXDGSystemBellManagerResource::good() {
    return m_resource->resource();
}

CXDGSystemBellProtocol::CXDGSystemBellProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CXDGSystemBellProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = WP<CXDGSystemBellManagerResource>{m_managers.emplace_back(makeUnique<CXDGSystemBellManagerResource>(makeUnique<CXdgSystemBellV1>(client, ver, id)))};

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        return;
    }
}

void CXDGSystemBellProtocol::destroyResource(CXDGSystemBellManagerResource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == res; });
}
