#include "TearingControl.hpp"
#include "../managers/ProtocolManager.hpp"
#include "../desktop/Window.hpp"
#include "../Compositor.hpp"
#include "core/Compositor.hpp"
#include "../managers/HookSystemManager.hpp"

CTearingControl::CTearingControl(SP<CWpTearingControlV1> resource_, SP<CWLSurfaceResource> surf_) : m_resource(resource_), m_surface(surf_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CWpTearingControlV1* res) { PROTO::tearing->destroyResource(this); });
    m_resource->setOnDestroy([this](CWpTearingControlV1* res) { PROTO::tearing->destroyResource(this); });

    m_resource->setSetPresentationHint([this](CWpTearingControlV1* res, wpTearingControlV1PresentationHint hint_) {
        if UNLIKELY (!m_surface) {
            PROTO::tearing->destroyResource(this);
            return;
        }

        m_surface->m_pending.updated.bits.tearing = true;
        m_surface->m_pending.tearingHint          = hint_ == WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC;
    });

    m_listeners.surfaceDestroyed = m_surface->m_events.destroy.registerListener([this](std::any data) { PROTO::tearing->destroyResource(this); });
}

CTearingControlProtocol::CTearingControlProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CTearingControlProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CWpTearingControlManagerV1>(client, ver, id)).get();

    RESOURCE->setDestroy([](CWpTearingControlManagerV1* pMgr) { PROTO::tearing->destroyResource(pMgr); });
    RESOURCE->setOnDestroy([](CWpTearingControlManagerV1* pMgr) { PROTO::tearing->destroyResource(pMgr); });

    RESOURCE->setGetTearingControl([this](CWpTearingControlManagerV1* pMgr, uint32_t id, wl_resource* surface_) {
        auto PSURFACE = CWLSurfaceResource::fromResource(surface_);
        if UNLIKELY (std::ranges::find_if(m_tearingControllers, [PSURFACE](const auto& other) { return other->m_surface == PSURFACE; }) != m_tearingControllers.end()) {
            pMgr->error(WP_TEARING_CONTROL_MANAGER_V1_ERROR_TEARING_CONTROL_EXISTS, "surface already has tearing control");
            return;
        }

        const auto& CONTROLLER = m_tearingControllers.emplace_back(makeUnique<CTearingControl>(makeShared<CWpTearingControlV1>(pMgr->client(), pMgr->version(), id), PSURFACE));

        if UNLIKELY (!CONTROLLER->good()) {
            pMgr->noMemory();
            m_tearingControllers.pop_back();
            return;
        }
    });
}

void CTearingControlProtocol::destroyResource(CWpTearingControlManagerV1* resource) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == resource; });
}

void CTearingControlProtocol::destroyResource(CTearingControl* resource) {
    // https://wayland.app/protocols/tearing-control-v1#wp_tearing_control_v1:request:destroy
    // Destroy this surface tearing object and revert the presentation hint to vsync. The change will be applied on the next wl_surface.commit.
    if (resource->m_surface)
        resource->m_surface->m_pending.tearingHint = false;
    std::erase_if(m_tearingControllers, [&](const auto& other) { return other.get() == resource; });
}

bool CTearingControl::good() {
    return m_resource->resource();
}
