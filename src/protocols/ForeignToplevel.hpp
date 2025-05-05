#pragma once

#include <vector>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "desktop/DesktopTypes.hpp"
#include "ext-foreign-toplevel-list-v1.hpp"

class CForeignToplevelHandle {
  public:
    CForeignToplevelHandle(SP<CExtForeignToplevelHandleV1> resource_, PHLWINDOW pWindow);

    bool      good();
    PHLWINDOW window();

  private:
    SP<CExtForeignToplevelHandleV1> m_resource;
    PHLWINDOWREF                    m_window;
    bool                            m_closed = false;

    friend class CForeignToplevelList;
    friend class CForeignToplevelProtocol;
};

class CForeignToplevelList {
  public:
    CForeignToplevelList(SP<CExtForeignToplevelListV1> resource_);

    void onMap(PHLWINDOW pWindow);
    void onTitle(PHLWINDOW pWindow);
    void onClass(PHLWINDOW pWindow);
    void onUnmap(PHLWINDOW pWindow);

    bool good();

  private:
    SP<CExtForeignToplevelListV1>           m_resource;
    bool                                    m_finished = false;

    SP<CForeignToplevelHandle>              handleForWindow(PHLWINDOW pWindow);

    std::vector<WP<CForeignToplevelHandle>> m_handles;
};

class CForeignToplevelProtocol : public IWaylandProtocol {
  public:
    CForeignToplevelProtocol(const wl_interface* iface, const int& ver, const std::string& name);
    PHLWINDOW    windowFromHandleResource(wl_resource* res);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void onManagerResourceDestroy(CForeignToplevelList* mgr);
    void destroyHandle(CForeignToplevelHandle* handle);
    bool windowValidForForeign(PHLWINDOW pWindow);

    //
    std::vector<UP<CForeignToplevelList>>   m_managers;
    std::vector<SP<CForeignToplevelHandle>> m_handles;

    friend class CForeignToplevelList;
    friend class CForeignToplevelHandle;
};

namespace PROTO {
    inline UP<CForeignToplevelProtocol> foreignToplevel;
};
