#pragma once

#include "LuaConfigValue.hpp"

#include <optional>
#include <functional>
#include <expected>

namespace Config::Lua {
    class CLuaConfigString : public ILuaConfigValue {
      public:
        CLuaConfigString(Config::STRING def, std::optional<std::function<std::expected<void, std::string>(const Config::STRING&)>>&& validator = std::nullopt);
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
        Config::STRING                                                                     m_default = "[[EMPTY]]";
        Config::STRING                                                                     m_data    = "[[EMPTY]]";
        std::optional<std::function<std::expected<void, std::string>(const std::string&)>> m_validator;
    };
};
