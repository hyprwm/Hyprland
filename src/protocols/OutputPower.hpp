#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "wlr-output-power-management-unstable-v1.hpp"
#include "../helpers/signal/Signal.hpp"

class CMonitor;

class COutputPower {
  public:
    COutputPower(SP<CZwlrOutputPowerV1> resource_, PHLMONITOR pMonitor);

    bool good();

  private:
    SP<CZwlrOutputPowerV1> resource;

    PHLMONITORREF          pMonitor;

    struct {
        CHyprSignalListener monitorDestroy;
        CHyprSignalListener monitorState;
        CHyprSignalListener monitorDpms;
        m_m_listeners;
    };

    class COutputPowerProtocol : public IWaylandProtocol {
      public:
        COutputPowerProtocol(const wl_interface* iface, const int& ver, const std::string& name);

        virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

      private:
        void onManagerResourceDestroy(wl_resource* res);
        void destroyOutputPower(COutputPower* pointer);
        void onGetOutputPower(CZwlrOutputPowerManagerV1* pMgr, uint32_t id, wl_resource* output);

        //
        std::vector<UP<CZwlrOutputPowerManagerV1>> m_vManagers;
        std::vector<UP<COutputPower>>              m_vOutputPowers;

        friend class COutputPower;
    };

    namespace PROTO {
        inline UP<COutputPowerProtocol> outputPower;
    };