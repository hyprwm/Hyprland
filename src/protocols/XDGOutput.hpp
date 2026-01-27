#pragma once

#include "xdg-output-unstable-v1.hpp"
#include "WaylandProtocol.hpp"
#include <optional>

class CMonitor;
class CXDGOutputProtocol;
class CWLOutputProtocol;

class CXDGOutput {
  public:
    CXDGOutput(SP<CZxdgOutputV1> resource, PHLMONITOR monitor_);

    void sendDetails();

  private:
    PHLMONITORREF         m_monitor;
    SP<CZxdgOutputV1>     m_resource;
    WP<CWLOutputProtocol> m_outputProto;

    wl_client*            m_client     = nullptr;
    bool                  m_isXWayland = false;

    friend class CXDGOutputProtocol;
};

class CXDGOutputProtocol : public IWaylandProtocol {
  public:
    CXDGOutputProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);
    void         updateAllOutputs();

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void onOutputResourceDestroy(wl_resource* res);
    void onManagerGetXDGOutput(CZxdgOutputManagerV1* mgr, uint32_t id, wl_resource* outputResource);

    //
    std::vector<UP<CZxdgOutputManagerV1>> m_managerResources;
    std::vector<UP<CXDGOutput>>           m_xdgOutputs;

    friend class CXDGOutput;
};

namespace PROTO {
    inline UP<CXDGOutputProtocol> xdgOutput;
};
