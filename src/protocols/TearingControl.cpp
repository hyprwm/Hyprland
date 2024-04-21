#include "TearingControl.hpp"
#include "../managers/ProtocolManager.hpp"
#include "../desktop/Window.hpp"
#include "../Compositor.hpp"

CTearingControlProtocol::CTearingControlProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P = g_pHookSystem->hookDynamic("destroyWindow", [this](void* self, SCallbackInfo& info, std::any param) { this->onWindowDestroy(std::any_cast<CWindow*>(param)); });
}

void CTearingControlProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CWpTearingControlManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CWpTearingControlManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CWpTearingControlManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetTearingControl([this](CWpTearingControlManagerV1* pMgr, uint32_t id, wl_resource* surface) {
        this->onGetController(wl_resource_get_client(pMgr->resource()), pMgr->resource(), id, wlr_surface_from_resource(surface));
    });
}

void CTearingControlProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CTearingControlProtocol::onGetController(wl_client* client, wl_resource* resource, uint32_t id, wlr_surface* surf) {
    const auto CONTROLLER =
        m_vTearingControllers.emplace_back(std::make_unique<CTearingControl>(std::make_shared<CWpTearingControlV1>(client, wl_resource_get_version(resource), id), surf)).get();

    if (!CONTROLLER->good()) {
        wl_resource_post_no_memory(resource);
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

CTearingControl::CTearingControl(SP<CWpTearingControlV1> resource_, wlr_surface* surf_) : resource(resource_) {
    resource->setData(this);
    resource->setOnDestroy([this](CWpTearingControlV1* res) { PROTO::tearing->onControllerDestroy(this); });
    resource->setDestroy([this](CWpTearingControlV1* res) { PROTO::tearing->onControllerDestroy(this); });
    resource->setSetPresentationHint([this](CWpTearingControlV1* res, wpTearingControlV1PresentationHint hint) { this->onHint(hint); });

    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_pWLSurface.wlr() == surf_) {
            pWindow = w.get();
            break;
        }
    }
}

void CTearingControl::onHint(wpTearingControlV1PresentationHint hint_) {
    hint = hint_;
    updateWindow();
}

void CTearingControl::updateWindow() {
    if (!pWindow)
        return;

    pWindow->m_bTearingHint = hint == WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC;
}

bool CTearingControl::good() {
    return resource->resource();
}
