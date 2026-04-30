#pragma once

#include "LuaObjectHelpers.hpp"
#include "../../../managers/eventLoop/EventLoopTimer.hpp"

namespace Config::Lua::Objects {
    class CLuaTimer : public ILuaObjectWrapper {
      public:
        void        setup(lua_State* L) override;
        static void push(lua_State* L, const SP<CEventLoopTimer>& timer, int timeoutMs);
    };
}
