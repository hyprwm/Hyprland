#pragma once

#include <hyprutils/math/Region.hpp>
#include <vector>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "hyprland-surface-v1.hpp"
#include "../helpers/signal/Signal.hpp"

class CWLSurfaceResource;
class CHyprlandSurfaceProtocol;

class CHyprlandSurface {
  public:
    CHyprlandSurface(SP<CHyprlandSurfaceV1> resource, SP<CWLSurfaceResource> surface);

    bool good() const;
    void setResource(SP<CHyprlandSurfaceV1> resource);

  private:
    SP<CHyprlandSurfaceV1> m_resource;
    WP<CWLSurfaceResource> m_surface;
    float                  m_opacity              = 1.0;
    bool                   m_visibleRegionChanged = false;
    CRegion                m_visibleRegion;

    void                   destroy();

    struct {
        CHyprSignalListener surfaceCommitted;
        CHyprSignalListener surfaceDestroyed;
    } m_listeners;

    friend class CHyprlandSurfaceProtocol;
};

class CHyprlandSurfaceProtocol : public IWaylandProtocol {
  public:
    CHyprlandSurfaceProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void                                                             destroyManager(CHyprlandSurfaceManagerV1* res);
    void                                                             destroySurface(CHyprlandSurface* surface);
    void                                                             getSurface(CHyprlandSurfaceManagerV1* manager, uint32_t id, SP<CWLSurfaceResource> surface);

    std::vector<UP<CHyprlandSurfaceManagerV1>>                       m_managers;
    std::unordered_map<WP<CWLSurfaceResource>, UP<CHyprlandSurface>> m_surfaces;

    friend class CHyprlandSurface;
};

namespace PROTO {
    inline UP<CHyprlandSurfaceProtocol> hyprlandSurface;
}
