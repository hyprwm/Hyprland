#pragma once

#include "../defines.hpp"

#define RESOURCE_OR_BAIL(resname)                                                                                                                                                  \
    const auto resname = (CWaylandResource*)wl_resource_get_user_data(resource);                                                                                                   \
    if (!resname)                                                                                                                                                                  \
        return;

class CWaylandResource {
  public:
    CWaylandResource(wl_client* client, const wl_interface* wlInterface, uint32_t version, uint32_t id);
    ~CWaylandResource();

    bool         good();
    wl_resource* resource();
    uint32_t     version();

    void         setImplementation(const void* impl, wl_resource_destroy_func_t df);

    wl_listener  m_liResourceDestroy; // private but has to be public
    void         markDefunct();

    void*        data();
    void         setData(void* data);

  private:
    bool         m_bImplementationSet = false;
    bool         m_bDefunct           = false; // m_liResourceDestroy fired
    wl_client*   m_pWLClient          = nullptr;
    wl_resource* m_pWLResource        = nullptr;
    void*        m_pData              = nullptr;
};

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