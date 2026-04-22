#pragma once

#include "LuaConfigValue.hpp"

#include <optional>

namespace Config::Lua {
    class CLuaConfigFloat : public ILuaConfigValue {
      public:
        CLuaConfigFloat(Config::FLOAT def, std::optional<Config::FLOAT> min = std::nullopt, std::optional<Config::FLOAT> max = std::nullopt);
        virtual ~CLuaConfigFloat() = default;

        virtual SParseError           parse(lua_State* s);
        virtual const std::type_info* underlying();
        virtual void const*           data();
        virtual std::string           toString();
        virtual void                  push(lua_State* s);
        virtual void                  reset();

        const Config::FLOAT&          parsed();

      private:
        Config::FLOAT                m_default = 0.F;
        Config::FLOAT                m_data    = 0.F;
        std::optional<Config::FLOAT> m_min, m_max;
    };
};
