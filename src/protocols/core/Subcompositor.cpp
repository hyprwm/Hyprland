#include "Subcompositor.hpp"
#include "Compositor.hpp"
#include <algorithm>

#define LOGM PROTO::subcompositor->protoLog

CWLSubsurfaceResource::CWLSubsurfaceResource(SP<CWlSubsurface> resource_, SP<CWLSurfaceResource> surface_, SP<CWLSurfaceResource> parent_) :
    surface(surface_), parent(parent_), resource(resource_) {
    if (!good())
        return;

    resource->setOnDestroy([this](CWlSubsurface* r) { destroy(); });
    resource->setDestroy([this](CWlSubsurface* r) { destroy(); });

    resource->setSetPosition([this](CWlSubsurface* r, int32_t x, int32_t y) { position = {x, y}; });

    resource->setSetDesync([this](CWlSubsurface* r) { sync = false; });
    resource->setSetSync([this](CWlSubsurface* r) { sync = true; });

    resource->setPlaceAbove([this](CWlSubsurface* r, wl_resource* surf) {
        auto SURF = CWLSurfaceResource::fromResource(surf);

        if (!parent)
            return;

        std::erase(parent->subsurfaces, self.lock());

        auto it = std::find(parent->subsurfaces.begin(), parent->subsurfaces.end(), SURF);

        if (it == parent->subsurfaces.end()) {
            LOGM(ERR, "Invalid surface reference in placeAbove");
            parent->subsurfaces.emplace_back(self.lock());
        } else
            parent->subsurfaces.insert(it, self.lock());
    });

    resource->setPlaceBelow([this](CWlSubsurface* r, wl_resource* surf) {
        auto SURF = CWLSurfaceResource::fromResource(surf);

        if (!parent)
            return;

        std::erase(parent->subsurfaces, self.lock());

        auto it = std::find(parent->subsurfaces.begin(), parent->subsurfaces.end(), SURF);

        if (it == parent->subsurfaces.end()) {
            LOGM(ERR, "Invalid surface reference in placeBelow");
            parent->subsurfaces.emplace_back(self.lock());
        } else
            parent->subsurfaces.insert(it--, self.lock());
    });

    listeners.commitSurface = surface->events.commit.registerListener([this](std::any d) {
        if (surface->current.buffer && !surface->mapped) {
            surface->map();
            return;
        }

        if (!surface->current.buffer && surface->mapped) {
            surface->unmap();
            return;
        }
    });
}

CWLSubsurfaceResource::~CWLSubsurfaceResource() {
    events.destroy.emit();
    if (surface)
        surface->resetRole();
}

void CWLSubsurfaceResource::destroy() {
    if (surface && surface->mapped)
        surface->unmap();
    events.destroy.emit();
    PROTO::subcompositor->destroyResource(this);
}

Vector2D CWLSubsurfaceResource::posRelativeToParent() {
    Vector2D               pos  = position;
    SP<CWLSurfaceResource> surf = parent.lock();

    // some apps might create cycles, which I believe _technically_ are not a protocol error
    // in some cases, notably firefox likes to do that, so we keep track of what
    // surfaces we've visited and if we hit a surface we've visited we bail out.
    std::vector<SP<CWLSurfaceResource>> surfacesVisited;

    while (surf->role->role() == SURFACE_ROLE_SUBSURFACE &&
           std::find_if(surfacesVisited.begin(), surfacesVisited.end(), [surf](const auto& other) { return surf == other; }) == surfacesVisited.end()) {
        surfacesVisited.emplace_back(surf);
        auto subsurface = (CWLSubsurfaceResource*)parent->role.get();
        pos += subsurface->position;
        surf = subsurface->parent.lock();
    }
    return pos;
}

bool CWLSubsurfaceResource::good() {
    return resource->resource();
}

eSurfaceRole CWLSubsurfaceResource::role() {
    return SURFACE_ROLE_SUBSURFACE;
}

SP<CWLSurfaceResource> CWLSubsurfaceResource::t1Parent() {
    SP<CWLSurfaceResource>              surf = parent.lock();
    std::vector<SP<CWLSurfaceResource>> surfacesVisited;

    while (surf->role->role() == SURFACE_ROLE_SUBSURFACE &&
           std::find_if(surfacesVisited.begin(), surfacesVisited.end(), [surf](const auto& other) { return surf == other; }) == surfacesVisited.end()) {
        surfacesVisited.emplace_back(surf);
        auto subsurface = (CWLSubsurfaceResource*)parent->role.get();
        surf            = subsurface->parent.lock();
    }
    return surf;
}

CWLSubcompositorResource::CWLSubcompositorResource(SP<CWlSubcompositor> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setOnDestroy([this](CWlSubcompositor* r) { PROTO::subcompositor->destroyResource(this); });
    resource->setDestroy([this](CWlSubcompositor* r) { PROTO::subcompositor->destroyResource(this); });

    resource->setGetSubsurface([](CWlSubcompositor* r, uint32_t id, wl_resource* surface, wl_resource* parent) {
        auto SURF   = CWLSurfaceResource::fromResource(surface);
        auto PARENT = CWLSurfaceResource::fromResource(parent);

        if (!SURF || !PARENT || SURF == PARENT) {
            r->error(WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, "Invalid surface/parent");
            return;
        }

        if (SURF->role->role() != SURFACE_ROLE_UNASSIGNED) {
            r->error(-1, "Surface already has a different role");
            return;
        }

        SP<CWLSurfaceResource> t1Parent = nullptr;

        if (PARENT->role->role() == SURFACE_ROLE_SUBSURFACE) {
            auto subsurface = (CWLSubsurfaceResource*)PARENT->role.get();
            t1Parent        = subsurface->t1Parent();
        } else
            t1Parent = PARENT;

        if (t1Parent == SURF) {
            r->error(WL_SUBCOMPOSITOR_ERROR_BAD_PARENT, "Bad parent, t1 parent == surf");
            return;
        }

        const auto RESOURCE =
            PROTO::subcompositor->m_vSurfaces.emplace_back(makeShared<CWLSubsurfaceResource>(makeShared<CWlSubsurface>(r->client(), r->version(), id), SURF, PARENT));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::subcompositor->m_vSurfaces.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;
        SURF->role     = RESOURCE;
        PARENT->subsurfaces.emplace_back(RESOURCE);

        LOGM(LOG, "New wl_subsurface with id {} at {:x}", id, (uintptr_t)RESOURCE.get());

        PARENT->events.newSubsurface.emit(RESOURCE);
    });
}

bool CWLSubcompositorResource::good() {
    return resource->resource();
}

CWLSubcompositorProtocol::CWLSubcompositorProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CWLSubcompositorProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CWLSubcompositorResource>(makeShared<CWlSubcompositor>(client, ver, id)));

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }
}

void CWLSubcompositorProtocol::destroyResource(CWLSubcompositorResource* resource) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == resource; });
}

void CWLSubcompositorProtocol::destroyResource(CWLSubsurfaceResource* resource) {
    std::erase_if(m_vSurfaces, [&](const auto& other) { return other.get() == resource; });
}
