#pragma once

#include "../defines.hpp"

class CWaylandResource {
  public:
    CWaylandResource(wl_client* client, const wl_interface* wlInterface, uint32_t version, uint32_t id, bool destroyInDestructor = true);
    ~CWaylandResource();

    bool         good();
    wl_resource* resource();
    uint32_t     version();
    void         blockDestroy(bool block);

    void         setImplementation(const void* impl, void* data, wl_resource_destroy_func_t df);

    wl_listener  m_liResourceDestroy; // private but has to be public
    void         onResourceDestroy();

  private:
    bool         m_bDestroyInDestructor = false;
    bool         m_bImplementationSet   = false;
    bool         m_bDefunct             = false; // m_liResourceDestroy fired
    wl_client*   m_pWLClient            = nullptr;
    wl_resource* m_pWLResource          = nullptr;
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