#include "TearingControl.hpp"
#include "tearing-control-v1-protocol.h"
#include "../managers/ProtocolManager.hpp"
#include "../desktop/Window.hpp"
#include "../Compositor.hpp"

static void destroyManager(wl_client* client, wl_resource* resource) {
    RESOURCE_OR_BAIL(PRESOURCE);
    reinterpret_cast<CTearingControlProtocol*>(PRESOURCE->data())->onManagerResourceDestroy(resource);
}

static void getTearingControl(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* surface) {
    RESOURCE_OR_BAIL(PRESOURCE);
    reinterpret_cast<CTearingControlProtocol*>(PRESOURCE->data())->onGetController(client, resource, id, wlr_surface_from_resource(surface));
}

//

CTearingControlProtocol::CTearingControlProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    g_pHookSystem->hookDynamic("destroyWindow", [this](void* self, SCallbackInfo& info, std::any param) { this->onWindowDestroy(std::any_cast<CWindow*>(param)); });
}

static const struct wp_tearing_control_manager_v1_interface MANAGER_IMPL = {
    .destroy             = ::destroyManager,
    .get_tearing_control = ::getTearingControl,
};

void CTearingControlProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CWaylandResource>(client, &wp_tearing_control_manager_v1_interface, ver, id)).get();

    if (!RESOURCE->good()) {
        Debug::log(LOG, "Couldn't bind TearingControlMgr");
        return;
    }

    RESOURCE->setImplementation(&MANAGER_IMPL, nullptr);
    RESOURCE->setData(this);
}

void CTearingControlProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CTearingControlProtocol::onGetController(wl_client* client, wl_resource* resource, uint32_t id, wlr_surface* surf) {
    const auto CONTROLLER = m_vTearingControllers
                                .emplace_back(std::make_unique<CTearingControl>(
                                    std::make_shared<CWaylandResource>(client, &wp_tearing_control_v1_interface, wl_resource_get_version(resource), id), surf))
                                .get();

    if (!CONTROLLER->good()) {
        m_vTearingControllers.pop_back();
        return;
    }
}

void CTearingControlProtocol::onControllerDestroy(CTearingControl* control) {
    std::erase_if(m_vTearingControllers, [control](const auto& other) { return other.get() == control; });
}

void CTearingControlProtocol::onWindowDestroy(CWindow* pWindow) {
    for (auto& c : m_vTearingControllers) {
        if (c->pWindow == pWindow)
            c->pWindow = nullptr;
    }
}

//

static void destroyController(wl_client* client, wl_resource* resource) {
    RESOURCE_OR_BAIL(PRESOURCE);
    PROTO::tearing->onControllerDestroy(reinterpret_cast<CTearingControl*>(PRESOURCE->data()));
}

static void setPresentationHint(wl_client* client, wl_resource* resource, uint32_t hint) {
    RESOURCE_OR_BAIL(PRESOURCE);
    reinterpret_cast<CTearingControl*>(PRESOURCE->data())->onHint(hint);
}

static const struct wp_tearing_control_v1_interface CONTROLLER_IMPL = {
    .set_presentation_hint = ::setPresentationHint,
    .destroy               = ::destroyController,
};

CTearingControl::CTearingControl(SP<CWaylandResource> resource_, wlr_surface* surf_) : resource(resource_) {
    resource->setImplementation(&CONTROLLER_IMPL, nullptr);
    resource->setData(this);
    resource->setOnDestroyHandler([](CWaylandResource* res) { PROTO::tearing->onControllerDestroy(reinterpret_cast<CTearingControl*>(res->data())); });

    pWindow = g_pCompositor->getWindowFromSurface(surf_);
}

void CTearingControl::onHint(uint32_t hint_) {
    hint = hint_ == WP_TEARING_CONTROL_V1_PRESENTATION_HINT_VSYNC ? TEARING_VSYNC : TEARING_ASYNC;
    updateWindow();
}

void CTearingControl::updateWindow() {
    if (!pWindow)
        return;

    pWindow->m_bTearingHint = hint == TEARING_ASYNC;
}

bool CTearingControl::good() {
    return resource->good();
}
