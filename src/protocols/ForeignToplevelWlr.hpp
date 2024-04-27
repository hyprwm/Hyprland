#pragma once

#include <memory>
#include <vector>
#include "WaylandProtocol.hpp"
#include "wlr-foreign-toplevel-management-unstable-v1.hpp"

class CWindow;
class CMonitor;

class CForeignToplevelHandleWlr {
  public:
    CForeignToplevelHandleWlr(SP<CZwlrForeignToplevelHandleV1> resource_, PHLWINDOW pWindow);

    bool         good();
    PHLWINDOW    window();
    wl_resource* res();

  private:
    SP<CZwlrForeignToplevelHandleV1> resource;
    PHLWINDOWREF                     pWindow;
    bool                             closed        = false;
    int64_t                          lastMonitorID = -1;

    void                             sendMonitor(CMonitor* pMonitor);
    void                             sendState();

    friend class CForeignToplevelWlrManager;
};

class CForeignToplevelWlrManager {
  public:
    CForeignToplevelWlrManager(SP<CZwlrForeignToplevelManagerV1> resource_);

    void onMap(PHLWINDOW pWindow);
    void onTitle(PHLWINDOW pWindow);
    void onClass(PHLWINDOW pWindow);
    void onMoveMonitor(PHLWINDOW pWindow);
    void onFullscreen(PHLWINDOW pWindow);
    void onNewFocus(PHLWINDOW pWindow);
    void onUnmap(PHLWINDOW pWindow);

    bool good();

  private:
    SP<CZwlrForeignToplevelManagerV1>          resource;
    bool                                       finished = false;
    PHLWINDOWREF                               lastFocus; // READ-ONLY

    SP<CForeignToplevelHandleWlr>              handleForWindow(PHLWINDOW pWindow);

    std::vector<WP<CForeignToplevelHandleWlr>> handles;
};

class CForeignToplevelWlrProtocol : public IWaylandProtocol {
  public:
    CForeignToplevelWlrProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    PHLWINDOW    windowFromHandleResource(wl_resource* res);

  private:
    void onManagerResourceDestroy(CForeignToplevelWlrManager* mgr);
    void destroyHandle(CForeignToplevelHandleWlr* handle);

    //
    std::vector<UP<CForeignToplevelWlrManager>> m_vManagers;
    std::vector<SP<CForeignToplevelHandleWlr>>  m_vHandles;

    friend class CForeignToplevelWlrManager;
    friend class CForeignToplevelHandleWlr;
};

namespace PROTO {
    inline UP<CForeignToplevelWlrProtocol> foreignToplevelWlr;
};
