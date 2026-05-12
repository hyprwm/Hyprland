#pragma once

#include "LuaObjectHelpers.hpp"
#include "../../../managers/KeybindManager.hpp"

namespace Config::Lua::Objects {
    class CLuaKeybind : public ILuaObjectWrapper {
      public:
        void        setup(lua_State* L) override;
        static void push(lua_State* L, const SP<SKeybind>& keybind);
    };
}
