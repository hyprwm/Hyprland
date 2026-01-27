#pragma once

#include <vector>
#include "WaylandProtocol.hpp"
#include "xdg-toplevel-tag-v1.hpp"

class CXDGToplevelResource;

class CXDGToplevelTagManagerResource {
  public:
    CXDGToplevelTagManagerResource(UP<CXdgToplevelTagManagerV1>&& resource);

    bool good();

  private:
    UP<CXdgToplevelTagManagerV1> m_resource;
};

class CXDGToplevelTagProtocol : public IWaylandProtocol {
  public:
    CXDGToplevelTagProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyResource(CXDGToplevelTagManagerResource* res);

    //
    std::vector<UP<CXDGToplevelTagManagerResource>> m_managers;

    friend class CXDGToplevelTagManagerResource;
};

namespace PROTO {
    inline UP<CXDGToplevelTagProtocol> xdgTag;
};
