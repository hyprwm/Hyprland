#include "LuaLayerSurface.hpp"
#include "LuaObjectHelpers.hpp"

#include "../../../desktop/view/LayerSurface.hpp"

#include <format>
#include <string_view>

static constexpr const char* MT = "HL.LayerSurface";

static int                   layerSurfaceIndex(lua_State* L) {
    auto*      ref = static_cast<PHLLSREF*>(luaL_checkudata(L, 1, MT));
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
        lua_pushinteger(L, static_cast<lua_Integer>(ls->getPID()));
    else
        lua_pushnil(L);

    return 1;
}

namespace Config::Lua::Objects {
    void CLuaLayerSurface::setup(lua_State* L) {
        registerMetatable(L, MT, layerSurfaceIndex, gcRef<PHLLSREF>);
    }

    void CLuaLayerSurface::push(lua_State* L, PHLLS ls) {
        new (lua_newuserdata(L, sizeof(PHLLSREF))) PHLLSREF(ls ? ls->m_self : nullptr);
        luaL_getmetatable(L, MT);
        lua_setmetatable(L, -2);
    }
}
