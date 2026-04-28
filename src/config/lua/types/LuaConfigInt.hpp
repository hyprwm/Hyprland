#pragma once

#include "LuaConfigValue.hpp"

#include <optional>
#include <unordered_map>
#include <string>

namespace Config::Lua {
    class CLuaConfigInt : public ILuaConfigValue {
      public:
        CLuaConfigInt(Config::INTEGER def, std::optional<Config::INTEGER> min = std::nullopt, std::optional<Config::INTEGER> max = std::nullopt,
                      std::optional<std::unordered_map<std::string, Config::INTEGER>> map = std::nullopt);
        virtual ~CLuaConfigInt() = default;

        virtual SParseError           parse(lua_State* s);
        virtual const std::type_info* underlying();
        virtual void const*           data();
        virtual std::string           toString();
        virtual void                  push(lua_State* s);
        virtual void                  reset();

        const Config::INTEGER&        parsed();

      private:
        Config::INTEGER                                                 m_default = 0;
        Config::INTEGER                                                 m_data    = 0;
        std::optional<Config::INTEGER>                                  m_min, m_max;
        std::optional<std::unordered_map<std::string, Config::INTEGER>> m_map;
    };
};
