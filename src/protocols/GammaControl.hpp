#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "wlr-gamma-control-unstable-v1.hpp"
#include "../helpers/signal/Listener.hpp"

class CMonitor;

class CGammaControl {
  public:
    CGammaControl(SP<CZwlrGammaControlV1> resource_, wl_resource* output);
    ~CGammaControl();

    bool      good();
    void      applyToMonitor();
    CMonitor* getMonitor();

  private:
    SP<CZwlrGammaControlV1> resource;
    CMonitor*               pMonitor      = nullptr;
    size_t                  gammaSize     = 0;
    bool                    gammaTableSet = false;
    std::vector<uint16_t>   gammaTable;

    void                    onMonitorDestroy();

    struct {
        CHyprSignalListener monitorDisconnect;
        CHyprSignalListener monitorDestroy;
    } listeners;
};

class CGammaControlProtocol : public IWaylandProtocol {
  public:
    CGammaControlProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         applyGammaToState(CMonitor* pMonitor);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyGammaControl(CGammaControl* gamma);
    void onGetGammaControl(CZwlrGammaControlManagerV1* pMgr, uint32_t id, wl_resource* output);

    //
    std::vector<UP<CZwlrGammaControlManagerV1>> m_vManagers;
    std::vector<UP<CGammaControl>>              m_vGammaControllers;

    friend class CGammaControl;
};

namespace PROTO {
    inline UP<CGammaControlProtocol> gamma;
};
