#pragma once

#include "WaylandProtocol.hpp"
#include "tearing-control-v1.hpp"

class CWindow;
class CTearingControlProtocol;
class CWLSurfaceResource;

class CTearingControl {
  public:
    CTearingControl(SP<CWpTearingControlV1> resource_, SP<CWLSurfaceResource> surf_);

    void onHint(wpTearingControlV1PresentationHint hint_);

    bool good();

    bool operator==(const wl_resource* other) const {
        return other == m_resource->resource();
    }

    bool operator==(const CTearingControl* other) const {
        return other->m_resource == m_resource;
    }

  private:
    void                               updateWindow();

    SP<CWpTearingControlV1>            m_resource;
    PHLWINDOWREF                       m_window;
    wpTearingControlV1PresentationHint m_hint = WP_TEARING_CONTROL_V1_PRESENTATION_HINT_VSYNC;

    friend class CTearingControlProtocol;
};

class CTearingControlProtocol : public IWaylandProtocol {
  public:
    CTearingControlProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void onControllerDestroy(CTearingControl* control);
    void onGetController(wl_client* client, CWpTearingControlManagerV1* pMgr, uint32_t id, SP<CWLSurfaceResource> surf);
    void onWindowDestroy(PHLWINDOW pWindow);

    //
    std::vector<UP<CWpTearingControlManagerV1>> m_managers;
    std::vector<UP<CTearingControl>>            m_tearingControllers;

    friend class CTearingControl;
};

namespace PROTO {
    inline UP<CTearingControlProtocol> tearing;
};