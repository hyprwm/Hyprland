#pragma once

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
    SP<CZwlrForeignToplevelHandleV1> m_resource;
    PHLWINDOWREF                     m_window;
    bool                             m_closed        = false;
    MONITORID                        m_lastMonitorID = MONITOR_INVALID;

    void                             sendMonitor(PHLMONITOR pMonitor);
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
    SP<CZwlrForeignToplevelManagerV1>          m_resource;
    bool                                       m_finished = false;
    PHLWINDOWREF                               m_lastFocus; // READ-ONLY

    SP<CForeignToplevelHandleWlr>              handleForWindow(PHLWINDOW pWindow);

    std::vector<WP<CForeignToplevelHandleWlr>> m_handles;
};

class CForeignToplevelWlrProtocol : public IWaylandProtocol {
  public:
    CForeignToplevelWlrProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    PHLWINDOW    windowFromHandleResource(wl_resource* res);

  private:
    void onManagerResourceDestroy(CForeignToplevelWlrManager* mgr);
    void destroyHandle(CForeignToplevelHandleWlr* handle);
    bool windowValidForForeign(PHLWINDOW pWindow);

    //
    std::vector<UP<CForeignToplevelWlrManager>> m_managers;
    std::vector<SP<CForeignToplevelHandleWlr>>  m_handles;

    friend class CForeignToplevelWlrManager;
    friend class CForeignToplevelHandleWlr;
};

namespace PROTO {
    inline UP<CForeignToplevelWlrProtocol> foreignToplevelWlr;
};
