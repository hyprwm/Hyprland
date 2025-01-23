#include "HyprlandSurface.hpp"
#include "../desktop/WLSurface.hpp"
#include "../render/Renderer.hpp"
#include "core/Compositor.hpp"
#include "hyprland-surface-v1.hpp"

CHyprlandSurface::CHyprlandSurface(SP<CHyprlandSurfaceV1> resource, SP<CWLSurfaceResource> surface) : m_pSurface(surface) {
    setResource(std::move(resource));
}

bool CHyprlandSurface::good() const {
    return m_pResource->resource();
}

void CHyprlandSurface::setResource(SP<CHyprlandSurfaceV1> resource) {
    m_pResource = std::move(resource);

    if UNLIKELY (!m_pResource->resource())
        return;

    m_pResource->setDestroy([this](CHyprlandSurfaceV1* resource) { destroy(); });
    m_pResource->setOnDestroy([this](CHyprlandSurfaceV1* resource) { destroy(); });

    m_pResource->setSetOpacity([this](CHyprlandSurfaceV1* resource, uint32_t opacity) {
        if UNLIKELY (!m_pSurface) {
            m_pResource->error(HYPRLAND_SURFACE_V1_ERROR_NO_SURFACE, "set_opacity called for destroyed wl_surface");
            return;
        }

        auto fOpacity = wl_fixed_to_double(opacity);
        if UNLIKELY (fOpacity < 0.0 || fOpacity > 1.0) {
            m_pResource->error(HYPRLAND_SURFACE_V1_ERROR_OUT_OF_RANGE, "set_opacity called with an opacity value larger than 1.0 or smaller than 0.0.");
            return;
        }

        m_fOpacity = fOpacity;
    });

    listeners.surfaceCommitted = m_pSurface->events.commit.registerListener([this](std::any data) {
        auto surface = CWLSurface::fromResource(m_pSurface.lock());

        if (surface && surface->m_fOverallOpacity != m_fOpacity) {
            surface->m_fOverallOpacity = m_fOpacity;
            auto box                   = surface->getSurfaceBoxGlobal();

            if (box.has_value())
                g_pHyprRenderer->damageBox(&*box);

            if (!m_pResource)
                PROTO::hyprlandSurface->destroySurface(this);
        }
    });

    listeners.surfaceDestroyed = m_pSurface->events.destroy.registerListener([this](std::any data) {
        if (!m_pResource)
            PROTO::hyprlandSurface->destroySurface(this);
    });
}

void CHyprlandSurface::destroy() {
    m_pResource.reset();
    m_fOpacity = 1.F;

    if (!m_pSurface)
        PROTO::hyprlandSurface->destroySurface(this);
}

CHyprlandSurfaceProtocol::CHyprlandSurfaceProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CHyprlandSurfaceProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    auto manager = m_vManagers.emplace_back(makeUnique<CHyprlandSurfaceManagerV1>(client, ver, id)).get();
    manager->setOnDestroy([this](CHyprlandSurfaceManagerV1* manager) { destroyManager(manager); });

    manager->setDestroy([this](CHyprlandSurfaceManagerV1* manager) { destroyManager(manager); });
    manager->setGetHyprlandSurface(
        [this](CHyprlandSurfaceManagerV1* manager, uint32_t id, wl_resource* surface) { getSurface(manager, id, CWLSurfaceResource::fromResource(surface)); });
}

void CHyprlandSurfaceProtocol::destroyManager(CHyprlandSurfaceManagerV1* manager) {
    std::erase_if(m_vManagers, [&](const auto& p) { return p.get() == manager; });
}

void CHyprlandSurfaceProtocol::destroySurface(CHyprlandSurface* surface) {
    std::erase_if(m_mSurfaces, [&](const auto& entry) { return entry.second.get() == surface; });
}

void CHyprlandSurfaceProtocol::getSurface(CHyprlandSurfaceManagerV1* manager, uint32_t id, SP<CWLSurfaceResource> surface) {
    CHyprlandSurface* hyprlandSurface = nullptr;
    auto              iter            = std::find_if(m_mSurfaces.begin(), m_mSurfaces.end(), [&](const auto& entry) { return entry.second->m_pSurface == surface; });

    if (iter != m_mSurfaces.end()) {
        if (iter->second->m_pResource) {
            LOGM(ERR, "HyprlandSurface already present for surface {:x}", (uintptr_t)surface.get());
            manager->error(HYPRLAND_SURFACE_MANAGER_V1_ERROR_ALREADY_CONSTRUCTED, "HyprlandSurface already present");
            return;
        } else {
            iter->second->setResource(makeShared<CHyprlandSurfaceV1>(manager->client(), manager->version(), id));
            hyprlandSurface = iter->second.get();
        }
    } else {
        hyprlandSurface =
            m_mSurfaces.emplace(surface, makeUnique<CHyprlandSurface>(makeShared<CHyprlandSurfaceV1>(manager->client(), manager->version(), id), surface)).first->second.get();
    }

    if UNLIKELY (!hyprlandSurface->good()) {
        manager->noMemory();
        m_mSurfaces.erase(surface);
    }
}
