#include "BackgroundEffect.hpp"
#include "../desktop/view/WLSurface.hpp"
#include "../render/Renderer.hpp"
#include "core/Compositor.hpp"
#include "ext-background-effect-v1.hpp"
#include <hyprutils/math/Region.hpp>
#include <wayland-server.h>

CBackgroundEffect::CBackgroundEffect(SP<CExtBackgroundEffectSurfaceV1> resource, SP<CWLSurfaceResource> surface) : m_surface(surface) {
    setResource(std::move(resource));
}

bool CBackgroundEffect::good() const {
    return m_resource->resource();
}

void CBackgroundEffect::setResource(SP<CExtBackgroundEffectSurfaceV1> resource) {
    m_resource = std::move(resource);

    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setDestroy([this](CExtBackgroundEffectSurfaceV1* resource) { destroy(); });
    m_resource->setOnDestroy([this](CExtBackgroundEffectSurfaceV1* resource) { destroy(); });

    m_resource->setSetBlurRegion([this](CExtBackgroundEffectSurfaceV1* resource, wl_resource* region) {
        if UNLIKELY (!m_surface) {
            m_resource->error(EXT_BACKGROUND_EFFECT_SURFACE_V1_ERROR_SURFACE_DESTROYED, "set_blur_region called for destroyed wl_surface");
            return;
        }

        if (!region) {
            m_blurRegion.clear();
            return;
        }

        m_blurRegion = CWLRegionResource::fromResource(region)->m_region;
    });

    m_listeners.surfaceCommitted = m_surface->m_events.commit.listen([this] {
        auto hlSurface = Desktop::View::CWLSurface::fromResource(m_surface.lock());

        if (!hlSurface)
            return;

        if (!m_resource) {
            // effect was destroyed, clear state on commit per spec
            hlSurface->m_hasBackgroundEffect = false;
            hlSurface->m_blurRegion.clear();
            auto box = hlSurface->getSurfaceBoxGlobal();
            if (box.has_value())
                g_pHyprRenderer->damageBox(*box);
            PROTO::backgroundEffect->destroyEffect(this);
            return;
        }

        hlSurface->m_hasBackgroundEffect = true;
        hlSurface->m_blurRegion          = m_blurRegion;
        auto box                         = hlSurface->getSurfaceBoxGlobal();

        if (box.has_value())
            g_pHyprRenderer->damageBox(*box);
    });

    m_listeners.surfaceDestroyed = m_surface->m_events.destroy.listen([this] {
        PROTO::backgroundEffect->destroyEffect(this);
    });
}

void CBackgroundEffect::destroy() {
    m_resource.reset();
    m_blurRegion.clear();
    // Per spec, effect removal is double-buffered: state clears on next wl_surface.commit.
    // The commit listener checks !m_resource and handles cleanup.
    // If the surface is already gone, clean up immediately.
    if (!m_surface)
        PROTO::backgroundEffect->destroyEffect(this);
}

CBackgroundEffectProtocol::CBackgroundEffectProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CBackgroundEffectProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    auto manager = m_managers.emplace_back(makeUnique<CExtBackgroundEffectManagerV1>(client, ver, id)).get();
    manager->setOnDestroy([this](CExtBackgroundEffectManagerV1* manager) { destroyManager(manager); });
    manager->setDestroy([this](CExtBackgroundEffectManagerV1* manager) { destroyManager(manager); });

    manager->sendCapabilities(EXT_BACKGROUND_EFFECT_MANAGER_V1_CAPABILITY_BLUR);

    manager->setGetBackgroundEffect(
        [this](CExtBackgroundEffectManagerV1* manager, uint32_t id, wl_resource* surface) { getBackgroundEffect(manager, id, CWLSurfaceResource::fromResource(surface)); });
}

void CBackgroundEffectProtocol::destroyManager(CExtBackgroundEffectManagerV1* manager) {
    std::erase_if(m_managers, [&](const auto& p) { return p.get() == manager; });
}

void CBackgroundEffectProtocol::destroyEffect(CBackgroundEffect* effect) {
    std::erase_if(m_effects, [&](const auto& entry) { return entry.second.get() == effect; });
}

void CBackgroundEffectProtocol::getBackgroundEffect(CExtBackgroundEffectManagerV1* manager, uint32_t id, SP<CWLSurfaceResource> surface) {
    CBackgroundEffect* effect = nullptr;
    auto               iter   = std::ranges::find_if(m_effects, [&](const auto& entry) { return entry.second->m_surface == surface; });

    if (iter != m_effects.end()) {
        if (iter->second->m_resource) {
            LOGM(Log::ERR, "BackgroundEffect already present for surface {:x}", (uintptr_t)surface.get());
            manager->error(EXT_BACKGROUND_EFFECT_MANAGER_V1_ERROR_BACKGROUND_EFFECT_EXISTS, "BackgroundEffect already present");
            return;
        } else {
            iter->second->setResource(makeShared<CExtBackgroundEffectSurfaceV1>(manager->client(), manager->version(), id));
            effect = iter->second.get();
        }
    } else {
        effect =
            m_effects.emplace(surface, makeUnique<CBackgroundEffect>(makeShared<CExtBackgroundEffectSurfaceV1>(manager->client(), manager->version(), id), surface)).first->second.get();
    }

    if UNLIKELY (!effect->good()) {
        manager->noMemory();
        m_effects.erase(surface);
        return;
    }

    auto hlSurface = Desktop::View::CWLSurface::fromResource(surface);
    if (hlSurface)
        hlSurface->m_hasBackgroundEffect = true;
}
