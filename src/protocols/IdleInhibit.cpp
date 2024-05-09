#include "IdleInhibit.hpp"

CIdleInhibitor::CIdleInhibitor(SP<CIdleInhibitorResource> resource_, wlr_surface* surf_) : resource(resource_), surface(surf_) {
    ;
}

CIdleInhibitorResource::CIdleInhibitorResource(SP<CZwpIdleInhibitorV1> resource_, wlr_surface* surface_) : resource(resource_), surface(surface_) {
    hyprListener_surfaceDestroy.initCallback(
        &surface->events.destroy,
        [this](void* owner, void* data) {
            surface = nullptr;
            hyprListener_surfaceDestroy.removeCallback();
            destroySent = true;
            events.destroy.emit();
        },
        this, "CIdleInhibitorResource");

    resource->setOnDestroy([this](CZwpIdleInhibitorV1* p) { PROTO::idleInhibit->removeInhibitor(this); });
    resource->setDestroy([this](CZwpIdleInhibitorV1* p) { PROTO::idleInhibit->removeInhibitor(this); });
}

CIdleInhibitorResource::~CIdleInhibitorResource() {
    hyprListener_surfaceDestroy.removeCallback();
    if (!destroySent)
        events.destroy.emit();
}

CIdleInhibitProtocol::CIdleInhibitProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CIdleInhibitProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [res](const auto& other) { return other->resource() == res; });
}

void CIdleInhibitProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CZwpIdleInhibitManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpIdleInhibitManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwpIdleInhibitManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setCreateInhibitor(
        [this](CZwpIdleInhibitManagerV1* pMgr, uint32_t id, wl_resource* surface) { this->onCreateInhibitor(pMgr, id, wlr_surface_from_resource(surface)); });
}

void CIdleInhibitProtocol::removeInhibitor(CIdleInhibitorResource* resource) {
    std::erase_if(m_vInhibitors, [resource](const auto& el) { return el.get() == resource; });
}

void CIdleInhibitProtocol::onCreateInhibitor(CZwpIdleInhibitManagerV1* pMgr, uint32_t id, wlr_surface* surface) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_vInhibitors.emplace_back(makeShared<CIdleInhibitorResource>(makeShared<CZwpIdleInhibitorV1>(CLIENT, pMgr->version(), id), surface));

    RESOURCE->inhibitor = makeShared<CIdleInhibitor>(RESOURCE, surface);
    events.newIdleInhibitor.emit(RESOURCE->inhibitor);
}