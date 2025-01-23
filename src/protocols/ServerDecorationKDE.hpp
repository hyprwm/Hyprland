#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "kde-server-decoration.hpp"

class CWLSurfaceResource;

class CServerDecorationKDE {
  public:
    CServerDecorationKDE(SP<COrgKdeKwinServerDecoration> resource_, SP<CWLSurfaceResource> surf);

    bool good();

  private:
    SP<COrgKdeKwinServerDecoration> resource;
};

class CServerDecorationKDEProtocol : public IWaylandProtocol {
  public:
    CServerDecorationKDEProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyResource(CServerDecorationKDE* deco);

    void createDecoration(COrgKdeKwinServerDecorationManager* pMgr, uint32_t id, wl_resource* surf);

    //
    std::vector<UP<COrgKdeKwinServerDecorationManager>> m_vManagers;
    std::vector<UP<CServerDecorationKDE>>               m_vDecos;

    friend class CServerDecorationKDE;
};

namespace PROTO {
    inline UP<CServerDecorationKDEProtocol> serverDecorationKDE;
};
