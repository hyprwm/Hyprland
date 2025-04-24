#pragma once

#include <vector>
#include "WaylandProtocol.hpp"
#include "hyprland-toplevel-mapping-v1.hpp"

class CToplevelWindowMappingHandle {
  public:
    CToplevelWindowMappingHandle(SP<CHyprlandToplevelWindowMappingHandleV1> resource_);

  private:
    SP<CHyprlandToplevelWindowMappingHandleV1> resource;

    friend class CToplevelMappingManager;
    friend class CToplevelMappingProtocol;
};

class CToplevelMappingManager {
  public:
    CToplevelMappingManager(SP<CHyprlandToplevelMappingManagerV1> resource_);

    bool good() const;

  private:
    SP<CHyprlandToplevelMappingManagerV1> resource;
};

class CToplevelMappingProtocol : IWaylandProtocol {
  public:
    CToplevelMappingProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void                                          onManagerResourceDestroy(CToplevelMappingManager* mgr);
    void                                          destroyHandle(CHyprlandToplevelWindowMappingHandleV1* handle);

    std::vector<UP<CToplevelMappingManager>>      m_vManagers;
    std::vector<SP<CToplevelWindowMappingHandle>> m_vHandles;

    friend class CToplevelMappingManager;
};

namespace PROTO {
    inline UP<CToplevelMappingProtocol> toplevelMapping;
};