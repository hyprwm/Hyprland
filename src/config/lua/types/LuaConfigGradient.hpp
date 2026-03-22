#pragma once

#include "LuaConfigValue.hpp"
#include "../../shared/complex/ComplexDataTypes.hpp"

namespace Config::Lua {
    class CLuaConfigGradient : public ILuaConfigValue {
      public:
        CLuaConfigGradient(CHyprColor def);
        virtual ~CLuaConfigGradient() = default;

        virtual SParseError           parse(lua_State* s);
        virtual const std::type_info* underlying();
        virtual void const*           data();
        virtual std::string           toString();

      private:
        CGradientValueData m_data;
    };
};
