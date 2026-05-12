#pragma once

#include "LuaObjectHelpers.hpp"
#include "../../../desktop/rule/windowRule/WindowRule.hpp"

namespace Config::Lua::Objects {
    class CLuaWindowRule : public ILuaObjectWrapper {
      public:
        void        setup(lua_State* L) override;
        static void push(lua_State* L, const SP<Desktop::Rule::CWindowRule>& rule);
    };
}
