#include "LuaLayerSurface.hpp"
#include "LuaMonitor.hpp"
#include "LuaObjectHelpers.hpp"

#include "../../../desktop/view/LayerSurface.hpp"

#include <format>
#include <string_view>

using namespace Config::Lua;

static constexpr const char* MT = "HL.LayerSurface";

//
static int layerSurfaceEq(lua_State* L) {
    const auto* lhs = sc<PHLLSREF*>(luaL_checkudata(L, 1, MT));
    const auto* rhs = sc<PHLLSREF*>(luaL_checkudata(L, 2, MT));

    lua_pushboolean(L, lhs->lock() == rhs->lock());
    return 1;
}

static int layerSurfaceToString(lua_State* L) {
    const auto* ref = sc<PHLLSREF*>(luaL_checkudata(L, 1, MT));
    const auto  ls  = ref->lock();

    if (!ls)
        lua_pushstring(L, "HL.LayerSurface(expired)");
    else
        lua_pushfstring(L, "HL.LayerSurface(%p)", ls.get());

    return 1;
}

static int layerSurfaceIndex(lua_State* L) {
    auto*      ref = sc<PHLLSREF*>(luaL_checkudata(L, 1, MT));
    const auto ls  = ref->lock();
    if (!ls) {
        Log::logger->log(Log::DEBUG, "[lua] Tried to access an expired object");
        lua_pushnil(L);
        return 1;
    }

    const std::string_view key = luaL_checkstring(L, 2);

    if (key == "address")
        lua_pushstring(L, std::format("0x{:x}", reinterpret_cast<uintptr_t>(ls.get())).c_str());
    else if (key == "x")
        lua_pushinteger(L, ls->m_geometry.x);
    else if (key == "y")
        lua_pushinteger(L, ls->m_geometry.y);
    else if (key == "w")
        lua_pushinteger(L, ls->m_geometry.width);
    else if (key == "h")
        lua_pushinteger(L, ls->m_geometry.height);
    else if (key == "namespace")
        lua_pushstring(L, ls->m_namespace.c_str());
    else if (key == "pid")
        lua_pushinteger(L, sc<lua_Integer>(ls->getPID()));
    else if (key == "monitor") {
        const auto mon = ls->m_monitor.lock();
        if (mon)
            Objects::CLuaMonitor::push(L, mon);
        else
            lua_pushnil(L);
    } else if (key == "mapped")
        lua_pushboolean(L, ls->m_mapped);
    else if (key == "layer")
        lua_pushinteger(L, sc<lua_Integer>(ls->m_layer));
    else if (key == "interactivity")
        lua_pushinteger(L, sc<lua_Integer>(ls->m_interactivity));
    else if (key == "above_fullscreen")
        lua_pushboolean(L, ls->m_aboveFullscreen);
    else
        lua_pushnil(L);

    return 1;
}

void Objects::CLuaLayerSurface::setup(lua_State* L) {
    registerMetatable(L, MT, layerSurfaceIndex, gcRef<PHLLSREF>, layerSurfaceEq, layerSurfaceToString);
}

void Objects::CLuaLayerSurface::push(lua_State* L, PHLLS ls) {
    new (lua_newuserdata(L, sizeof(PHLLSREF))) PHLLSREF(ls ? ls->m_self : nullptr);
    luaL_getmetatable(L, MT);
    lua_setmetatable(L, -2);
}
