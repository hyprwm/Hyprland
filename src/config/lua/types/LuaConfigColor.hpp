#pragma once

#include "LuaConfigValue.hpp"

namespace Config::Lua {
    class CLuaConfigColor : public ILuaConfigValue {
      public:
        CLuaConfigColor(Config::INTEGER def);
        virtual ~CLuaConfigColor() = default;

        virtual SParseError           parse(lua_State* s);
        virtual const std::type_info* underlying();
        virtual void const*           data();
        virtual std::string           toString();

        const Config::INTEGER&        parsed();

      private:
        // colors are stored as ints for compat
        Config::INTEGER m_data = 0;
    };
};
