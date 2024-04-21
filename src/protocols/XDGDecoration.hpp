#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "xdg-decoration-unstable-v1.hpp"

class CXDGDecoration {
  public:
    CXDGDecoration(SP<CZxdgToplevelDecorationV1> resource_, wl_resource* toplevel);

    bool         good();
    wl_resource* toplevelResource();

  private:
    SP<CZxdgToplevelDecorationV1> resource;
    wl_resource*                  pToplevelResource = nullptr; // READ-ONLY.
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
    std::vector<UP<CZxdgDecorationManagerV1>>            m_vManagers;
    std::unordered_map<wl_resource*, UP<CXDGDecoration>> m_mDecorations; // xdg_toplevel -> deco

    friend class CXDGDecoration;
};

namespace PROTO {
    inline UP<CXDGDecorationProtocol> xdgDecoration;
};
