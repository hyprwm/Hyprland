#include "IdleInhibit.hpp"
#include "core/Compositor.hpp"

CIdleInhibitor::CIdleInhibitor(SP<CIdleInhibitorResource> resource_, SP<CWLSurfaceResource> surf_) : m_resource(resource_), m_surface(surf_) {
    ;
}

CIdleInhibitorResource::CIdleInhibitorResource(SP<CZwpIdleInhibitorV1> resource_, SP<CWLSurfaceResource> surface_) : m_resource(resource_), m_surface(surface_) {
    m_listeners.destroySurface = m_surface->m_events.destroy.listen([this] {
        m_surface.reset();
        m_listeners.destroySurface.reset();
        m_destroySent = true;
        m_events.destroy.emit();
    });

    m_resource->setOnDestroy([this](CZwpIdleInhibitorV1* p) { PROTO::idleInhibit->removeInhibitor(this); });
    m_resource->setDestroy([this](CZwpIdleInhibitorV1* p) { PROTO::idleInhibit->removeInhibitor(this); });
}

CIdleInhibitorResource::~CIdleInhibitorResource() {
    if (!m_destroySent)
        m_events.destroy.emit();
}

CIdleInhibitProtocol::CIdleInhibitProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CIdleInhibitProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [res](const auto& other) { return other->resource() == res; });
}

void CIdleInhibitProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CZwpIdleInhibitManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpIdleInhibitManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwpIdleInhibitManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setCreateInhibitor(
        [this](CZwpIdleInhibitManagerV1* pMgr, uint32_t id, wl_resource* surface) { this->onCreateInhibitor(pMgr, id, CWLSurfaceResource::fromResource(surface)); });
}

void CIdleInhibitProtocol::removeInhibitor(CIdleInhibitorResource* resource) {
    std::erase_if(m_inhibitors, [resource](const auto& el) { return el.get() == resource; });
}

void CIdleInhibitProtocol::onCreateInhibitor(CZwpIdleInhibitManagerV1* pMgr, uint32_t id, SP<CWLSurfaceResource> surface) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_inhibitors.emplace_back(makeShared<CIdleInhibitorResource>(makeShared<CZwpIdleInhibitorV1>(CLIENT, pMgr->version(), id), surface));

    RESOURCE->m_inhibitor = makeShared<CIdleInhibitor>(RESOURCE, surface);
    m_events.newIdleInhibitor.emit(RESOURCE->m_inhibitor);
}
