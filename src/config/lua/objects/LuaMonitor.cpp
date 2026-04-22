#include "LuaMonitor.hpp"
#include "LuaWorkspace.hpp"
#include "LuaObjectHelpers.hpp"

#include "../../../helpers/Monitor.hpp"
#include "../../../desktop/state/FocusState.hpp"

#include <string_view>

using namespace Config::Lua;

static constexpr const char* MT = "HL.Monitor";

//
static int monitorEq(lua_State* L) {
    const auto* lhs = sc<PHLMONITORREF*>(luaL_checkudata(L, 1, MT));
    const auto* rhs = sc<PHLMONITORREF*>(luaL_checkudata(L, 2, MT));

    lua_pushboolean(L, lhs->lock() == rhs->lock());
    return 1;
}

static int monitorToString(lua_State* L) {
    const auto* ref = sc<PHLMONITORREF*>(luaL_checkudata(L, 1, MT));
    const auto  mon = ref->lock();

    if (!mon)
        lua_pushstring(L, "HL.Monitor(expired)");
    else
        lua_pushfstring(L, "HL.Monitor(%d:%s)", mon->m_id, mon->m_name.c_str());

    return 1;
}

static int monitorIndex(lua_State* L) {
    auto*      ref = sc<PHLMONITORREF*>(luaL_checkudata(L, 1, MT));
    const auto mon = ref->lock();
    if (!mon) {
        Log::logger->log(Log::DEBUG, "[lua] Tried to access an expired object");
        lua_pushnil(L);
        return 1;
    }

    const std::string_view key = luaL_checkstring(L, 2);

    if (key == "id")
        lua_pushinteger(L, sc<lua_Integer>(mon->m_id));
    else if (key == "name")
        lua_pushstring(L, mon->m_name.c_str());
    else if (key == "description")
        lua_pushstring(L, mon->m_shortDescription.c_str());
    else if (key == "width")
        lua_pushinteger(L, sc<int>(mon->m_pixelSize.x));
    else if (key == "height")
        lua_pushinteger(L, sc<int>(mon->m_pixelSize.y));
    else if (key == "refresh_rate")
        lua_pushnumber(L, mon->m_refreshRate);
    else if (key == "x")
        lua_pushinteger(L, sc<int>(mon->m_position.x));
    else if (key == "y")
        lua_pushinteger(L, sc<int>(mon->m_position.y));
    else if (key == "active_workspace") {
        if (mon->m_activeWorkspace)
            Objects::CLuaWorkspace::push(L, mon->m_activeWorkspace);
        else
            lua_pushnil(L);
    } else if (key == "active_special_workspace") {
        if (mon->m_activeSpecialWorkspace)
            Objects::CLuaWorkspace::push(L, mon->m_activeSpecialWorkspace);
        else
            lua_pushnil(L);
    } else if (key == "position") {
        lua_newtable(L);
        lua_pushinteger(L, sc<int>(mon->m_position.x));
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, sc<int>(mon->m_position.y));
        lua_setfield(L, -2, "y");
    } else if (key == "size") {
        lua_newtable(L);
        lua_pushinteger(L, sc<int>(mon->m_pixelSize.x));
        lua_setfield(L, -2, "width");
        lua_pushinteger(L, sc<int>(mon->m_pixelSize.y));
        lua_setfield(L, -2, "height");
    } else if (key == "scale")
        lua_pushnumber(L, mon->m_scale);
    else if (key == "transform")
        lua_pushinteger(L, sc<int>(mon->m_transform));
    else if (key == "dpms_status")
        lua_pushboolean(L, mon->m_dpmsStatus);
    else if (key == "vrr_active")
        lua_pushboolean(L, mon->m_vrrActive);
    else if (key == "is_mirror")
        lua_pushboolean(L, mon->isMirror());
    else if (key == "mirrors") {
        lua_newtable(L);

        int i = 1;
        for (const auto& mirrorRef : mon->m_mirrors) {
            const auto mirror = mirrorRef.lock();
            if (!mirror)
                continue;

            Objects::CLuaMonitor::push(L, mirror);
            lua_rawseti(L, -2, i++);
        }
    } else if (key == "focused")
        lua_pushboolean(L, mon == Desktop::focusState()->monitor());
    else
        lua_pushnil(L);

    return 1;
}

void Objects::CLuaMonitor::setup(lua_State* L) {
    registerMetatable(L, MT, monitorIndex, gcRef<PHLMONITORREF>, monitorEq, monitorToString);
}

void Objects::CLuaMonitor::push(lua_State* L, PHLMONITOR mon) {
    new (lua_newuserdata(L, sizeof(PHLMONITORREF))) PHLMONITORREF(mon ? mon->m_self : nullptr);
    luaL_getmetatable(L, MT);
    lua_setmetatable(L, -2);
}
