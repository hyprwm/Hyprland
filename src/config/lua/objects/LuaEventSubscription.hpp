#pragma once

#include "LuaObjectHelpers.hpp"
#include "../LuaEventHandler.hpp"

namespace Config::Lua::Objects {
    class CLuaEventSubscription : public ILuaObjectWrapper {
      public:
        void        setup(lua_State* L) override;
        static void push(lua_State* L, CLuaEventHandler* handler, uint64_t handle);
    };
}
