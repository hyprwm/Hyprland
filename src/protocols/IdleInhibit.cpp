#include "IdleInhibit.hpp"
#include "core/Compositor.hpp"

CIdleInhibitor::CIdleInhibitor(SP<CIdleInhibitorResource> resource_, SP<CWLSurfaceResource> surf_) : resource(resource_), surface(surf_) {
    ;
}

CIdleInhibitorResource::CIdleInhibitorResource(SP<CZwpIdleInhibitorV1> resource_, SP<CWLSurfaceResource> surface_) : resource(resource_), surface(surface_) {
    listeners.destroySurface = surface->m_events.destroy.registerListener([this](std::any d) {
        surface.reset();
        listeners.destroySurface.reset();
        destroySent = true;
        events.destroy.emit();
    });

    resource->setOnDestroy([this](CZwpIdleInhibitorV1* p) { PROTO::idleInhibit->removeInhibitor(this); });
    resource->setDestroy([this](CZwpIdleInhibitorV1* p) { PROTO::idleInhibit->removeInhibitor(this); });
}

CIdleInhibitorResource::~CIdleInhibitorResource() {
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
    const auto RESOURCE = m_vManagers.emplace_back(makeUnique<CZwpIdleInhibitManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpIdleInhibitManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwpIdleInhibitManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setCreateInhibitor(
        [this](CZwpIdleInhibitManagerV1* pMgr, uint32_t id, wl_resource* surface) { this->onCreateInhibitor(pMgr, id, CWLSurfaceResource::fromResource(surface)); });
}

void CIdleInhibitProtocol::removeInhibitor(CIdleInhibitorResource* resource) {
    std::erase_if(m_vInhibitors, [resource](const auto& el) { return el.get() == resource; });
}

void CIdleInhibitProtocol::onCreateInhibitor(CZwpIdleInhibitManagerV1* pMgr, uint32_t id, SP<CWLSurfaceResource> surface) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_vInhibitors.emplace_back(makeShared<CIdleInhibitorResource>(makeShared<CZwpIdleInhibitorV1>(CLIENT, pMgr->version(), id), surface));

    RESOURCE->inhibitor = makeShared<CIdleInhibitor>(RESOURCE, surface);
    events.newIdleInhibitor.emit(RESOURCE->inhibitor);
}