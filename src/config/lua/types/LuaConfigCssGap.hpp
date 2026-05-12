#pragma once

#include "LuaConfigValue.hpp"
#include "../../shared/complex/ComplexDataTypes.hpp"

#include <optional>

namespace Config::Lua {
    class CLuaConfigCssGap : public ILuaConfigValue {
      public:
        CLuaConfigCssGap(Config::INTEGER def, std::optional<int64_t> min = std::nullopt, std::optional<int64_t> max = std::nullopt);
        virtual ~CLuaConfigCssGap() = default;

        virtual SParseError           parse(lua_State* s);
        virtual const std::type_info* underlying();
        virtual void const*           data();
        virtual std::string           toString();
        virtual void                  push(lua_State* s);
        virtual void                  reset();

        const CCssGapData&            parsed();

      private:
        Config::INTEGER        m_default = 0;
        CCssGapData            m_data;
        std::optional<int64_t> m_min, m_max;
    };
};
