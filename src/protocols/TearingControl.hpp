#pragma once

#include <memory>
#include "WaylandProtocol.hpp"

class CWindow;

enum eTearingPresentationHint {
    TEARING_VSYNC = 0,
    TEARING_ASYNC,
};

class CTearingControlProtocol;

class CTearingControl {
  public:
    CTearingControl(SP<CWaylandResource> resource_, wlr_surface* surf_);

    void onHint(uint32_t hint_);

    bool good();

    bool operator==(const wl_resource* other) const {
        return other == resource->resource();
    }

    bool operator==(const CTearingControl* other) const {
        return other->resource == resource;
    }

  private:
    void                     updateWindow();

    SP<CWaylandResource>     resource;
    CWindow*                 pWindow = nullptr;
    eTearingPresentationHint hint    = TEARING_VSYNC;

    friend class CTearingControlProtocol;
};

class CTearingControlProtocol : public IWaylandProtocol {
  public:
    CTearingControlProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         onManagerResourceDestroy(wl_resource* res);
    void         onControllerDestroy(CTearingControl* control);
    void         onGetController(wl_client* client, wl_resource* resource, uint32_t id, wlr_surface* surf);

  private:
    void                              onWindowDestroy(CWindow* pWindow);

    std::vector<UP<CWaylandResource>> m_vManagers;
    std::vector<UP<CTearingControl>>  m_vTearingControllers;
};

namespace PROTO {
    inline UP<CTearingControlProtocol> tearing;
};