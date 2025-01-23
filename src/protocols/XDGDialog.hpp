#pragma once

#include <vector>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "xdg-dialog-v1.hpp"

class CXDGToplevelResource;

class CXDGDialogV1Resource {
  public:
    CXDGDialogV1Resource(SP<CXdgDialogV1> resource_, SP<CXDGToplevelResource> toplevel_);

    bool good();

    bool modal = false;

  private:
    SP<CXdgDialogV1>         resource;
    WP<CXDGToplevelResource> toplevel;

    void                     updateWindow();
};

class CXDGWmDialogManagerResource {
  public:
    CXDGWmDialogManagerResource(SP<CXdgWmDialogV1> resource_);

    bool good();

  private:
    SP<CXdgWmDialogV1> resource;
};

class CXDGDialogProtocol : public IWaylandProtocol {
  public:
    CXDGDialogProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyResource(CXDGWmDialogManagerResource* res);
    void destroyResource(CXDGDialogV1Resource* res);

    //
    std::vector<SP<CXDGWmDialogManagerResource>> m_vManagers;
    std::vector<SP<CXDGDialogV1Resource>>        m_vDialogs;

    friend class CXDGWmDialogManagerResource;
    friend class CXDGDialogV1Resource;
};

namespace PROTO {
    inline UP<CXDGDialogProtocol> xdgDialog;
};
