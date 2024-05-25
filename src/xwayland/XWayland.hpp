#pragma once

#include <memory>
#include "../helpers/signal/Signal.hpp"

#include "XSurface.hpp"

#ifndef NO_XWAYLAND
#include "Server.hpp"
#include "XWM.hpp"
#else
class CXWaylandServer;
class CXWM;
#endif

class CXWayland {
  public:
    CXWayland();

#ifndef NO_XWAYLAND
    std::unique_ptr<CXWaylandServer> pServer;
    std::unique_ptr<CXWM>            pWM;
#endif

    void setCursor(unsigned char* pixData, uint32_t stride, const Vector2D& size, const Vector2D& hotspot);

    struct {
        CSignal newSurface;
    } events;
};

inline std::unique_ptr<CXWayland> g_pXWayland;

#define HYPRATOM(name)                                                                                                                                                             \
    { name, 0 }
inline std::unordered_map<std::string, uint32_t> HYPRATOMS = {
    HYPRATOM("_NET_SUPPORTED"),
    HYPRATOM("_NET_SUPPORTING_WM_CHECK"),
    HYPRATOM("_NET_WM_NAME"),
    HYPRATOM("_NET_WM_VISIBLE_NAME"),
    HYPRATOM("_NET_WM_MOVERESIZE"),
    HYPRATOM("_NET_WM_STATE_STICKY"),
    HYPRATOM("_NET_WM_STATE_FULLSCREEN"),
    HYPRATOM("_NET_WM_STATE_DEMANDS_ATTENTION"),
    HYPRATOM("_NET_WM_STATE_MODAL"),
    HYPRATOM("_NET_WM_STATE_HIDDEN"),
    HYPRATOM("_NET_WM_STATE_FOCUSED"),
    HYPRATOM("_NET_WM_STATE"),
    HYPRATOM("_NET_WM_WINDOW_TYPE"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_NORMAL"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_DOCK"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_DIALOG"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_UTILITY"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_TOOLBAR"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_SPLASH"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_MENU"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_POPUP_MENU"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_TOOLTIP"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_NOTIFICATION"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_COMBO"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_DND"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_DESKTOP"),
    HYPRATOM("_NET_WM_STATE_MAXIMIZED_HORZ"),
    HYPRATOM("_NET_WM_STATE_MAXIMIZED_VERT"),
    HYPRATOM("_NET_WM_DESKTOP"),
    HYPRATOM("_NET_WM_STRUT_PARTIAL"),
    HYPRATOM("_NET_CLIENT_LIST"),
    HYPRATOM("_NET_CLIENT_LIST_STACKING"),
    HYPRATOM("_NET_CURRENT_DESKTOP"),
    HYPRATOM("_NET_NUMBER_OF_DESKTOPS"),
    HYPRATOM("_NET_DESKTOP_NAMES"),
    HYPRATOM("_NET_DESKTOP_VIEWPORT"),
    HYPRATOM("_NET_ACTIVE_WINDOW"),
    HYPRATOM("_NET_CLOSE_WINDOW"),
    HYPRATOM("_NET_MOVERESIZE_WINDOW"),
    HYPRATOM("_NET_WM_USER_TIME"),
    HYPRATOM("_NET_STARTUP_ID"),
    HYPRATOM("_NET_WORKAREA"),
    HYPRATOM("_NET_WM_ICON"),
    HYPRATOM("_NET_WM_CM_S0"),
    HYPRATOM("WM_PROTOCOLS"),
    HYPRATOM("WM_HINTS"),
    HYPRATOM("WM_DELETE_WINDOW"),
    HYPRATOM("UTF8_STRING"),
    HYPRATOM("WM_STATE"),
    HYPRATOM("WM_CLIENT_LEADER"),
    HYPRATOM("WM_TAKE_FOCUS"),
    HYPRATOM("WM_NORMAL_HINTS"),
    HYPRATOM("WM_SIZE_HINTS"),
    HYPRATOM("WM_WINDOW_ROLE"),
    HYPRATOM("_NET_REQUEST_FRAME_EXTENTS"),
    HYPRATOM("_NET_FRAME_EXTENTS"),
    HYPRATOM("_MOTIF_WM_HINTS"),
    HYPRATOM("WM_CHANGE_STATE"),
    HYPRATOM("_NET_SYSTEM_TRAY_OPCODE"),
    HYPRATOM("_NET_SYSTEM_TRAY_COLORS"),
    HYPRATOM("_NET_SYSTEM_TRAY_VISUAL"),
    HYPRATOM("_NET_SYSTEM_TRAY_ORIENTATION"),
    HYPRATOM("_XEMBED_INFO"),
    HYPRATOM("MANAGER"),
    HYPRATOM("XdndSelection"),
    HYPRATOM("XdndAware"),
    HYPRATOM("XdndStatus"),
    HYPRATOM("XdndPosition"),
    HYPRATOM("XdndEnter"),
    HYPRATOM("XdndLeave"),
    HYPRATOM("XdndDrop"),
    HYPRATOM("XdndFinished"),
    HYPRATOM("XdndProxy"),
    HYPRATOM("XdndTypeList"),
    HYPRATOM("XdndActionMove"),
    HYPRATOM("XdndActionCopy"),
    HYPRATOM("XdndActionAsk"),
    HYPRATOM("XdndActionPrivate"),
    HYPRATOM("CLIPBOARD"),
    HYPRATOM("PRIMARY"),
    HYPRATOM("_WL_SELECTION"),
    HYPRATOM("CLIPBOARD_MANAGER"),
    HYPRATOM("WINDOW"),
    HYPRATOM("WM_S0"),
    HYPRATOM("WL_SURFACE_ID"),
    HYPRATOM("WL_SURFACE_SERIAL"),
    HYPRATOM("TARGETS"),
    HYPRATOM("TIMESTAMP"),
    HYPRATOM("DELETE"),
    HYPRATOM("TEXT"),
    HYPRATOM("INCR"),
};