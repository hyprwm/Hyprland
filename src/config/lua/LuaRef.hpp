#pragma once

#include "../../helpers/memory/Memory.hpp"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace Config::Lua {

    struct SLuaStateLifetime {
        lua_State* state = nullptr; // nulled before the state is closed
    };

    class CLuaRef {
      public:
        CLuaRef() = default;

        CLuaRef(SP<SLuaStateLifetime> lifetime, lua_State* L, int idx);
        ~CLuaRef();

        CLuaRef(CLuaRef&& other) noexcept;
        CLuaRef& operator=(CLuaRef&& other) noexcept;

        CLuaRef(const CLuaRef&)              = delete;
        CLuaRef&   operator=(const CLuaRef&) = delete;

        lua_State* state() const;
        bool       push() const;
        int        id() const; // for display only

      private:
        void                  release();

        SP<SLuaStateLifetime> m_lifetime;
        int                   m_ref = LUA_NOREF;
    };
}
