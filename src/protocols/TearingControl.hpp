#pragma once

#include <memory>
#include "WaylandProtocol.hpp"
#include "tearing-control-v1.hpp"

class CWindow;
class CTearingControlProtocol;

class CTearingControl {
  public:
    CTearingControl(SP<CWpTearingControlV1> resource_, wlr_surface* surf_);

    void onHint(wpTearingControlV1PresentationHint hint_);

    bool good();

    bool operator==(const wl_resource* other) const {
        return other == resource->resource();
    }

    bool operator==(const CTearingControl* other) const {
        return other->resource == resource;
    }

  private:
    void                               updateWindow();

    SP<CWpTearingControlV1>            resource;
    CWindow*                           pWindow = nullptr;
    wpTearingControlV1PresentationHint hint    = WP_TEARING_CONTROL_V1_PRESENTATION_HINT_VSYNC;

    friend class CTearingControlProtocol;
};

class CTearingControlProtocol : public IWaylandProtocol {
  public:
    CTearingControlProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void onControllerDestroy(CTearingControl* control);
    void onGetController(wl_client* client, wl_resource* resource, uint32_t id, wlr_surface* surf);
    void onWindowDestroy(CWindow* pWindow);

    //
    std::vector<UP<CWpTearingControlManagerV1>> m_vManagers;
    std::vector<UP<CTearingControl>>            m_vTearingControllers;

    friend class CTearingControl;
};

namespace PROTO {
    inline UP<CTearingControlProtocol> tearing;
};