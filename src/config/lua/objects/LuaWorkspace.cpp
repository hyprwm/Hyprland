#include "LuaWorkspace.hpp"
#include "LuaMonitor.hpp"
#include "LuaObjectHelpers.hpp"

#include "../../../desktop/Workspace.hpp"
#include "../../../helpers/Monitor.hpp"

#include <string_view>

using namespace Config::Lua;

static constexpr const char* MT = "HL.Workspace";

//
static int workspaceEq(lua_State* L) {
    const auto* lhs = sc<PHLWORKSPACEREF*>(luaL_checkudata(L, 1, MT));
    const auto* rhs = sc<PHLWORKSPACEREF*>(luaL_checkudata(L, 2, MT));

    lua_pushboolean(L, lhs->lock() == rhs->lock());
    return 1;
}

static int workspaceToString(lua_State* L) {
    const auto* ref = sc<PHLWORKSPACEREF*>(luaL_checkudata(L, 1, MT));
    const auto  ws  = ref->lock();

    if (!ws || ws->inert())
        lua_pushstring(L, "HL.Workspace(expired)");
    else
        lua_pushfstring(L, "HL.Workspace(%d:%s)", ws->m_id, ws->m_name.c_str());

    return 1;
}

static int workspaceIndex(lua_State* L) {
    auto*      ref = sc<PHLWORKSPACEREF*>(luaL_checkudata(L, 1, MT));
    const auto ws  = ref->lock();
    if (!ws || ws->inert()) {
        Log::logger->log(Log::DEBUG, "[lua] Tried to access an expired object");
        lua_pushnil(L);
        return 1;
    }

    const std::string_view key = luaL_checkstring(L, 2);

    if (key == "id")
        lua_pushinteger(L, sc<lua_Integer>(ws->m_id));
    else if (key == "name")
        lua_pushstring(L, ws->m_name.c_str());
    else if (key == "monitor") {
        const auto mon = ws->m_monitor.lock();
        if (mon)
            Objects::CLuaMonitor::push(L, mon);
        else
            lua_pushnil(L);
    } else if (key == "windows")
        lua_pushinteger(L, sc<lua_Integer>(ws->getWindows()));
    else if (key == "visible")
        lua_pushboolean(L, ws->isVisible());
    else if (key == "special")
        lua_pushboolean(L, ws->m_isSpecialWorkspace);
    else if (key == "active") {
        const auto mon = ws->m_monitor.lock();
        lua_pushboolean(L, mon && (mon->m_activeWorkspace == ws || mon->m_activeSpecialWorkspace == ws));
    } else if (key == "has_urgent")
        lua_pushboolean(L, ws->hasUrgentWindow());
    else if (key == "fullscreen_mode")
        lua_pushinteger(L, sc<lua_Integer>(ws->m_fullscreenMode));
    else if (key == "has_fullscreen")
        lua_pushboolean(L, ws->m_hasFullscreenWindow);
    else if (key == "is_persistent")
        lua_pushboolean(L, ws->isPersistent());
    else
        lua_pushnil(L);

    return 1;
}

void Objects::CLuaWorkspace::setup(lua_State* L) {
    registerMetatable(L, MT, workspaceIndex, gcRef<PHLWORKSPACEREF>, workspaceEq, workspaceToString);
}

void Objects::CLuaWorkspace::push(lua_State* L, PHLWORKSPACE ws) {
    new (lua_newuserdata(L, sizeof(PHLWORKSPACEREF))) PHLWORKSPACEREF(ws ? ws->m_self : nullptr);
    luaL_getmetatable(L, MT);
    lua_setmetatable(L, -2);
}
