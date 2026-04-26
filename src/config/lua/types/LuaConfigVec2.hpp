#pragma once

#include "LuaConfigValue.hpp"

#include <optional>
#include <functional>
#include <expected>

namespace Config::Lua {
    class CLuaConfigVec2 : public ILuaConfigValue {
      public:
        CLuaConfigVec2(Config::VEC2 def, std::optional<std::function<std::expected<void, std::string>(const Config::VEC2&)>>&& validator = std::nullopt);
        virtual ~CLuaConfigVec2() = default;

        virtual SParseError           parse(lua_State* s);
        virtual const std::type_info* underlying();
        virtual void const*           data();
        virtual std::string           toString();
        virtual void                  push(lua_State* s);
        virtual void                  reset();

        const Config::VEC2&           parsed();

      private:
        Config::VEC2                                                                        m_default = {};
        Config::VEC2                                                                        m_data    = {};
        std::optional<std::function<std::expected<void, std::string>(const Config::VEC2&)>> m_validator;
    };
};
