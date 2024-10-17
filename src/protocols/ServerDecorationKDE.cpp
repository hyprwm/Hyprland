#include "ServerDecorationKDE.hpp"
#include "core/Compositor.hpp"

CServerDecorationKDE::CServerDecorationKDE(SP<COrgKdeKwinServerDecoration> resource_, SP<CWLSurfaceResource> surf) : resource(resource_) {
    if (!good())
        return;

    resource->setRelease([this](COrgKdeKwinServerDecoration* pMgr) { PROTO::serverDecorationKDE->destroyResource(this); });
    resource->setOnDestroy([this](COrgKdeKwinServerDecoration* pMgr) { PROTO::serverDecorationKDE->destroyResource(this); });

    // we send this and ignore request_mode.
    resource->sendMode(ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_SERVER);
}

bool CServerDecorationKDE::good() {
    return resource->resource();
}

CServerDecorationKDEProtocol::CServerDecorationKDEProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CServerDecorationKDEProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<COrgKdeKwinServerDecorationManager>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](COrgKdeKwinServerDecorationManager* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setCreate([this](COrgKdeKwinServerDecorationManager* pMgr, uint32_t id, wl_resource* pointer) { this->createDecoration(pMgr, id, pointer); });

    // send default mode of SSD, as Hyprland will never ask for CSD. Screw Gnome and GTK.
    RESOURCE->sendDefaultMode(ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_SERVER);
}

void CServerDecorationKDEProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CServerDecorationKDEProtocol::destroyResource(CServerDecorationKDE* hayperlaaaand) {
    std::erase_if(m_vDecos, [&](const auto& other) { return other.get() == hayperlaaaand; });
}

void CServerDecorationKDEProtocol::createDecoration(COrgKdeKwinServerDecorationManager* pMgr, uint32_t id, wl_resource* surf) {
    const auto CLIENT = pMgr->client();
    const auto RESOURCE =
        m_vDecos.emplace_back(std::make_unique<CServerDecorationKDE>(makeShared<COrgKdeKwinServerDecoration>(CLIENT, pMgr->version(), id), CWLSurfaceResource::fromResource(surf)))
            .get();

    if (!RESOURCE->good()) {
        pMgr->noMemory();
        m_vDecos.pop_back();
        return;
    }
}
