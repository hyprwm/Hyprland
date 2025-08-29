#include "ServerDecorationKDE.hpp"
#include "core/Compositor.hpp"

// 'csd' can be nullptr in the 'bindManager' case
orgKdeKwinServerDecorationManagerMode kdeDefaultModeCSD(CServerDecorationKDE* csd) {
    return ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_SERVER;
}

orgKdeKwinServerDecorationManagerMode kdeModeOnRequestCSD(CServerDecorationKDE* csd, uint32_t modeRequestedByClient) {
    return kdeDefaultModeCSD(csd);
}

orgKdeKwinServerDecorationManagerMode kdeModeOnReleaseCSD(CServerDecorationKDE* csd) {
    return kdeDefaultModeCSD(csd);
}

CServerDecorationKDE::CServerDecorationKDE(SP<COrgKdeKwinServerDecoration> resource_, SP<CWLSurfaceResource> surf_) : m_surf(surf_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setRelease([this](COrgKdeKwinServerDecoration* pMgr) { PROTO::serverDecorationKDE->destroyResource(this); });
    m_resource->setOnDestroy([this](COrgKdeKwinServerDecoration* pMgr) { PROTO::serverDecorationKDE->destroyResource(this); });
    m_resource->setRequestMode([this](COrgKdeKwinServerDecoration*, uint32_t mode) { m_resource->sendMode(kdeModeOnRequestCSD(this, mode)); });
    m_resource->setRelease([this](COrgKdeKwinServerDecoration* pMgr) { m_resource->sendMode(kdeModeOnReleaseCSD(this)); });

    // we send this and ignore request_mode.
    m_resource->sendMode(kdeDefaultModeCSD(this));
}

bool CServerDecorationKDE::good() {
    return m_resource->resource();
}

CServerDecorationKDEProtocol::CServerDecorationKDEProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CServerDecorationKDEProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<COrgKdeKwinServerDecorationManager>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](COrgKdeKwinServerDecorationManager* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setCreate([this](COrgKdeKwinServerDecorationManager* pMgr, uint32_t id, wl_resource* pointer) { this->createDecoration(pMgr, id, pointer); });

    // send default mode of SSD, as Hyprland will never ask for CSD. Screw Gnome and GTK.
    RESOURCE->sendDefaultMode(kdeDefaultModeCSD(nullptr));
}

void CServerDecorationKDEProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CServerDecorationKDEProtocol::destroyResource(CServerDecorationKDE* hayperlaaaand) {
    std::erase_if(m_decos, [&](const auto& other) { return other.get() == hayperlaaaand; });
}

void CServerDecorationKDEProtocol::createDecoration(COrgKdeKwinServerDecorationManager* pMgr, uint32_t id, wl_resource* surf) {
    const auto CLIENT = pMgr->client();
    const auto RESOURCE =
        m_decos.emplace_back(makeUnique<CServerDecorationKDE>(makeShared<COrgKdeKwinServerDecoration>(CLIENT, pMgr->version(), id), CWLSurfaceResource::fromResource(surf))).get();

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_decos.pop_back();
        return;
    }
}
