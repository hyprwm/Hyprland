#pragma once

#include "../../../helpers/memory/Memory.hpp"

extern "C" {
#include <lauxlib.h>
}

namespace Config::Lua::Objects {

    template <typename T>
    inline int gcRef(lua_State* L) {
        sc<T*>(lua_touserdata(L, 1))->~T();
        return 0;
    }

    inline int readOnlyNewIndex(lua_State* L) {
        return luaL_error(L, "attempt to modify read-only hl object");
    }

    inline void registerMetatable(lua_State* L, const char* name, lua_CFunction indexFn, lua_CFunction gcFn) {
        luaL_newmetatable(L, name);
        lua_pushcfunction(L, indexFn);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, readOnlyNewIndex);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, gcFn);
        lua_setfield(L, -2, "__gc");
        lua_pop(L, 1);
    }

    class ILuaObjectWrapper {
      public:
        virtual ~ILuaObjectWrapper()     = default;
        virtual void setup(lua_State* L) = 0;
    };

}
