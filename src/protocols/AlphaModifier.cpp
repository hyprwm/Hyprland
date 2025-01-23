#include "AlphaModifier.hpp"
#include "../desktop/WLSurface.hpp"
#include "../render/Renderer.hpp"
#include "alpha-modifier-v1.hpp"
#include "core/Compositor.hpp"

CAlphaModifier::CAlphaModifier(SP<CWpAlphaModifierSurfaceV1> resource, SP<CWLSurfaceResource> surface) : m_pSurface(surface) {
    setResource(std::move(resource));
}

bool CAlphaModifier::good() {
    return m_pResource->resource();
}

void CAlphaModifier::setResource(SP<CWpAlphaModifierSurfaceV1> resource) {
    m_pResource = std::move(resource);

    if UNLIKELY (!m_pResource->resource())
        return;

    m_pResource->setDestroy([this](CWpAlphaModifierSurfaceV1* resource) { destroy(); });
    m_pResource->setOnDestroy([this](CWpAlphaModifierSurfaceV1* resource) { destroy(); });

    m_pResource->setSetMultiplier([this](CWpAlphaModifierSurfaceV1* resource, uint32_t alpha) {
        if (!m_pSurface) {
            m_pResource->error(WP_ALPHA_MODIFIER_SURFACE_V1_ERROR_NO_SURFACE, "set_multiplier called for destroyed wl_surface");
            return;
        }

        m_fAlpha = alpha / (float)UINT32_MAX;
    });

    listeners.surfaceCommitted = m_pSurface->events.commit.registerListener([this](std::any data) {
        auto surface = CWLSurface::fromResource(m_pSurface.lock());

        if (surface && surface->m_fAlphaModifier != m_fAlpha) {
            surface->m_fAlphaModifier = m_fAlpha;
            auto box                  = surface->getSurfaceBoxGlobal();

            if (box.has_value())
                g_pHyprRenderer->damageBox(&*box);

            if (!m_pResource)
                PROTO::alphaModifier->destroyAlphaModifier(this);
        }
    });

    listeners.surfaceDestroyed = m_pSurface->events.destroy.registerListener([this](std::any data) {
        if (!m_pResource)
            PROTO::alphaModifier->destroyAlphaModifier(this);
    });
}

void CAlphaModifier::destroy() {
    m_pResource.reset();
    m_fAlpha = 1.F;

    if (!m_pSurface)
        PROTO::alphaModifier->destroyAlphaModifier(this);
}

CAlphaModifierProtocol::CAlphaModifierProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CAlphaModifierProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeUnique<CWpAlphaModifierV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CWpAlphaModifierV1* manager) { destroyManager(manager); });

    RESOURCE->setDestroy([this](CWpAlphaModifierV1* manager) { destroyManager(manager); });
    RESOURCE->setGetSurface([this](CWpAlphaModifierV1* manager, uint32_t id, wl_resource* surface) { getSurface(manager, id, CWLSurfaceResource::fromResource(surface)); });
}

void CAlphaModifierProtocol::destroyManager(CWpAlphaModifierV1* manager) {
    std::erase_if(m_vManagers, [&](const auto& p) { return p.get() == manager; });
}

void CAlphaModifierProtocol::destroyAlphaModifier(CAlphaModifier* modifier) {
    std::erase_if(m_mAlphaModifiers, [&](const auto& entry) { return entry.second.get() == modifier; });
}

void CAlphaModifierProtocol::getSurface(CWpAlphaModifierV1* manager, uint32_t id, SP<CWLSurfaceResource> surface) {
    CAlphaModifier* alphaModifier = nullptr;
    auto            iter          = std::find_if(m_mAlphaModifiers.begin(), m_mAlphaModifiers.end(), [&](const auto& entry) { return entry.second->m_pSurface == surface; });

    if (iter != m_mAlphaModifiers.end()) {
        if (iter->second->m_pResource) {
            LOGM(ERR, "AlphaModifier already present for surface {:x}", (uintptr_t)surface.get());
            manager->error(WP_ALPHA_MODIFIER_V1_ERROR_ALREADY_CONSTRUCTED, "AlphaModifier already present");
            return;
        } else {
            iter->second->setResource(makeShared<CWpAlphaModifierSurfaceV1>(manager->client(), manager->version(), id));
            alphaModifier = iter->second.get();
        }
    } else {
        alphaModifier = m_mAlphaModifiers.emplace(surface, makeUnique<CAlphaModifier>(makeShared<CWpAlphaModifierSurfaceV1>(manager->client(), manager->version(), id), surface))
                            .first->second.get();
    }

    if UNLIKELY (!alphaModifier->good()) {
        manager->noMemory();
        m_mAlphaModifiers.erase(surface);
    }
}
