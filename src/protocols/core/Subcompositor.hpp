
#pragma once

/*
    Implementations for:
     - wl_subsurface
     - wl_subcompositor
*/

#include <vector>
#include <cstdint>
#include "../WaylandProtocol.hpp"
#include "wayland.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "../types/SurfaceRole.hpp"

class CWLSurfaceResource;
class CWLSubsurfaceResource;

class CSubsurfaceRole : public ISurfaceRole {
  public:
    CSubsurfaceRole(SP<CWLSubsurfaceResource> sub);

    virtual eSurfaceRole role() {
        return SURFACE_ROLE_SUBSURFACE;
    }

    WP<CWLSubsurfaceResource> m_subsurface;
};

class CWLSubsurfaceResource {
  public:
    CWLSubsurfaceResource(SP<CWlSubsurface> resource_, SP<CWLSurfaceResource> surface_, SP<CWLSurfaceResource> parent_);
    ~CWLSubsurfaceResource();

    Vector2D                  posRelativeToParent();
    bool                      good();
    SP<CWLSurfaceResource>    t1Parent();

    bool                      m_sync = false;
    Vector2D                  m_position;

    WP<CWLSurfaceResource>    m_surface;
    WP<CWLSurfaceResource>    m_parent;

    WP<CWLSubsurfaceResource> m_self;

    int                       m_zIndex = 1; // by default, it's above

    struct {
        CSignalT<> destroy;
    } m_events;

  private:
    SP<CWlSubsurface> m_resource;

    void              destroy();

    struct {
        CHyprSignalListener commitSurface;
    } m_listeners;
};

class CWLSubcompositorResource {
  public:
    CWLSubcompositorResource(SP<CWlSubcompositor> resource_);

    bool good();

  private:
    SP<CWlSubcompositor> m_resource;
};

class CWLSubcompositorProtocol : public IWaylandProtocol {
  public:
    CWLSubcompositorProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyResource(CWLSubcompositorResource* resource);
    void destroyResource(CWLSubsurfaceResource* resource);

    //
    std::vector<SP<CWLSubcompositorResource>> m_managers;
    std::vector<SP<CWLSubsurfaceResource>>    m_surfaces;

    friend class CWLSubcompositorResource;
    friend class CWLSubsurfaceResource;
};

namespace PROTO {
    inline UP<CWLSubcompositorProtocol> subcompositor;
};
