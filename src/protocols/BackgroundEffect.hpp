#pragma once

#include <hyprutils/math/Region.hpp>
#include <vector>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "ext-background-effect-v1.hpp"
#include "../helpers/signal/Signal.hpp"

class CWLSurfaceResource;
class CBackgroundEffectProtocol;

class CBackgroundEffect {
  public:
    CBackgroundEffect(SP<CExtBackgroundEffectSurfaceV1> resource, SP<CWLSurfaceResource> surface);

    bool good() const;
    void setResource(SP<CExtBackgroundEffectSurfaceV1> resource);

  private:
    SP<CExtBackgroundEffectSurfaceV1> m_resource;
    WP<CWLSurfaceResource>           m_surface;
    CRegion                           m_blurRegion;

    void                              destroy();

    struct {
        CHyprSignalListener surfaceCommitted;
        CHyprSignalListener surfaceDestroyed;
    } m_listeners;

    friend class CBackgroundEffectProtocol;
};

class CBackgroundEffectProtocol : public IWaylandProtocol {
  public:
    CBackgroundEffectProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyManager(CExtBackgroundEffectManagerV1* res);
    void destroyEffect(CBackgroundEffect* effect);
    void getBackgroundEffect(CExtBackgroundEffectManagerV1* manager, uint32_t id, SP<CWLSurfaceResource> surface);

    std::vector<UP<CExtBackgroundEffectManagerV1>>                       m_managers;
    std::unordered_map<WP<CWLSurfaceResource>, UP<CBackgroundEffect>>   m_effects;

    friend class CBackgroundEffect;
};

namespace PROTO {
    inline UP<CBackgroundEffectProtocol> backgroundEffect;
}
