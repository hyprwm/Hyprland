#pragma once

#include "LuaObjectHelpers.hpp"

#include <cstdint>

class IHID;
namespace Aquamarine {
    class ISwitch;
}

namespace Config::Lua::Objects {
    class CLuaDevice : public ILuaObjectWrapper {
      public:
        void        setup(lua_State* L) override;
        static void push(lua_State* L, WP<IHID> device);
        static void push(lua_State* L, WP<Aquamarine::ISwitch> device, uintptr_t address);
    };
}
