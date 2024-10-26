#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "hyprland-ctm-control-v1.hpp"
#include <unordered_map>

class CMonitor;

class CHyprlandCTMControlResource {
  public:
    CHyprlandCTMControlResource(SP<CHyprlandCtmControlManagerV1> resource_);
    ~CHyprlandCTMControlResource();

    bool good();

  private:
    SP<CHyprlandCtmControlManagerV1>        resource;

    std::unordered_map<std::string, Mat3x3> ctms;
};

class CHyprlandCTMControlProtocol : public IWaylandProtocol {
  public:
    CHyprlandCTMControlProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyResource(CHyprlandCTMControlResource* resource);

    void setCTM(PHLMONITOR monitor, const Mat3x3& ctm);

    //
    std::vector<SP<CHyprlandCTMControlResource>> m_vManagers;

    friend class CHyprlandCTMControlResource;
};

namespace PROTO {
    inline UP<CHyprlandCTMControlProtocol> ctm;
};