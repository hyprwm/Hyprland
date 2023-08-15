#pragma once

#include "WaylandProtocol.hpp"
#include <optional>

class CMonitor;

struct SXDGOutput {
    CMonitor*                         monitor = nullptr;
    std::unique_ptr<CWaylandResource> resource;

    std::optional<Vector2D>           overridePosition;

    wl_client*                        client     = nullptr;
    bool                              isXWayland = false;
};

class CXDGOutputProtocol : public IWaylandProtocol {
  public:
    CXDGOutputProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         onManagerResourceDestroy(wl_resource* res);
    void         onOutputResourceDestroy(wl_resource* res);
    void         onManagerGetXDGOutput(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* outputResource);

  private:
    void                                           updateOutputDetails(SXDGOutput* pOutput);
    void                                           updateAllOutputs();

    std::vector<std::unique_ptr<CWaylandResource>> m_vManagerResources;
    std::vector<std::unique_ptr<SXDGOutput>>       m_vXDGOutputs;
};