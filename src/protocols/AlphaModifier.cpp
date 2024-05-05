#include "AlphaModifier.hpp"
#include <algorithm>
#include "../desktop/WLSurface.hpp"
#include "../render/Renderer.hpp"

#define LOGM PROTO::alphaModifier->protoLog

CAlphaModifier::CAlphaModifier(SP<CWpAlphaModifierSurfaceV1> resource_, wlr_surface* surface_) : resource(resource_), pSurface(surface_) {
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

    hyprListener_surfaceDestroy.initCallback(
        &surface_->events.destroy, [this](void* owner, void* data) { onSurfaceDestroy(); }, this, "CAlphaModifier");

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
    hyprListener_surfaceDestroy.removeCallback();
}

bool CAlphaModifier::good() {
    return resource->resource();
}

wlr_surface* CAlphaModifier::getSurface() {
    return pSurface;
}

void CAlphaModifier::setSurfaceAlpha(float a) {
    CWLSurface* surf = CWLSurface::surfaceFromWlr(pSurface);

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
    hyprListener_surfaceDestroy.removeCallback();
    pSurface = nullptr;
}

CAlphaModifierProtocol::CAlphaModifierProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CAlphaModifierProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CWpAlphaModifierV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CWpAlphaModifierV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CWpAlphaModifierV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetSurface([this](CWpAlphaModifierV1* pMgr, uint32_t id, wl_resource* surface) { this->onGetSurface(pMgr, id, wlr_surface_from_resource(surface)); });
}

void CAlphaModifierProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CAlphaModifierProtocol::destroyModifier(CAlphaModifier* modifier) {
    if (modifier->getSurface())
        m_mAlphaModifiers.erase(modifier->getSurface());
    else {
        // find it first
        wlr_surface* deadptr = nullptr;
        for (auto& [k, v] : m_mAlphaModifiers) {
            if (v.get() == modifier) {
                deadptr = k;
                break;
            }
        }

        if (!deadptr) {
            LOGM(ERR, "CAlphaModifierProtocol::destroyModifier: dead resource but no deadptr???");
            return;
        }

        m_mAlphaModifiers.erase(deadptr);
    }
}

void CAlphaModifierProtocol::onGetSurface(CWpAlphaModifierV1* pMgr, uint32_t id, wlr_surface* surface) {
    if (m_mAlphaModifiers.contains(surface)) {
        LOGM(ERR, "AlphaModifier already present for surface {:x}", (uintptr_t)surface);
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