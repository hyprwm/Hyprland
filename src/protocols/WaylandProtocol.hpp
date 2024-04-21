#pragma once

#include "../defines.hpp"

#include <functional>

#define RESOURCE_OR_BAIL(resname)                                                                                                                                                  \
    const auto resname = (CWaylandResource*)wl_resource_get_user_data(resource);                                                                                                   \
    if (!resname)                                                                                                                                                                  \
        return;

#define SP std::shared_ptr
#define UP std::unique_ptr
#define WP std::weak_ptr

#define PROTO NProtocols

class IWaylandProtocol {
  public:
    IWaylandProtocol(const wl_interface* iface, const int& ver, const std::string& name);
    ~IWaylandProtocol();

    virtual void onDisplayDestroy();

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) = 0;

  private:
    wl_global*  m_pGlobal = nullptr;
    wl_listener m_liDisplayDestroy;
};