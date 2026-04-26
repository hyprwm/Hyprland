#pragma once

#include "LuaConfigValue.hpp"

namespace Config::Lua {
    class CLuaConfigBool : public ILuaConfigValue {
      public:
        CLuaConfigBool(Config::BOOL def);
        virtual ~CLuaConfigBool() = default;

        virtual SParseError           parse(lua_State* s);
        virtual const std::type_info* underlying();
        virtual void const*           data();
        virtual std::string           toString();
        virtual void                  push(lua_State* s);
        virtual void                  reset();

        const Config::BOOL&           parsed();

      private:
        Config::BOOL m_default = false;
        Config::BOOL m_data    = false;
    };
};
