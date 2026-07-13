#pragma once

#include "LuaObjectHelpers.hpp"
#include "../../shared/workspace/WorkspaceRule.hpp"

namespace Config::Lua::Objects {
    class CLuaWorkspaceRule : public ILuaObjectWrapper {
      public:
        void        setup(lua_State* L) override;
        static void push(lua_State* L, const SP<Config::CWorkspaceRule>& rule);
    };
}
