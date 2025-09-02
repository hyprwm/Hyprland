#include "ServerDecorationKDE.hpp"
#include "core/Compositor.hpp"

CServerDecorationKDE::CServerDecorationKDE(SP<COrgKdeKwinServerDecoration> resource_, SP<CWLSurfaceResource> surf_) : m_surf(surf_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setRelease([this](COrgKdeKwinServerDecoration* pMgr) { PROTO::serverDecorationKDE->destroyResource(this); });
    m_resource->setOnDestroy([this](COrgKdeKwinServerDecoration* pMgr) { PROTO::serverDecorationKDE->destroyResource(this); });
    m_resource->setRequestMode([this](COrgKdeKwinServerDecoration*, uint32_t mode) {
        auto sendMode = kdeModeOnRequestCSD(mode);
        m_resource->sendMode(sendMode);
        mostRecentlySent      = sendMode;
        mostRecentlyRequested = mode;
    });

    // we send this and ignore request_mode.
    auto sendMode = kdeDefaultModeCSD();
    m_resource->sendMode(sendMode);
    mostRecentlySent = sendMode;
}

bool CServerDecorationKDE::good() {
    return m_resource->resource();
}

uint32_t CServerDecorationKDE::kdeDefaultModeCSD() {
    return ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_SERVER;
}

uint32_t CServerDecorationKDE::kdeModeOnRequestCSD(uint32_t modeRequestedByClient) {
    return kdeDefaultModeCSD();
}

uint32_t CServerDecorationKDE::kdeModeOnReleaseCSD() {
    return kdeDefaultModeCSD();
}

CServerDecorationKDEProtocol::CServerDecorationKDEProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CServerDecorationKDEProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<COrgKdeKwinServerDecorationManager>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](COrgKdeKwinServerDecorationManager* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setCreate([this](COrgKdeKwinServerDecorationManager* pMgr, uint32_t id, wl_resource* pointer) { this->createDecoration(pMgr, id, pointer); });

    // send default mode of SSD, as Hyprland will never ask for CSD. Screw Gnome and GTK.
    RESOURCE->sendDefaultMode(kdeDefaultManagerModeCSD());
}

uint32_t CServerDecorationKDEProtocol::kdeDefaultManagerModeCSD() {
    return ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_SERVER;
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
