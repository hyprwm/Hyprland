#include "LuaWorkspace.hpp"
#include "LuaMonitor.hpp"
#include "LuaObjectHelpers.hpp"

#include "../../../desktop/Workspace.hpp"

#include <string_view>

static constexpr const char* MT = "HL.Workspace";

static int                   workspaceIndex(lua_State* L) {
    auto*      ref = static_cast<PHLWORKSPACEREF*>(luaL_checkudata(L, 1, MT));
    const auto ws  = ref->lock();
    if (!ws || ws->inert()) {
        Log::logger->log(Log::DEBUG, "[lua] Tried to access an expired object");
        lua_pushnil(L);
        return 1;
    }

    const std::string_view key = luaL_checkstring(L, 2);

    if (key == "id")
        lua_pushinteger(L, static_cast<lua_Integer>(ws->m_id));
    else if (key == "name")
        lua_pushstring(L, ws->m_name.c_str());
    else if (key == "monitor") {
        const auto mon = ws->m_monitor.lock();
        if (mon)
            Config::Lua::Objects::CLuaMonitor::push(L, mon);
        else
            lua_pushnil(L);
    } else if (key == "windows")
        lua_pushinteger(L, static_cast<lua_Integer>(ws->getWindows()));
    else if (key == "has_fullscreen")
        lua_pushboolean(L, ws->m_hasFullscreenWindow);
    else if (key == "is_persistent")
        lua_pushboolean(L, ws->isPersistent());
    else
        lua_pushnil(L);

    return 1;
}

namespace Config::Lua::Objects {
    void CLuaWorkspace::setup(lua_State* L) {
        registerMetatable(L, MT, workspaceIndex, gcRef<PHLWORKSPACEREF>);
    }

    void CLuaWorkspace::push(lua_State* L, PHLWORKSPACE ws) {
        new (lua_newuserdata(L, sizeof(PHLWORKSPACEREF))) PHLWORKSPACEREF(ws ? ws->m_self : nullptr);
        luaL_getmetatable(L, MT);
        lua_setmetatable(L, -2);
    }
}
