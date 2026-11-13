#pragma once

#include "LuaConfigValue.hpp"
#include "../../shared/complex/ComplexDataTypes.hpp"

#include <optional>

namespace Config::Lua {
    class CLuaConfigCssGap : public ILuaConfigValue {
      public:
        CLuaConfigCssGap(double def, std::optional<double> min = std::nullopt, std::optional<double> max = std::nullopt);
        virtual ~CLuaConfigCssGap() = default;

        virtual SParseError           parse(lua_State* s);
        virtual const std::type_info* underlying();
        virtual void const*           data();
        virtual std::string           toString();
        virtual void                  push(lua_State* s);
        virtual void                  reset();

        const CCssGapData&            parsed();

      private:
        double                m_default = 0;
        CCssGapData           m_data;
        std::optional<double> m_min, m_max;
    };
};
