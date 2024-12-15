#pragma once

#include <memory>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "protocols/ColorManagement.hpp"
#include "protocols/core/Compositor.hpp"
#include "frog-color-management-v1.hpp"

class CFrogColorManager {
  public:
    CFrogColorManager(SP<CFrogColorManagementFactoryV1> resource_);

    bool good();

  private:
    SP<CFrogColorManagementFactoryV1> resource;
};

class CFrogColorManagementSurface {
  public:
    CFrogColorManagementSurface(SP<CFrogColorManagedSurface> resource_, SP<CWLSurfaceResource> surface_);

    bool                            good();
    wl_client*                      client();

    WP<CFrogColorManagementSurface> self;
    WP<CWLSurfaceResource>          surface;

    SImageDescription               settings;

  private:
    SP<CFrogColorManagedSurface> resource;
    wl_client*                   pClient = nullptr;
};

class CFrogColorManagementProtocol : public IWaylandProtocol {
  public:
    CFrogColorManagementProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void                                         destroyResource(CFrogColorManager* resource);
    void                                         destroyResource(CFrogColorManagementSurface* resource);

    std::vector<SP<CFrogColorManager>>           m_vManagers;
    std::vector<SP<CFrogColorManagementSurface>> m_vSurfaces;

    friend class CFrogColorManager;
    friend class CFrogColorManagementSurface;
};

namespace PROTO {
    inline UP<CFrogColorManagementProtocol> frogColorManagement;
};
