#pragma once

#include <vector>
#include "WaylandProtocol.hpp"
#include "xdg-system-bell-v1.hpp"

class CXDGSystemBellManagerResource {
  public:
    CXDGSystemBellManagerResource(UP<CXdgSystemBellV1>&& resource);

    bool good();

  private:
    UP<CXdgSystemBellV1> m_resource;
};

class CXDGSystemBellProtocol : public IWaylandProtocol {
  public:
    CXDGSystemBellProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyResource(CXDGSystemBellManagerResource* res);

    //
    std::vector<UP<CXDGSystemBellManagerResource>> m_vManagers;

    friend class CXDGSystemBellManagerResource;
};

namespace PROTO {
    inline UP<CXDGSystemBellProtocol> xdgBell;
};
