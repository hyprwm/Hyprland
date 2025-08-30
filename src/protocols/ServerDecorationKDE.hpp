#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "kde-server-decoration.hpp"

class CWLSurfaceResource;

class CServerDecorationKDE {
  public:
    CServerDecorationKDE(SP<COrgKdeKwinServerDecoration> resource_, SP<CWLSurfaceResource> surf);

    SP<CWLSurfaceResource> m_surf;

    uint32_t               mostRecentlySent      = 0;
    uint32_t               mostRecentlyRequested = 0;

    bool                   good();

    uint32_t               kdeDefaultModeCSD();
    uint32_t               kdeModeOnRequestCSD(uint32_t modeRequestedByClient);
    uint32_t               kdeModeOnReleaseCSD();

  private:
    SP<COrgKdeKwinServerDecoration> m_resource;
};

class CServerDecorationKDEProtocol : public IWaylandProtocol {
  public:
    CServerDecorationKDEProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    uint32_t     kdeDefaultManagerModeCSD();

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyResource(CServerDecorationKDE* deco);

    void createDecoration(COrgKdeKwinServerDecorationManager* pMgr, uint32_t id, wl_resource* surf);

    //
    std::vector<UP<COrgKdeKwinServerDecorationManager>> m_managers;
    std::vector<UP<CServerDecorationKDE>>               m_decos;

    friend class CServerDecorationKDE;
};

namespace PROTO {
    inline UP<CServerDecorationKDEProtocol> serverDecorationKDE;
};
