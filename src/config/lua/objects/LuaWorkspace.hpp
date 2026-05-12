#pragma once

#include "LuaObjectHelpers.hpp"
#include "../../../desktop/DesktopTypes.hpp"

namespace Config::Lua::Objects {
    class CLuaWorkspace : public ILuaObjectWrapper {
      public:
        void        setup(lua_State* L) override;
        static void push(lua_State* L, PHLWORKSPACE ws);
    };
}
