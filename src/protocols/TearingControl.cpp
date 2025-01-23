#include "TearingControl.hpp"
#include "../managers/ProtocolManager.hpp"
#include "../desktop/Window.hpp"
#include "../Compositor.hpp"
#include "core/Compositor.hpp"
#include "../managers/HookSystemManager.hpp"

CTearingControlProtocol::CTearingControlProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    static auto P =
        g_pHookSystem->hookDynamic("destroyWindow", [this](void* self, SCallbackInfo& info, std::any param) { this->onWindowDestroy(std::any_cast<PHLWINDOW>(param)); });
}

void CTearingControlProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeUnique<CWpTearingControlManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CWpTearingControlManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CWpTearingControlManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetTearingControl([this](CWpTearingControlManagerV1* pMgr, uint32_t id, wl_resource* surface) {
        this->onGetController(pMgr->client(), pMgr, id, CWLSurfaceResource::fromResource(surface));
    });
}

void CTearingControlProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CTearingControlProtocol::onGetController(wl_client* client, CWpTearingControlManagerV1* pMgr, uint32_t id, SP<CWLSurfaceResource> surf) {
    const auto CONTROLLER = m_vTearingControllers.emplace_back(makeUnique<CTearingControl>(makeShared<CWpTearingControlV1>(client, pMgr->version(), id), surf)).get();

    if UNLIKELY (!CONTROLLER->good()) {
        pMgr->noMemory();
        m_vTearingControllers.pop_back();
        return;
    }
}

void CTearingControlProtocol::onControllerDestroy(CTearingControl* control) {
    std::erase_if(m_vTearingControllers, [control](const auto& other) { return other.get() == control; });
}

void CTearingControlProtocol::onWindowDestroy(PHLWINDOW pWindow) {
    for (auto const& c : m_vTearingControllers) {
        if (c->pWindow.lock() == pWindow)
            c->pWindow.reset();
    }
}

//

CTearingControl::CTearingControl(SP<CWpTearingControlV1> resource_, SP<CWLSurfaceResource> surf_) : resource(resource_) {
    resource->setData(this);
    resource->setOnDestroy([this](CWpTearingControlV1* res) { PROTO::tearing->onControllerDestroy(this); });
    resource->setDestroy([this](CWpTearingControlV1* res) { PROTO::tearing->onControllerDestroy(this); });
    resource->setSetPresentationHint([this](CWpTearingControlV1* res, wpTearingControlV1PresentationHint hint) { this->onHint(hint); });

    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->m_pWLSurface->resource() == surf_) {
            pWindow = w;
            break;
        }
    }
}

void CTearingControl::onHint(wpTearingControlV1PresentationHint hint_) {
    hint = hint_;
    updateWindow();
}

void CTearingControl::updateWindow() {
    if UNLIKELY (pWindow.expired())
        return;

    pWindow->m_bTearingHint = hint == WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC;
}

bool CTearingControl::good() {
    return resource->resource();
}
