#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "ext-foreign-toplevel-list-v1.hpp"

class CWindow;

class CForeignToplevelHandle {
  public:
    CForeignToplevelHandle(SP<CExtForeignToplevelHandleV1> resource_, CWindow* pWindow);

    bool     good();
    CWindow* window();

  private:
    SP<CExtForeignToplevelHandleV1> resource;
    CWindow*                        pWindow = nullptr;
    bool                            closed  = false;

    friend class CForeignToplevelList;
};

class CForeignToplevelList {
  public:
    CForeignToplevelList(SP<CExtForeignToplevelListV1> resource_);

    void onMap(CWindow* pWindow);
    void onTitle(CWindow* pWindow);
    void onClass(CWindow* pWindow);
    void onUnmap(CWindow* pWindow);

    bool good();

  private:
    SP<CExtForeignToplevelListV1>           resource;
    bool                                    finished = false;

    SP<CForeignToplevelHandle>              handleForWindow(CWindow* pWindow);

    std::vector<WP<CForeignToplevelHandle>> handles;
};

class CForeignToplevelProtocol : public IWaylandProtocol {
  public:
    CForeignToplevelProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void onManagerResourceDestroy(CForeignToplevelList* mgr);
    void destroyHandle(CForeignToplevelHandle* handle);

    //
    std::vector<UP<CForeignToplevelList>>   m_vManagers;
    std::vector<SP<CForeignToplevelHandle>> m_vHandles;

    friend class CForeignToplevelList;
    friend class CForeignToplevelHandle;
};

namespace PROTO {
    inline UP<CForeignToplevelProtocol> foreignToplevel;
};
