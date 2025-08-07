#include "Subcompositor.hpp"
#include "Compositor.hpp"
#include <algorithm>

CWLSubsurfaceResource::CWLSubsurfaceResource(SP<CWlSubsurface> resource_, SP<CWLSurfaceResource> surface_, SP<CWLSurfaceResource> parent_) :
    m_surface(surface_), m_parent(parent_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CWlSubsurface* r) { destroy(); });
    m_resource->setDestroy([this](CWlSubsurface* r) { destroy(); });

    m_resource->setSetPosition([this](CWlSubsurface* r, int32_t x, int32_t y) { m_position = {x, y}; });

    m_resource->setSetDesync([this](CWlSubsurface* r) { m_sync = false; });
    m_resource->setSetSync([this](CWlSubsurface* r) { m_sync = true; });

    m_resource->setPlaceAbove([this](CWlSubsurface* r, wl_resource* surf) {
        auto SURF = CWLSurfaceResource::fromResource(surf);

        if (!m_parent)
            return;

        std::erase_if(m_parent->m_subsurfaces, [this](const auto& e) { return e == m_self || !e; });

        std::ranges::for_each(m_parent->m_subsurfaces, [](const auto& e) { e->m_zIndex *= 2; });

        auto it = std::ranges::find_if(m_parent->m_subsurfaces, [SURF](const auto& s) { return s->m_surface == SURF; });

        if ((it == m_parent->m_subsurfaces.end() && m_parent != SURF) || SURF == m_surface) {
            // protocol error, this is not a valid surface
            r->error(-1, "Invalid surface in placeAbove");
            return;
        }

        if (it == m_parent->m_subsurfaces.end()) {
            // parent surface
            m_parent->m_subsurfaces.emplace_back(m_self);
            m_zIndex = 1;
        } else {
            m_zIndex = (*it)->m_zIndex + 1;
            m_parent->m_subsurfaces.emplace_back(m_self);
        }

        m_parent->sortSubsurfaces();
    });

    m_resource->setPlaceBelow([this](CWlSubsurface* r, wl_resource* surf) {
        auto SURF = CWLSurfaceResource::fromResource(surf);

        if (!m_parent)
            return;

        std::erase_if(m_parent->m_subsurfaces, [this](const auto& e) { return e == m_self || !e; });

        std::ranges::for_each(m_parent->m_subsurfaces, [](const auto& e) { e->m_zIndex *= 2; });

        auto it = std::ranges::find_if(m_parent->m_subsurfaces, [SURF](const auto& s) { return s->m_surface == SURF; });

        if ((it == m_parent->m_subsurfaces.end() && m_parent != SURF) || SURF == m_surface) {
            // protocol error, this is not a valid surface
            r->error(-1, "Invalid surface in placeBelow");
            return;
        }

        if (it == m_parent->m_subsurfaces.end()) {
            // parent
            m_parent->m_subsurfaces.emplace_back(m_self);
            m_zIndex = -1;
        } else {
            m_zIndex = (*it)->m_zIndex - 1;
            m_parent->m_subsurfaces.emplace_back(m_self);
        }

        m_parent->sortSubsurfaces();
    });

    m_listeners.commitSurface = m_surface->m_events.commit.listen([this] {
        if (m_surface->m_current.texture && !m_surface->m_mapped) {
            m_surface->map();
            m_surface->m_events.map.emit();
            return;
        }

        if (!m_surface->m_current.texture && m_surface->m_mapped) {
            m_surface->m_events.unmap.emit();
            m_surface->unmap();
            return;
        }
    });
}

CWLSubsurfaceResource::~CWLSubsurfaceResource() {
    m_events.destroy.emit();
    if (m_surface)
        m_surface->resetRole();
}

void CWLSubsurfaceResource::destroy() {
    if (m_surface && m_surface->m_mapped) {
        m_surface->m_events.unmap.emit();
        m_surface->unmap();
    }
    m_events.destroy.emit();
    PROTO::subcompositor->destroyResource(this);
}

Vector2D CWLSubsurfaceResource::posRelativeToParent() {
    Vector2D               pos  = m_position;
    SP<CWLSurfaceResource> surf = m_parent.lock();

    // some apps might create cycles, which I believe _technically_ are not a protocol error
    // in some cases, notably firefox likes to do that, so we keep track of what
    // surfaces we've visited and if we hit a surface we've visited we bail out.
    std::vector<SP<CWLSurfaceResource>> surfacesVisited;

    while (surf->m_role->role() == SURFACE_ROLE_SUBSURFACE && std::ranges::find_if(surfacesVisited, [surf](const auto& other) { return surf == other; }) == surfacesVisited.end()) {
        surfacesVisited.emplace_back(surf);
        auto subsurface = sc<CSubsurfaceRole*>(m_parent->m_role.get())->m_subsurface.lock();
        pos += subsurface->m_position;
        surf = subsurface->m_parent.lock();
    }
    return pos;
}

bool CWLSubsurfaceResource::good() {
    return m_resource->resource();
}

SP<CWLSurfaceResource> CWLSubsurfaceResource::t1Parent() {
    SP<CWLSurfaceResource>              surf = m_parent.lock();
    std::vector<SP<CWLSurfaceResource>> surfacesVisited;

    while (surf->m_role->role() == SURFACE_ROLE_SUBSURFACE && std::ranges::find_if(surfacesVisited, [surf](const auto& other) { return surf == other; }) == surfacesVisited.end()) {
        surfacesVisited.emplace_back(surf);
        auto subsurface = sc<CSubsurfaceRole*>(m_parent->m_role.get())->m_subsurface.lock();
        surf            = subsurface->m_parent.lock();
    }
    return surf;
}

CWLSubcompositorResource::CWLSubcompositorResource(SP<CWlSubcompositor> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CWlSubcompositor* r) { PROTO::subcompositor->destroyResource(this); });
    m_resource->setDestroy([this](CWlSubcompositor* r) { PROTO::subcompositor->destroyResource(this); });

    m_resource->setGetSubsurface([](CWlSubcompositor* r, uint32_t id, wl_resource* surface, wl_resource* parent) {
        auto SURF   = CWLSurfaceResource::fromResource(surface);
        auto PARENT = CWLSurfaceResource::fromResource(parent);

        if UNLIKELY (!SURF || !PARENT || SURF == PARENT) {
            r->error(WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, "Invalid surface/parent");
            return;
        }

        if UNLIKELY (SURF->m_role->role() != SURFACE_ROLE_UNASSIGNED) {
            r->error(-1, "Surface already has a different role");
            return;
        }

        SP<CWLSurfaceResource> t1Parent = nullptr;

        if (PARENT->m_role->role() == SURFACE_ROLE_SUBSURFACE) {
            auto subsurface = sc<CSubsurfaceRole*>(PARENT->m_role.get())->m_subsurface.lock();
            t1Parent        = subsurface->t1Parent();
        } else
            t1Parent = PARENT;

        if UNLIKELY (t1Parent == SURF) {
            r->error(WL_SUBCOMPOSITOR_ERROR_BAD_PARENT, "Bad parent, t1 parent == surf");
            return;
        }

        const auto RESOURCE =
            PROTO::subcompositor->m_surfaces.emplace_back(makeShared<CWLSubsurfaceResource>(makeShared<CWlSubsurface>(r->client(), r->version(), id), SURF, PARENT));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::subcompositor->m_surfaces.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;
        SURF->m_role     = makeShared<CSubsurfaceRole>(RESOURCE);
        PARENT->m_subsurfaces.emplace_back(RESOURCE);

        LOGM(LOG, "New wl_subsurface with id {} at {:x}", id, (uintptr_t)RESOURCE.get());

        PARENT->m_events.newSubsurface.emit(RESOURCE);
    });
}

bool CWLSubcompositorResource::good() {
    return m_resource->resource();
}

CWLSubcompositorProtocol::CWLSubcompositorProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CWLSubcompositorProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CWLSubcompositorResource>(makeShared<CWlSubcompositor>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }
}

void CWLSubcompositorProtocol::destroyResource(CWLSubcompositorResource* resource) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == resource; });
}

void CWLSubcompositorProtocol::destroyResource(CWLSubsurfaceResource* resource) {
    std::erase_if(m_surfaces, [&](const auto& other) { return other.get() == resource; });
}

CSubsurfaceRole::CSubsurfaceRole(SP<CWLSubsurfaceResource> sub) : m_subsurface(sub) {
    ;
}
