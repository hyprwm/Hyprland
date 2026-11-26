#include "HyprlandSurface.hpp"
#include "../desktop/view/WLSurface.hpp"
#include "../helpers/math/Math.hpp"
#include "../render/Renderer.hpp"
#include "core/Compositor.hpp"
#include "hyprland-surface-v1.hpp"
#include <hyprutils/math/Region.hpp>
#include <utility>
#include <wayland-server.h>

static CRegion effectiveVisibleRegion(const CRegion& region, const CBox& bufferBox) {
    if (region.empty())
        return CRegion{bufferBox};

    CRegion effective = region.copy();
    effective.intersect(bufferBox);
    if (!effective.empty())
        effective.expand(1);
    return effective;
}

static std::pair<Vector2D, Vector2D> viewportSourceUV(const SSurfaceState& state) {
    if (!state.viewport.hasSource || state.bufferSize.x <= 0 || state.bufferSize.y <= 0)
        return {{0.0, 0.0}, {1.0, 1.0}};

    auto           bufferSource = state.viewport.source;
    const Vector2D trc          = state.transform % 2 == 1 ? Vector2D{state.bufferSize.y, state.bufferSize.x} : state.bufferSize;
    bufferSource.scale(state.scale);
    bufferSource.transform(Math::wlTransformToHyprutils(Math::invertTransform(state.transform)), trc.x, trc.y);

    const Vector2D uvTL = {bufferSource.x / state.bufferSize.x, bufferSource.y / state.bufferSize.y};
    const Vector2D uvBR = {(bufferSource.x + bufferSource.width) / state.bufferSize.x, (bufferSource.y + bufferSource.height) / state.bufferSize.y};

    if (uvBR.x < 0.00001 || uvBR.y < 0.00001)
        return {{0.0, 0.0}, {1.0, 1.0}};

    return {uvTL, uvBR};
}

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
        markPending();
    });

    m_resource->setSetVisibleRegion([this](CHyprlandSurfaceV1* resource, wl_resource* region) {
        if (!region) {
            m_visibleRegion.clear();
            markPending();
            return;
        }

        m_visibleRegion = CWLRegionResource::fromResource(region)->m_region;
        markPending();
    });

    m_listeners.surfaceCommitted = m_surface->m_events.commit.listen([this] {
        auto surface = Desktop::View::CWLSurface::fromResource(m_surface.lock());
        if (!surface) {
            if (!m_resource)
                PROTO::hyprlandSurface->destroySurface(this);
            return;
        }

        const auto& cur            = m_surface->m_current;
        const bool  opacityChanged = surface->m_overallOpacity != m_opacity;
        const bool  regionChanged  = !pixman_region32_equal(surface->m_visibleRegion.pixman(), m_visibleRegion.pixman());
        const bool  hasMask        = !surface->m_visibleRegion.empty() || !m_visibleRegion.empty();
        const bool  mappingChanged = hasMask && (cur.updated.bits.viewport || cur.updated.bits.scale || cur.updated.bits.transform);

        surface->m_overallOpacity = m_opacity;

        const auto box = surface->getSurfaceBoxGlobal();
        if (opacityChanged || mappingChanged) {
            surface->m_visibleRegion = m_visibleRegion;
            if (box.has_value())
                g_pHyprRenderer->damageBox(*box);
        } else if (regionChanged && box.has_value()) {
            if (cur.bufferSize.x > 0 && cur.bufferSize.y > 0) {
                const CBox bufferBox{Vector2D(), cur.bufferSize};

                CRegion    oldRegion = effectiveVisibleRegion(surface->m_visibleRegion, bufferBox);
                CRegion    newRegion = effectiveVisibleRegion(m_visibleRegion, bufferBox);

                surface->m_visibleRegion = m_visibleRegion;

                const auto [uvTL, uvBR] = viewportSourceUV(cur);
                const Vector2D uvExtent = uvBR - uvTL;
                if (uvExtent.x > 0.0 && uvExtent.y > 0.0) {
                    const Vector2D uvOffset = -uvTL * cur.bufferSize;
                    oldRegion.translate(uvOffset);
                    newRegion.translate(uvOffset);

                    const Vector2D scale = (Vector2D{1.0, 1.0} / uvExtent) * (box->size() / cur.bufferSize);
                    g_pHyprRenderer->damageRegionDiff(oldRegion, newRegion, *box, scale);
                } else
                    g_pHyprRenderer->damageBox(*box);
            } else
                surface->m_visibleRegion = m_visibleRegion;
        } else
            surface->m_visibleRegion = m_visibleRegion;

        if (!m_resource)
            PROTO::hyprlandSurface->destroySurface(this);
    });

    m_listeners.surfaceDestroyed = m_surface->m_events.destroy.listen([this] {
        if (!m_resource)
            PROTO::hyprlandSurface->destroySurface(this);
    });
}

void CHyprlandSurface::destroy() {
    m_resource.reset();
    m_opacity = 1.F;

    m_visibleRegion.clear();
    markPending();

    if (!m_surface)
        PROTO::hyprlandSurface->destroySurface(this);
}

void CHyprlandSurface::markPending() {
    if (m_surface)
        m_surface->m_pending.updated.bits.hyprlandSurface = true;
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
            LOG(Log::ERR, "HyprlandSurface already present for surface {:x}", (uintptr_t)surface.get());
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
