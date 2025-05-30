#include "HyprlandSurface.hpp"
#include "../desktop/WLSurface.hpp"
#include "../render/Renderer.hpp"
#include "core/Compositor.hpp"
#include "hyprland-surface-v1.hpp"
#include <hyprutils/math/Region.hpp>
#include <wayland-server.h>

CHyprlandSurface::CHyprlandSurface(SP<CHyprlandSurfaceV1> resource, SP<CWLSurfaceResource> surface) : m_surface(surface) {
    setResource(std::move(resource));
}

bool CHyprlandSurface::good() const {
    return m_resource->resource();
}

void CHyprlandSurface::setResource(SP<CHyprlandSurfaceV1> resource) {
    m_resource = std::move(resource);

    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setDestroy([this](CHyprlandSurfaceV1* resource) { destroy(); });
    m_resource->setOnDestroy([this](CHyprlandSurfaceV1* resource) { destroy(); });

    m_resource->setSetOpacity([this](CHyprlandSurfaceV1* resource, uint32_t opacity) {
        if UNLIKELY (!m_surface) {
            m_resource->error(HYPRLAND_SURFACE_V1_ERROR_NO_SURFACE, "set_opacity called for destroyed wl_surface");
            return;
        }

        auto fOpacity = wl_fixed_to_double(opacity);
        if UNLIKELY (fOpacity < 0.0 || fOpacity > 1.0) {
            m_resource->error(HYPRLAND_SURFACE_V1_ERROR_OUT_OF_RANGE, "set_opacity called with an opacity value larger than 1.0 or smaller than 0.0.");
            return;
        }

        m_opacity = fOpacity;
    });

    m_resource->setSetVisibleRegion([this](CHyprlandSurfaceV1* resource, wl_resource* region) {
        if (!region) {
            if (!m_visibleRegion.empty())
                m_visibleRegionChanged = true;

            m_visibleRegion.clear();
            return;
        }

        m_visibleRegionChanged = true;
        m_visibleRegion        = CWLRegionResource::fromResource(region)->m_region;
    });

    m_listeners.surfaceCommitted = m_surface->m_events.commit.registerListener([this](std::any data) {
        auto surface = CWLSurface::fromResource(m_surface.lock());

        if (surface && (surface->m_overallOpacity != m_opacity || m_visibleRegionChanged)) {
            surface->m_overallOpacity = m_opacity;
            surface->m_visibleRegion  = m_visibleRegion;
            auto box                  = surface->getSurfaceBoxGlobal();

            if (box.has_value())
                g_pHyprRenderer->damageBox(*box);

            if (!m_resource)
                PROTO::hyprlandSurface->destroySurface(this);
        }
    });

    m_listeners.surfaceDestroyed = m_surface->m_events.destroy.registerListener([this](std::any data) {
        if (!m_resource)
            PROTO::hyprlandSurface->destroySurface(this);
    });
}

void CHyprlandSurface::destroy() {
    m_resource.reset();
    m_opacity = 1.F;

    if (!m_visibleRegion.empty())
        m_visibleRegionChanged = true;

    m_visibleRegion.clear();

    if (!m_surface)
        PROTO::hyprlandSurface->destroySurface(this);
}

CHyprlandSurfaceProtocol::CHyprlandSurfaceProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CHyprlandSurfaceProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    auto manager = m_managers.emplace_back(makeUnique<CHyprlandSurfaceManagerV1>(client, ver, id)).get();
    manager->setOnDestroy([this](CHyprlandSurfaceManagerV1* manager) { destroyManager(manager); });

    manager->setDestroy([this](CHyprlandSurfaceManagerV1* manager) { destroyManager(manager); });
    manager->setGetHyprlandSurface(
        [this](CHyprlandSurfaceManagerV1* manager, uint32_t id, wl_resource* surface) { getSurface(manager, id, CWLSurfaceResource::fromResource(surface)); });
}

void CHyprlandSurfaceProtocol::destroyManager(CHyprlandSurfaceManagerV1* manager) {
    std::erase_if(m_managers, [&](const auto& p) { return p.get() == manager; });
}

void CHyprlandSurfaceProtocol::destroySurface(CHyprlandSurface* surface) {
    std::erase_if(m_surfaces, [&](const auto& entry) { return entry.second.get() == surface; });
}

void CHyprlandSurfaceProtocol::getSurface(CHyprlandSurfaceManagerV1* manager, uint32_t id, SP<CWLSurfaceResource> surface) {
    CHyprlandSurface* hyprlandSurface = nullptr;
    auto              iter            = std::ranges::find_if(m_surfaces, [&](const auto& entry) { return entry.second->m_surface == surface; });

    if (iter != m_surfaces.end()) {
        if (iter->second->m_resource) {
            LOGM(ERR, "HyprlandSurface already present for surface {:x}", (uintptr_t)surface.get());
            manager->error(HYPRLAND_SURFACE_MANAGER_V1_ERROR_ALREADY_CONSTRUCTED, "HyprlandSurface already present");
            return;
        } else {
            iter->second->setResource(makeShared<CHyprlandSurfaceV1>(manager->client(), manager->version(), id));
            hyprlandSurface = iter->second.get();
        }
    } else {
        hyprlandSurface =
            m_surfaces.emplace(surface, makeUnique<CHyprlandSurface>(makeShared<CHyprlandSurfaceV1>(manager->client(), manager->version(), id), surface)).first->second.get();
    }

    if UNLIKELY (!hyprlandSurface->good()) {
        manager->noMemory();
        m_surfaces.erase(surface);
    }
}
