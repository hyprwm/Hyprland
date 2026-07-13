#pragma once

#include "LuaConfigValue.hpp"

#include <functional>
#include <expected>

namespace Config::Lua {
    class CLuaConfigString : public ILuaConfigValue {
      public:
        CLuaConfigString(Config::STRING def, std::function<std::expected<void, std::string>(const Config::STRING&)>&& validator = {});
        virtual ~CLuaConfigString() = default;

        virtual SParseError           parse(lua_State* s);
        virtual const std::type_info* underlying();
        virtual void const*           data();
        virtual std::string           toString();
        virtual void                  push(lua_State* s);
        virtual void                  reset();
        virtual Config::STRING        asString();

        const Config::STRING&         parsed();

      private:
        Config::STRING                                                      m_default = "[[EMPTY]]";
        Config::STRING                                                      m_data    = "[[EMPTY]]";
        std::function<std::expected<void, std::string>(const std::string&)> m_validator;
    };
};
