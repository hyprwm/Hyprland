#pragma once

#include "WaylandProtocol.hpp"
#include "tearing-control-v1.hpp"

#include "../helpers/signal/Signal.hpp"

class CWindow;
class CTearingControlProtocol;
class CWLSurfaceResource;

class CTearingControl {
  public:
    CTearingControl(SP<CWpTearingControlV1> resource_, SP<CWLSurfaceResource> surf_);

    bool good();

  private:
    SP<CWpTearingControlV1> resource;
    SP<CWLSurfaceResource>  surface;

    struct {
        CHyprSignalListener surfaceDestroyed;
    } listeners;

    friend class CTearingControlProtocol;
};

class CTearingControlProtocol : public IWaylandProtocol {
  public:
    CTearingControlProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyResource(CWpTearingControlManagerV1* resource);
    void destroyResource(CTearingControl* resource);

    //
    std::vector<UP<CWpTearingControlManagerV1>> m_vManagers;
    std::vector<UP<CTearingControl>>            m_vTearingControllers;

    friend class CTearingControl;
};

namespace PROTO {
    inline UP<CTearingControlProtocol> tearing;
};
