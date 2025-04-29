#include "XDGBell.hpp"
#include "XDGShell.hpp"
#include "../desktop/Window.hpp"
#include "../managers/EventManager.hpp"
#include "../Compositor.hpp"

CXDGSystemBellManagerResource::CXDGSystemBellManagerResource(UP<CXdgSystemBellV1>&& resource) : m_resource(std::move(resource)) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CXdgSystemBellV1* r) { PROTO::xdgBell->destroyResource(this); });
    m_resource->setOnDestroy([this](CXdgSystemBellV1* r) { PROTO::xdgBell->destroyResource(this); });

    resource->setRing([](CXdgSystemBellV1* r, wl_resource* toplevel) {
        auto TOPLEVEL = CXDGToplevelResource::fromResource(toplevel);

        if (!TOPLEVEL) {
            g_pEventManager->postEvent(SHyprIPCEvent{
                .event = "bell",
                .data  = "",
            });
            return;
        }

        for (const auto& w : g_pCompositor->m_windows) {
            if (!w->m_isMapped || w->m_isX11 || !w->m_xdgSurface)
                continue;

            if (w->m_xdgSurface == TOPLEVEL->owner) {
                g_pEventManager->postEvent(SHyprIPCEvent{
                    .event = "bell",
                    .data  = std::format("{:x}", (uintptr_t)w.get()),
                });
                return;
            }
        }

        g_pEventManager->postEvent(SHyprIPCEvent{
            .event = "bell",
            .data  = "",
        });
    });
}

bool CXDGSystemBellManagerResource::good() {
    return m_resource->resource();
}

CXDGSystemBellProtocol::CXDGSystemBellProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CXDGSystemBellProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = WP<CXDGSystemBellManagerResource>{m_vManagers.emplace_back(makeUnique<CXDGSystemBellManagerResource>(makeUnique<CXdgSystemBellV1>(client, ver, id)))};

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        return;
    }
}

void CXDGSystemBellProtocol::destroyResource(CXDGSystemBellManagerResource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == res; });
}
