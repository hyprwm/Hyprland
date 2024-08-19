#include "AlphaModifier.hpp"
#include <algorithm>
#include "../desktop/WLSurface.hpp"
#include "../render/Renderer.hpp"
#include "core/Compositor.hpp"

CAlphaModifier::CAlphaModifier(SP<CWpAlphaModifierSurfaceV1> resource_, SP<CWLSurfaceResource> surface_) : resource(resource_), pSurface(surface_) {
    if (!resource->resource())
        return;

    resource->setDestroy([this](CWpAlphaModifierSurfaceV1* pMgr) {
        PROTO::alphaModifier->destroyModifier(this);
        setSurfaceAlpha(1.F);
    });
    resource->setOnDestroy([this](CWpAlphaModifierSurfaceV1* pMgr) {
        PROTO::alphaModifier->destroyModifier(this);
        setSurfaceAlpha(1.F);
    });

    listeners.destroySurface = pSurface->events.destroy.registerListener([this](std::any d) { onSurfaceDestroy(); });

    resource->setSetMultiplier([this](CWpAlphaModifierSurfaceV1* mod, uint32_t alpha) {
        if (!pSurface) {
            LOGM(ERR, "Resource {:x} tried to setMultiplier but surface is gone", (uintptr_t)mod->resource());
            mod->error(WP_ALPHA_MODIFIER_SURFACE_V1_ERROR_NO_SURFACE, "Surface is gone");
            return;
        }

        float a = alpha / (float)UINT32_MAX;

        setSurfaceAlpha(a);
    });
}

CAlphaModifier::~CAlphaModifier() {
    ;
}

bool CAlphaModifier::good() {
    return resource->resource();
}

SP<CWLSurfaceResource> CAlphaModifier::getSurface() {
    return pSurface.lock();
}

void CAlphaModifier::setSurfaceAlpha(float a) {
    auto surf = CWLSurface::fromResource(pSurface.lock());

    if (!surf) {
        LOGM(ERR, "CAlphaModifier::setSurfaceAlpha: No CWLSurface for given surface??");
        return;
    }

    surf->m_pAlphaModifier = a;

    auto SURFBOX = surf->getSurfaceBoxGlobal();
    if (SURFBOX.has_value())
        g_pHyprRenderer->damageBox(&*SURFBOX);
}

void CAlphaModifier::onSurfaceDestroy() {
    pSurface.reset();
}

CAlphaModifierProtocol::CAlphaModifierProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CAlphaModifierProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CWpAlphaModifierV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CWpAlphaModifierV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CWpAlphaModifierV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetSurface([this](CWpAlphaModifierV1* pMgr, uint32_t id, wl_resource* surface) { this->onGetSurface(pMgr, id, CWLSurfaceResource::fromResource(surface)); });
}

void CAlphaModifierProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CAlphaModifierProtocol::destroyModifier(CAlphaModifier* modifier) {
    std::erase_if(m_mAlphaModifiers, [](const auto& e) { return e.first.expired(); });
}

void CAlphaModifierProtocol::onGetSurface(CWpAlphaModifierV1* pMgr, uint32_t id, SP<CWLSurfaceResource> surface) {
    if (std::find_if(m_mAlphaModifiers.begin(), m_mAlphaModifiers.end(), [surface](const auto& e) { return e.first == surface; }) != m_mAlphaModifiers.end()) {
        LOGM(ERR, "AlphaModifier already present for surface {:x}", (uintptr_t)surface.get());
        pMgr->error(WP_ALPHA_MODIFIER_V1_ERROR_ALREADY_CONSTRUCTED, "AlphaModifier already present");
        return;
    }

    const auto RESOURCE = m_mAlphaModifiers.emplace(surface, std::make_unique<CAlphaModifier>(makeShared<CWpAlphaModifierSurfaceV1>(pMgr->client(), pMgr->version(), id), surface))
                              .first->second.get();

    if (!RESOURCE->good()) {
        pMgr->noMemory();
        m_mAlphaModifiers.erase(surface);
        return;
    }
}
