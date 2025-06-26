#pragma once

#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "idle-inhibit-unstable-v1.hpp"
#include "../helpers/signal/Signal.hpp"

class CIdleInhibitorResource;
class CWLSurfaceResource;

class CIdleInhibitor {
  public:
    CIdleInhibitor(SP<CIdleInhibitorResource> resource_, SP<CWLSurfaceResource> surf_);

    struct {
        CHyprSignalListener destroy;
    } m_listeners;

    WP<CIdleInhibitorResource> m_resource;
    WP<CWLSurfaceResource>     m_surface;
};

class CIdleInhibitorResource {
  public:
    CIdleInhibitorResource(SP<CZwpIdleInhibitorV1> resource_, SP<CWLSurfaceResource> surface_);
    ~CIdleInhibitorResource();

    SP<CIdleInhibitor> m_inhibitor;

    struct {
        CSignalT<> destroy;
    } m_events;

  private:
    SP<CZwpIdleInhibitorV1> m_resource;
    WP<CWLSurfaceResource>  m_surface;
    bool                    m_destroySent = false;

    struct {
        CHyprSignalListener destroySurface;
    } m_listeners;
};

class CIdleInhibitProtocol : public IWaylandProtocol {
  public:
    CIdleInhibitProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    struct {
        CSignalT<SP<CIdleInhibitor>> newIdleInhibitor;
    } m_events;

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void onCreateInhibitor(CZwpIdleInhibitManagerV1* pMgr, uint32_t id, SP<CWLSurfaceResource> surface);

    void removeInhibitor(CIdleInhibitorResource*);

    //
    std::vector<UP<CZwpIdleInhibitManagerV1>> m_managers;
    std::vector<SP<CIdleInhibitorResource>>   m_inhibitors;

    friend class CIdleInhibitorResource;
};

namespace PROTO {
    inline UP<CIdleInhibitProtocol> idleInhibit;
}
