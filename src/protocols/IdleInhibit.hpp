#pragma once

#include <memory>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "idle-inhibit-unstable-v1.hpp"
#include "../helpers/signal/Signal.hpp"

class CIdleInhibitorResource;

class CIdleInhibitor {
  public:
    CIdleInhibitor(SP<CIdleInhibitorResource> resource_, wlr_surface* surf_);

    struct {
        CHyprSignalListener destroy;
    } listeners;

    WP<CIdleInhibitorResource> resource;
    wlr_surface*               surface = nullptr;
};

class CIdleInhibitorResource {
  public:
    CIdleInhibitorResource(SP<CZwpIdleInhibitorV1> resource_, wlr_surface* surface_);
    ~CIdleInhibitorResource();

    SP<CIdleInhibitor> inhibitor;

    struct {
        CSignal destroy;
    } events;

  private:
    SP<CZwpIdleInhibitorV1> resource;
    wlr_surface*            surface     = nullptr;
    bool                    destroySent = false;

    DYNLISTENER(surfaceDestroy);
};

class CIdleInhibitProtocol : public IWaylandProtocol {
  public:
    CIdleInhibitProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    struct {
        CSignal newIdleInhibitor; // data: SP<CIdleInhibitor>
    } events;

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void onCreateInhibitor(CZwpIdleInhibitManagerV1* pMgr, uint32_t id, wlr_surface* surface);

    void removeInhibitor(CIdleInhibitorResource*);

    //
    std::vector<UP<CZwpIdleInhibitManagerV1>> m_vManagers;
    std::vector<SP<CIdleInhibitorResource>>   m_vInhibitors;

    friend class CIdleInhibitorResource;
};

namespace PROTO {
    inline UP<CIdleInhibitProtocol> idleInhibit;
}