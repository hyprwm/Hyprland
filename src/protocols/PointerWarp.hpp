#pragma once

#include "WaylandProtocol.hpp"
#include "pointer-warp-v1.hpp"

class CPointerWarpProtocol : public IWaylandProtocol {
  public:
    CPointerWarpProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyManager(CWpPointerWarpV1* manager);

    //
    std::vector<UP<CWpPointerWarpV1>> m_managers;
};

namespace PROTO {
    inline UP<CPointerWarpProtocol> pointerWarp;
};
