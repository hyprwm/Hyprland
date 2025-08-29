#pragma once

#include <vector>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "xdg-decoration-unstable-v1.hpp"

class CXDGDecoration {
  public:
    CXDGDecoration(SP<CZxdgToplevelDecorationV1> resource_, wl_resource* toplevel);

    bool         good();
    wl_resource* toplevelResource();

    uint32_t     mostRecentlySent      = 0;
    uint32_t     mostRecentlyRequested = 0;

  private:
    SP<CZxdgToplevelDecorationV1> m_resource;
    wl_resource*                  m_toplevelResource = nullptr; // READ-ONLY.
};

class CXDGDecorationProtocol : public IWaylandProtocol {
  public:
    CXDGDecorationProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyDecoration(CXDGDecoration* decoration);
    void onGetDecoration(CZxdgDecorationManagerV1* pMgr, uint32_t id, wl_resource* xdgToplevel);

    //
    std::vector<UP<CZxdgDecorationManagerV1>>            m_managers;
    std::unordered_map<wl_resource*, UP<CXDGDecoration>> m_decorations; // xdg_toplevel -> deco

    friend class CXDGDecoration;
};

namespace PROTO {
    inline UP<CXDGDecorationProtocol> xdgDecoration;
};
