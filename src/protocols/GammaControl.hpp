#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "wlr-gamma-control-unstable-v1.hpp"
#include "../helpers/signal/Signal.hpp"

class CMonitor;

class CGammaControl {
  public:
    CGammaControl(SP<CZwlrGammaControlV1> resource_, wl_resource* output);
    ~CGammaControl();

    bool       good();
    void       applyToMonitor();
    PHLMONITOR getMonitor();

  private:
    SP<CZwlrGammaControlV1> m_resource;
    PHLMONITORREF           m_monitor;
    size_t                  m_gammaSize     = 0;
    bool                    m_gammaTableSet = false;
    std::vector<uint16_t>   m_gammaTable; // [r,g,b]+

    void                    onMonitorDestroy();

    struct {
        CHyprSignalListener monitorDisconnect;
        CHyprSignalListener monitorDestroy;
    } m_listeners;
};

class CGammaControlProtocol : public IWaylandProtocol {
  public:
    CGammaControlProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         applyGammaToState(PHLMONITOR pMonitor);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyGammaControl(CGammaControl* gamma);
    void onGetGammaControl(CZwlrGammaControlManagerV1* pMgr, uint32_t id, wl_resource* output);

    //
    std::vector<UP<CZwlrGammaControlManagerV1>> m_managers;
    std::vector<UP<CGammaControl>>              m_gammaControllers;

    friend class CGammaControl;
};

namespace PROTO {
    inline UP<CGammaControlProtocol> gamma;
};
