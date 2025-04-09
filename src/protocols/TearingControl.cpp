#include "TearingControl.hpp"
#include "../managers/ProtocolManager.hpp"
#include "../desktop/Window.hpp"
#include "../Compositor.hpp"
#include "core/Compositor.hpp"
#include "../managers/HookSystemManager.hpp"

CTearingControl::CTearingControl(SP<CWpTearingControlV1> resource_, SP<CWLSurfaceResource> surf_) : resource(resource_), surface(surf_) {
    if UNLIKELY (!good())
        return;

    resource->setDestroy([this](CWpTearingControlV1* res) { PROTO::tearing->destroyResource(this); });
    resource->setOnDestroy([this](CWpTearingControlV1* res) { PROTO::tearing->destroyResource(this); });

    resource->setSetPresentationHint([this](CWpTearingControlV1* res, wpTearingControlV1PresentationHint hint_) {
        if UNLIKELY (!surface) {
            PROTO::tearing->destroyResource(this);
            return;
        }

        surface->pending.updated.tearing = true;
        surface->pending.tearingHint     = hint_ == WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC;
    });

    listeners.surfaceDestroyed = surface->events.destroy.registerListener([this](std::any data) { PROTO::tearing->destroyResource(this); });
}

CTearingControlProtocol::CTearingControlProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CTearingControlProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeUnique<CWpTearingControlManagerV1>(client, ver, id)).get();

    RESOURCE->setDestroy([](CWpTearingControlManagerV1* pMgr) { PROTO::tearing->destroyResource(pMgr); });
    RESOURCE->setOnDestroy([](CWpTearingControlManagerV1* pMgr) { PROTO::tearing->destroyResource(pMgr); });

    RESOURCE->setGetTearingControl([this](CWpTearingControlManagerV1* pMgr, uint32_t id, wl_resource* surface_) {
        auto PSURFACE = CWLSurfaceResource::fromResource(surface_);
        if UNLIKELY (std::ranges::find_if(m_vTearingControllers, [PSURFACE](const auto& other) { return other->surface == PSURFACE; }) != m_vTearingControllers.end()) {
            pMgr->error(WP_TEARING_CONTROL_MANAGER_V1_ERROR_TEARING_CONTROL_EXISTS, "surface already has tearing control");
            return;
        }

        const auto& CONTROLLER = m_vTearingControllers.emplace_back(makeUnique<CTearingControl>(makeShared<CWpTearingControlV1>(pMgr->client(), pMgr->version(), id), PSURFACE));

        if UNLIKELY (!CONTROLLER->good()) {
            pMgr->noMemory();
            m_vTearingControllers.pop_back();
            return;
        }
    });
}

void CTearingControlProtocol::destroyResource(CWpTearingControlManagerV1* resource) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == resource; });
}

void CTearingControlProtocol::destroyResource(CTearingControl* resource) {
    // https://wayland.app/protocols/tearing-control-v1#wp_tearing_control_v1:request:destroy
    // Destroy this surface tearing object and revert the presentation hint to vsync. The change will be applied on the next wl_surface.commit.
    if (resource->surface)
        resource->surface->pending.tearingHint = false;
    std::erase_if(m_vTearingControllers, [&](const auto& other) { return other.get() == resource; });
}

bool CTearingControl::good() {
    return resource->resource();
}
