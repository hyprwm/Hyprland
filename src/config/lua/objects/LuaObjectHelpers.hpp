#pragma once

#include "../bindings/LuaBindingsInternal.hpp"

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
        return Config::Lua::Bindings::Internal::configError(L, "attempt to modify read-only hl object");
    }

    inline void registerMetatable(lua_State* L, const char* name, lua_CFunction indexFn, lua_CFunction gcFn, lua_CFunction eqFn = nullptr, lua_CFunction toStringFn = nullptr) {
        luaL_newmetatable(L, name);
        lua_pushcfunction(L, indexFn);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, readOnlyNewIndex);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, gcFn);
        lua_setfield(L, -2, "__gc");

        if (eqFn) {
            lua_pushcfunction(L, eqFn);
            lua_setfield(L, -2, "__eq");
        }

        if (toStringFn) {
            lua_pushcfunction(L, toStringFn);
            lua_setfield(L, -2, "__tostring");
        }

        lua_pop(L, 1);
    }

    class ILuaObjectWrapper {
      public:
        virtual ~ILuaObjectWrapper()     = default;
        virtual void setup(lua_State* L) = 0;
    };

}
