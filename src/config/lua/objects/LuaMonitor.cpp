#include "LuaMonitor.hpp"
#include "LuaWorkspace.hpp"
#include "LuaObjectHelpers.hpp"

#include "../../../helpers/Monitor.hpp"
#include "../../../desktop/state/FocusState.hpp"

#include <string_view>

static constexpr const char* MT = "HL.Monitor";

static int                   monitorIndex(lua_State* L) {
    auto*      ref = static_cast<PHLMONITORREF*>(luaL_checkudata(L, 1, MT));
    const auto mon = ref->lock();
    if (!mon) {
        Log::logger->log(Log::DEBUG, "[lua] Tried to access an expired object");
        lua_pushnil(L);
        return 1;
    }

    const std::string_view key = luaL_checkstring(L, 2);

    if (key == "id")
        lua_pushinteger(L, static_cast<lua_Integer>(mon->m_id));
    else if (key == "name")
        lua_pushstring(L, mon->m_name.c_str());
    else if (key == "description")
        lua_pushstring(L, mon->m_shortDescription.c_str());
    else if (key == "width")
        lua_pushinteger(L, static_cast<int>(mon->m_pixelSize.x));
    else if (key == "height")
        lua_pushinteger(L, static_cast<int>(mon->m_pixelSize.y));
    else if (key == "refresh_rate")
        lua_pushnumber(L, mon->m_refreshRate);
    else if (key == "x")
        lua_pushinteger(L, static_cast<int>(mon->m_position.x));
    else if (key == "y")
        lua_pushinteger(L, static_cast<int>(mon->m_position.y));
    else if (key == "active_workspace") {
        if (mon->m_activeWorkspace)
            Config::Lua::Objects::CLuaWorkspace::push(L, mon->m_activeWorkspace);
        else
            lua_pushnil(L);
    } else if (key == "scale")
        lua_pushnumber(L, mon->m_scale);
    else if (key == "transform")
        lua_pushinteger(L, static_cast<int>(mon->m_transform));
    else if (key == "dpms_status")
        lua_pushboolean(L, mon->m_dpmsStatus);
    else if (key == "focused")
        lua_pushboolean(L, mon == Desktop::focusState()->monitor());
    else
        lua_pushnil(L);

    return 1;
}

namespace Config::Lua::Objects {
    void CLuaMonitor::setup(lua_State* L) {
        registerMetatable(L, MT, monitorIndex, gcRef<PHLMONITORREF>);
    }

    void CLuaMonitor::push(lua_State* L, PHLMONITOR mon) {
        new (lua_newuserdata(L, sizeof(PHLMONITORREF))) PHLMONITORREF(mon ? mon->m_self : nullptr);
        luaL_getmetatable(L, MT);
        lua_setmetatable(L, -2);
    }
}
