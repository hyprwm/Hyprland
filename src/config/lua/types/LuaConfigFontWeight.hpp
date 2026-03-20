#pragma once

#include "LuaConfigValue.hpp"
#include "../../shared/complex/ComplexDataTypes.hpp"

namespace Config::Lua {
    class CLuaConfigFontWeight : public ILuaConfigValue {
      public:
        CLuaConfigFontWeight(Config::INTEGER def = 400);
        virtual ~CLuaConfigFontWeight() = default;

        virtual SParseError               parse(lua_State* s);
        virtual const std::type_info*     underlying();
        virtual void const*               data();
        virtual std::string               toString();

        const CFontWeightConfigValueData& parsed();

      private:
        CFontWeightConfigValueData m_data;
    };
};
