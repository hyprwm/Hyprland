#include "LuaBindings.hpp"
#include "ConfigManager.hpp"
#include "LuaEventHandler.hpp"
#include "objects/LuaWindow.hpp"
#include "objects/LuaWorkspace.hpp"
#include "objects/LuaMonitor.hpp"
#include "objects/LuaLayerSurface.hpp"

#include "../../Compositor.hpp"
#include "../../helpers/Monitor.hpp"
#include "../../managers/KeybindManager.hpp"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Config::Lua::Bindings;
using namespace Config::Lua;

static void pushDispatcher(lua_State* L, const char* handler, std::string arg) {
    lua_newtable(L);
    lua_pushstring(L, handler);
    lua_setfield(L, -2, "_h");
    lua_pushstring(L, arg.c_str());
    lua_setfield(L, -2, "_a");
}

// converts a Lua string-or-number at stack position idx to std::string.
static std::string argStr(lua_State* L, int idx) {
    if (lua_type(L, idx) == LUA_TNUMBER)
        return std::to_string((long long)lua_tonumber(L, idx));
    size_t      n = 0;
    const char* s = luaL_checklstring(L, idx, &n);
    return {s, n};
}

// returns def if the argument is absent or nil.
static std::string optStr(lua_State* L, int idx, const char* def = "") {
    if (lua_isnoneornil(L, idx))
        return def;
    return argStr(L, idx);
}

static int hlBind(lua_State* L) {
    auto*       mgr = static_cast<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

    const char* mods = luaL_checkstring(L, 1);
    const char* key  = luaL_checkstring(L, 2);

    if (!lua_istable(L, 3)) {
        mgr->addError("hl.bind: dispatcher must be a table returned by a dispatcher factory (e.g. hl.exec_cmd(), hl.window.close())");
        return 0;
    }

    lua_getfield(L, 3, "_h");
    lua_getfield(L, 3, "_a");

    if (!lua_isstring(L, -2)) {
        mgr->addError("hl.bind: invalid dispatcher table - missing handler");
        lua_pop(L, 2);
        return 0;
    }

    SKeybind kb;
    kb.key         = key;
    kb.modmask     = g_pKeybindManager->stringToModMask(mods);
    kb.handler     = lua_tostring(L, -2);
    kb.arg         = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
    kb.submap.name = mgr->m_currentSubmap;
    lua_pop(L, 2);

    if (lua_istable(L, 4)) {
        auto getBool = [&](const char* field) -> bool {
            lua_getfield(L, 4, field);
            bool v = lua_toboolean(L, -1);
            lua_pop(L, 1);
            return v;
        };
        kb.mouse        = getBool("mouse");
        kb.repeat       = getBool("repeat");
        kb.locked       = getBool("locked");
        kb.release      = getBool("release");
        kb.nonConsuming = getBool("nonConsuming");
        kb.transparent  = getBool("transparent");
        kb.ignoreMods   = getBool("ignoreMods");
        kb.dontInhibit  = getBool("dontInhibit");
    }

    g_pKeybindManager->addKeybind(kb);
    return 0;
}

static int hlDefineSubmap(lua_State* L) {
    auto*       mgr  = static_cast<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));
    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    std::string prev     = mgr->m_currentSubmap;
    mgr->m_currentSubmap = name;

    lua_pushvalue(L, 2);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        mgr->addError(std::string("hl.define_submap: error in submap \"") + name + "\": " + lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    mgr->m_currentSubmap = prev;
    return 0;
}

static int hlExecCmd(lua_State* L) {
    pushDispatcher(L, "exec", argStr(L, 1));
    return 1;
}

static int hlExecRaw(lua_State* L) {
    pushDispatcher(L, "execr", argStr(L, 1));
    return 1;
}

static int hlExit(lua_State* L) {
    pushDispatcher(L, "exit", "");
    return 1;
}

static int hlSubmap(lua_State* L) {
    pushDispatcher(L, "submap", argStr(L, 1));
    return 1;
}

static int hlPass(lua_State* L) {
    pushDispatcher(L, "pass", argStr(L, 1));
    return 1;
}

static int hlSendShortcut(lua_State* L) {
    pushDispatcher(L, "sendshortcut", argStr(L, 1));
    return 1;
}

static int hlSendKeyState(lua_State* L) {
    pushDispatcher(L, "sendkeystate", argStr(L, 1));
    return 1;
}

static int hlLayout(lua_State* L) {
    pushDispatcher(L, "layoutmsg", argStr(L, 1));
    return 1;
}

static int hlDpms(lua_State* L) {
    std::string arg = argStr(L, 1);
    if (!lua_isnoneornil(L, 2))
        arg += " " + argStr(L, 2);
    pushDispatcher(L, "dpms", arg);
    return 1;
}

static int hlEvent(lua_State* L) {
    pushDispatcher(L, "event", argStr(L, 1));
    return 1;
}

static int hlGlobal(lua_State* L) {
    pushDispatcher(L, "global", argStr(L, 1));
    return 1;
}

static int hlForceRendererReload(lua_State* L) {
    pushDispatcher(L, "forcerendererreload", "");
    return 1;
}

static int hlForceIdle(lua_State* L) {
    pushDispatcher(L, "forceidle", "");
    return 1;
}

static int hlPrint(lua_State* L) {
    const int   n = lua_gettop(L);
    std::string out;
    for (int i = 1; i <= n; i++) {
        size_t      len = 0;
        const char* s   = luaL_tolstring(L, i, &len); // honours __tostring
        if (i > 1)
            out += '\t';
        out.append(s, len);
        lua_pop(L, 1);
    }
    Log::logger->log(Log::INFO, "[Lua] {}", out);
    return 0;
}

static int hlGetWindows(lua_State* L) {
    lua_newtable(L);
    int i = 1;
    for (const auto& w : g_pCompositor->m_windows) {
        if (!w->m_isMapped)
            continue;
        Objects::CLuaWindow::push(L, w);
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

static int hlGetWorkspaces(lua_State* L) {
    lua_newtable(L);
    int i = 1;
    for (const auto& wsRef : g_pCompositor->getWorkspaces()) {
        const auto ws = wsRef.lock();
        if (!ws || ws->inert())
            continue;
        Objects::CLuaWorkspace::push(L, ws);
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

static int hlGetMonitors(lua_State* L) {
    lua_newtable(L);
    int i = 1;
    for (const auto& mon : g_pCompositor->m_monitors) {
        Objects::CLuaMonitor::push(L, mon);
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

static int hlGetLayers(lua_State* L) {
    lua_newtable(L);
    int i = 1;
    for (const auto& mon : g_pCompositor->m_monitors) {
        for (const auto& level : mon->m_layerSurfaceLayers) {
            for (const auto& lsRef : level) {
                const auto ls = lsRef.lock();
                if (!ls)
                    continue;
                Objects::CLuaLayerSurface::push(L, ls);
                lua_rawseti(L, -2, i++);
            }
        }
    }
    return 1;
}

static int hlOn(lua_State* L) {
    auto*       mgr       = static_cast<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));
    const char* eventName = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    if (!mgr->m_eventHandler->registerEvent(eventName, ref)) {
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        const auto& known = CLuaEventHandler::knownEvents();
        std::string list;
        for (const auto& e : known)
            list += "\n  " + e;
        mgr->addError(std::string("hl.on: unknown event \"") + eventName + "\". Known events:" + list);
    }

    return 0;
}

static int hlWindowClose(lua_State* L) {
    pushDispatcher(L, "killactive", "");
    return 1;
}

static int hlWindowForceClose(lua_State* L) {
    pushDispatcher(L, "forcekillactive", "");
    return 1;
}

static int hlWindowCloseWindow(lua_State* L) {
    pushDispatcher(L, "closewindow", optStr(L, 1));
    return 1;
}

static int hlWindowKillWindow(lua_State* L) {
    pushDispatcher(L, "killwindow", optStr(L, 1));
    return 1;
}

static int hlWindowSignal(lua_State* L) {
    pushDispatcher(L, "signal", argStr(L, 1));
    return 1;
}

static int hlWindowSignalWindow(lua_State* L) {
    pushDispatcher(L, "signalwindow", argStr(L, 1) + "," + argStr(L, 2));
    return 1;
}

static int hlWindowToggleFloat(lua_State* L) {
    pushDispatcher(L, "togglefloating", optStr(L, 1));
    return 1;
}

static int hlWindowSetFloat(lua_State* L) {
    pushDispatcher(L, "setfloating", optStr(L, 1));
    return 1;
}

static int hlWindowSetTiled(lua_State* L) {
    pushDispatcher(L, "settiled", optStr(L, 1));
    return 1;
}

static int hlWindowFullscreen(lua_State* L) {
    pushDispatcher(L, "fullscreen", optStr(L, 1, "0"));
    return 1;
}

static int hlWindowFullscreenState(lua_State* L) {
    pushDispatcher(L, "fullscreenstate", argStr(L, 1) + " " + argStr(L, 2));
    return 1;
}

static int hlWindowPseudo(lua_State* L) {
    pushDispatcher(L, "pseudo", "");
    return 1;
}

static int hlWindowMove(lua_State* L) {
    pushDispatcher(L, "movewindow", optStr(L, 1));
    return 1;
}

static int hlWindowSwap(lua_State* L) {
    pushDispatcher(L, "swapwindow", argStr(L, 1));
    return 1;
}

static int hlWindowCenter(lua_State* L) {
    pushDispatcher(L, "centerwindow", optStr(L, 1));
    return 1;
}

static int hlWindowCycleNext(lua_State* L) {
    pushDispatcher(L, "cyclenext", optStr(L, 1));
    return 1;
}

static int hlWindowSwapNext(lua_State* L) {
    pushDispatcher(L, "swapnext", optStr(L, 1));
    return 1;
}

static int hlWindowFocus(lua_State* L) {
    pushDispatcher(L, "focuswindow", argStr(L, 1));
    return 1;
}

static int hlWindowFocusByClass(lua_State* L) {
    pushDispatcher(L, "focuswindowbyclass", argStr(L, 1));
    return 1;
}

static int hlWindowTag(lua_State* L) {
    std::string arg = argStr(L, 1);
    if (!lua_isnoneornil(L, 2))
        arg += " " + argStr(L, 2);
    pushDispatcher(L, "tagwindow", arg);
    return 1;
}

static int hlWindowToggleSwallow(lua_State* L) {
    pushDispatcher(L, "toggleswallow", "");
    return 1;
}

static int hlWindowResizeActive(lua_State* L) {
    pushDispatcher(L, "resizeactive", argStr(L, 1) + " " + argStr(L, 2));
    return 1;
}

static int hlWindowMoveActive(lua_State* L) {
    pushDispatcher(L, "moveactive", argStr(L, 1) + " " + argStr(L, 2));
    return 1;
}

static int hlWindowResizePixel(lua_State* L) {
    pushDispatcher(L, "resizewindowpixel", argStr(L, 1) + "," + argStr(L, 2));
    return 1;
}

static int hlWindowMovePixel(lua_State* L) {
    pushDispatcher(L, "movewindowpixel", argStr(L, 1) + "," + argStr(L, 2));
    return 1;
}

static int hlWindowPin(lua_State* L) {
    pushDispatcher(L, "pin", "");
    return 1;
}

static int hlWindowBringToTop(lua_State* L) {
    pushDispatcher(L, "bringactivetotop", "");
    return 1;
}

static int hlWindowAlterZOrder(lua_State* L) {
    pushDispatcher(L, "alterzorder", argStr(L, 1));
    return 1;
}

static int hlWindowSetProp(lua_State* L) {
    pushDispatcher(L, "setprop", argStr(L, 1) + " " + argStr(L, 2) + " " + argStr(L, 3));
    return 1;
}

static int hlWindowMoveIntoGroup(lua_State* L) {
    pushDispatcher(L, "moveintogroup", argStr(L, 1));
    return 1;
}

static int hlWindowMoveOutOfGroup(lua_State* L) {
    pushDispatcher(L, "moveoutofgroup", optStr(L, 1));
    return 1;
}

static int hlWindowMoveWindowOrGroup(lua_State* L) {
    pushDispatcher(L, "movewindoworgroup", argStr(L, 1));
    return 1;
}

static int hlWindowDenyFromGroup(lua_State* L) {
    pushDispatcher(L, "denywindowfromgroup", optStr(L, 1, "toggle"));
    return 1;
}

static int hlWindowDrag(lua_State* L) {
    pushDispatcher(L, "mouse", "movewindow");
    return 1;
}

static int hlWindowResize(lua_State* L) {
    pushDispatcher(L, "mouse", "resizewindow");
    return 1;
}

static int hlFocusDirection(lua_State* L) {
    pushDispatcher(L, "movefocus", argStr(L, 1));
    return 1;
}

static int hlFocusMonitor(lua_State* L) {
    pushDispatcher(L, "focusmonitor", argStr(L, 1));
    return 1;
}

static int hlFocusUrgentOrLast(lua_State* L) {
    pushDispatcher(L, "focusurgentorlast", "");
    return 1;
}

static int hlFocusCurrentOrLast(lua_State* L) {
    pushDispatcher(L, "focuscurrentorlast", "");
    return 1;
}

static int hlWorkspaceGo(lua_State* L) {
    pushDispatcher(L, "workspace", argStr(L, 1));
    return 1;
}

static int hlWorkspaceMoveWindow(lua_State* L) {
    pushDispatcher(L, "movetoworkspace", argStr(L, 1));
    return 1;
}

static int hlWorkspaceMoveWindowSilent(lua_State* L) {
    pushDispatcher(L, "movetoworkspacesilent", argStr(L, 1));
    return 1;
}

static int hlWorkspaceSpecial(lua_State* L) {
    pushDispatcher(L, "togglespecialworkspace", optStr(L, 1));
    return 1;
}

static int hlWorkspaceRename(lua_State* L) {
    pushDispatcher(L, "renameworkspace", argStr(L, 1) + " " + argStr(L, 2));
    return 1;
}

static int hlWorkspaceMoveToMonitor(lua_State* L) {
    pushDispatcher(L, "moveworkspacetomonitor", argStr(L, 1) + " " + argStr(L, 2));
    return 1;
}

static int hlWorkspaceMoveCurrentToMonitor(lua_State* L) {
    pushDispatcher(L, "movecurrentworkspacetomonitor", argStr(L, 1));
    return 1;
}

static int hlWorkspaceFocusOnMonitor(lua_State* L) {
    pushDispatcher(L, "focusworkspaceoncurrentmonitor", argStr(L, 1));
    return 1;
}

static int hlWorkspaceSwapMonitors(lua_State* L) {
    pushDispatcher(L, "swapactiveworkspaces", argStr(L, 1) + " " + argStr(L, 2));
    return 1;
}

static int hlCursorMoveToCorner(lua_State* L) {
    pushDispatcher(L, "movecursortocorner", argStr(L, 1));
    return 1;
}

static int hlCursorMove(lua_State* L) {
    pushDispatcher(L, "movecursor", argStr(L, 1) + " " + argStr(L, 2));
    return 1;
}

static int hlGroupToggle(lua_State* L) {
    pushDispatcher(L, "togglegroup", "");
    return 1;
}

static int hlGroupChangeActive(lua_State* L) {
    pushDispatcher(L, "changegroupactive", argStr(L, 1));
    return 1;
}

static int hlGroupMoveWindow(lua_State* L) {
    pushDispatcher(L, "movegroupwindow", argStr(L, 1));
    return 1;
}

static int hlGroupLock(lua_State* L) {
    pushDispatcher(L, "lockgroups", optStr(L, 1, "toggle"));
    return 1;
}

static int hlGroupLockActive(lua_State* L) {
    pushDispatcher(L, "lockactivegroup", optStr(L, 1, "toggle"));
    return 1;
}

static int hlGroupIgnoreLock(lua_State* L) {
    pushDispatcher(L, "setignoregrouplock", optStr(L, 1, "toggle"));
    return 1;
}

void Bindings::registerBindings(lua_State* L, CConfigManager* mgr) {
    lua_getglobal(L, "hl");

    // hl.on
    lua_pushlightuserdata(L, mgr);
    lua_pushcclosure(L, hlOn, 1);
    lua_setfield(L, -2, "on");

    // hl.bind
    lua_pushlightuserdata(L, mgr);
    lua_pushcclosure(L, hlBind, 1);
    lua_setfield(L, -2, "bind");

    // hl.define_submap
    lua_pushlightuserdata(L, mgr);
    lua_pushcclosure(L, hlDefineSubmap, 1);
    lua_setfield(L, -2, "define_submap");

    // Top-level dispatcher factories
    lua_pushcfunction(L, hlExecCmd);
    lua_setfield(L, -2, "exec_cmd");
    lua_pushcfunction(L, hlExecRaw);
    lua_setfield(L, -2, "exec_raw");
    lua_pushcfunction(L, hlExit);
    lua_setfield(L, -2, "exit");
    lua_pushcfunction(L, hlSubmap);
    lua_setfield(L, -2, "submap");
    lua_pushcfunction(L, hlPass);
    lua_setfield(L, -2, "pass");
    lua_pushcfunction(L, hlSendShortcut);
    lua_setfield(L, -2, "send_shortcut");
    lua_pushcfunction(L, hlSendKeyState);
    lua_setfield(L, -2, "send_key_state");
    lua_pushcfunction(L, hlLayout);
    lua_setfield(L, -2, "layout");
    lua_pushcfunction(L, hlDpms);
    lua_setfield(L, -2, "dpms");
    lua_pushcfunction(L, hlEvent);
    lua_setfield(L, -2, "event");
    lua_pushcfunction(L, hlGlobal);
    lua_setfield(L, -2, "global");
    lua_pushcfunction(L, hlForceRendererReload);
    lua_setfield(L, -2, "force_renderer_reload");
    lua_pushcfunction(L, hlForceIdle);
    lua_setfield(L, -2, "force_idle");
    lua_pushcfunction(L, hlGetWindows);
    lua_setfield(L, -2, "get_windows");
    lua_pushcfunction(L, hlGetWorkspaces);
    lua_setfield(L, -2, "get_workspaces");
    lua_pushcfunction(L, hlGetMonitors);
    lua_setfield(L, -2, "get_monitors");
    lua_pushcfunction(L, hlGetLayers);
    lua_setfield(L, -2, "get_layers");

    // hl.window
    lua_newtable(L);
    lua_pushcfunction(L, hlWindowClose);
    lua_setfield(L, -2, "close");
    lua_pushcfunction(L, hlWindowForceClose);
    lua_setfield(L, -2, "force_close");
    lua_pushcfunction(L, hlWindowCloseWindow);
    lua_setfield(L, -2, "close_window");
    lua_pushcfunction(L, hlWindowKillWindow);
    lua_setfield(L, -2, "kill_window");
    lua_pushcfunction(L, hlWindowSignal);
    lua_setfield(L, -2, "signal");
    lua_pushcfunction(L, hlWindowSignalWindow);
    lua_setfield(L, -2, "signal_window");
    lua_pushcfunction(L, hlWindowToggleFloat);
    lua_setfield(L, -2, "toggle_float");
    lua_pushcfunction(L, hlWindowSetFloat);
    lua_setfield(L, -2, "set_float");
    lua_pushcfunction(L, hlWindowSetTiled);
    lua_setfield(L, -2, "set_tiled");
    lua_pushcfunction(L, hlWindowFullscreen);
    lua_setfield(L, -2, "fullscreen");
    lua_pushcfunction(L, hlWindowFullscreenState);
    lua_setfield(L, -2, "fullscreen_state");
    lua_pushcfunction(L, hlWindowPseudo);
    lua_setfield(L, -2, "pseudo");
    lua_pushcfunction(L, hlWindowMove);
    lua_setfield(L, -2, "move");
    lua_pushcfunction(L, hlWindowSwap);
    lua_setfield(L, -2, "swap");
    lua_pushcfunction(L, hlWindowCenter);
    lua_setfield(L, -2, "center");
    lua_pushcfunction(L, hlWindowCycleNext);
    lua_setfield(L, -2, "cycle_next");
    lua_pushcfunction(L, hlWindowSwapNext);
    lua_setfield(L, -2, "swap_next");
    lua_pushcfunction(L, hlWindowFocus);
    lua_setfield(L, -2, "focus");
    lua_pushcfunction(L, hlWindowFocusByClass);
    lua_setfield(L, -2, "focus_by_class");
    lua_pushcfunction(L, hlWindowTag);
    lua_setfield(L, -2, "tag");
    lua_pushcfunction(L, hlWindowToggleSwallow);
    lua_setfield(L, -2, "toggle_swallow");
    lua_pushcfunction(L, hlWindowResizeActive);
    lua_setfield(L, -2, "resize_active");
    lua_pushcfunction(L, hlWindowMoveActive);
    lua_setfield(L, -2, "move_active");
    lua_pushcfunction(L, hlWindowResizePixel);
    lua_setfield(L, -2, "resize_pixel");
    lua_pushcfunction(L, hlWindowMovePixel);
    lua_setfield(L, -2, "move_pixel");
    lua_pushcfunction(L, hlWindowPin);
    lua_setfield(L, -2, "pin");
    lua_pushcfunction(L, hlWindowBringToTop);
    lua_setfield(L, -2, "bring_to_top");
    lua_pushcfunction(L, hlWindowAlterZOrder);
    lua_setfield(L, -2, "alter_zorder");
    lua_pushcfunction(L, hlWindowSetProp);
    lua_setfield(L, -2, "set_prop");
    lua_pushcfunction(L, hlWindowMoveIntoGroup);
    lua_setfield(L, -2, "move_into_group");
    lua_pushcfunction(L, hlWindowMoveOutOfGroup);
    lua_setfield(L, -2, "move_out_of_group");
    lua_pushcfunction(L, hlWindowMoveWindowOrGroup);
    lua_setfield(L, -2, "move_window_or_group");
    lua_pushcfunction(L, hlWindowDenyFromGroup);
    lua_setfield(L, -2, "deny_from_group");
    lua_pushcfunction(L, hlWindowDrag);
    lua_setfield(L, -2, "drag");
    lua_pushcfunction(L, hlWindowResize);
    lua_setfield(L, -2, "resize");
    lua_setfield(L, -2, "window");

    // hl.focus
    lua_newtable(L);
    lua_pushcfunction(L, hlFocusDirection);
    lua_setfield(L, -2, "direction");
    lua_pushcfunction(L, hlFocusMonitor);
    lua_setfield(L, -2, "monitor");
    lua_pushcfunction(L, hlFocusUrgentOrLast);
    lua_setfield(L, -2, "urgent_or_last");
    lua_pushcfunction(L, hlFocusCurrentOrLast);
    lua_setfield(L, -2, "current_or_last");
    lua_setfield(L, -2, "focus");

    // hl.workspace
    lua_newtable(L);
    lua_pushcfunction(L, hlWorkspaceGo);
    lua_setfield(L, -2, "go");
    lua_pushcfunction(L, hlWorkspaceMoveWindow);
    lua_setfield(L, -2, "move_window");
    lua_pushcfunction(L, hlWorkspaceMoveWindowSilent);
    lua_setfield(L, -2, "move_window_silent");
    lua_pushcfunction(L, hlWorkspaceSpecial);
    lua_setfield(L, -2, "special");
    lua_pushcfunction(L, hlWorkspaceRename);
    lua_setfield(L, -2, "rename");
    lua_pushcfunction(L, hlWorkspaceMoveToMonitor);
    lua_setfield(L, -2, "move_to_monitor");
    lua_pushcfunction(L, hlWorkspaceMoveCurrentToMonitor);
    lua_setfield(L, -2, "move_current_to_monitor");
    lua_pushcfunction(L, hlWorkspaceFocusOnMonitor);
    lua_setfield(L, -2, "focus_on_monitor");
    lua_pushcfunction(L, hlWorkspaceSwapMonitors);
    lua_setfield(L, -2, "swap_monitors");
    lua_setfield(L, -2, "workspace");

    // hl.cursor
    lua_newtable(L);
    lua_pushcfunction(L, hlCursorMoveToCorner);
    lua_setfield(L, -2, "move_to_corner");
    lua_pushcfunction(L, hlCursorMove);
    lua_setfield(L, -2, "move");
    lua_setfield(L, -2, "cursor");

    // hl.group
    lua_newtable(L);
    lua_pushcfunction(L, hlGroupToggle);
    lua_setfield(L, -2, "toggle");
    lua_pushcfunction(L, hlGroupChangeActive);
    lua_setfield(L, -2, "change_active");
    lua_pushcfunction(L, hlGroupMoveWindow);
    lua_setfield(L, -2, "move_window");
    lua_pushcfunction(L, hlGroupLock);
    lua_setfield(L, -2, "lock");
    lua_pushcfunction(L, hlGroupLockActive);
    lua_setfield(L, -2, "lock_active");
    lua_pushcfunction(L, hlGroupIgnoreLock);
    lua_setfield(L, -2, "ignore_lock");
    lua_setfield(L, -2, "group");

    lua_pop(L, 1); // pop hl

    // override the global print() to route through the Hyprland logger.
    lua_pushcfunction(L, hlPrint);
    lua_setglobal(L, "print");
}
