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
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CWpTearingControlManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CWpTearingControlManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CWpTearingControlManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetTearingControl([this](CWpTearingControlManagerV1* pMgr, uint32_t id, wl_resource* surface) {
        this->onGetController(pMgr->client(), pMgr, id, CWLSurfaceResource::fromResource(surface));
    });
}

void CTearingControlProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CTearingControlProtocol::onGetController(wl_client* client, CWpTearingControlManagerV1* pMgr, uint32_t id, SP<CWLSurfaceResource> surf) {
    const auto CONTROLLER = m_tearingControllers.emplace_back(makeUnique<CTearingControl>(makeShared<CWpTearingControlV1>(client, pMgr->version(), id), surf)).get();

    if UNLIKELY (!CONTROLLER->good()) {
        pMgr->noMemory();
        m_tearingControllers.pop_back();
        return;
    }
}

void CTearingControlProtocol::onControllerDestroy(CTearingControl* control) {
    std::erase_if(m_tearingControllers, [control](const auto& other) { return other.get() == control; });
}

void CTearingControlProtocol::onWindowDestroy(PHLWINDOW pWindow) {
    for (auto const& c : m_tearingControllers) {
        if (c->m_window.lock() == pWindow)
            c->m_window.reset();
    }
}

//

CTearingControl::CTearingControl(SP<CWpTearingControlV1> resource_, SP<CWLSurfaceResource> surf_) : m_resource(resource_) {
    m_resource->setData(this);
    m_resource->setOnDestroy([this](CWpTearingControlV1* res) { PROTO::tearing->onControllerDestroy(this); });
    m_resource->setDestroy([this](CWpTearingControlV1* res) { PROTO::tearing->onControllerDestroy(this); });
    m_resource->setSetPresentationHint([this](CWpTearingControlV1* res, wpTearingControlV1PresentationHint hint) { this->onHint(hint); });

    for (auto const& w : g_pCompositor->m_windows) {
        if (w->m_wlSurface->resource() == surf_) {
            m_window = w;
            break;
        }
    }
}

void CTearingControl::onHint(wpTearingControlV1PresentationHint hint_) {
    m_hint = hint_;
    updateWindow();
}

void CTearingControl::updateWindow() {
    if UNLIKELY (m_window.expired())
        return;

    m_window->m_tearingHint = m_hint == WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC;
}

bool CTearingControl::good() {
    return m_resource->resource();
}
