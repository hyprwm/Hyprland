#pragma once

#include "LuaConfigValue.hpp"

#include "../../../helpers/math/Expression.hpp"

namespace Config::Lua {
    class CLuaConfigExpressionVec2 : public ILuaConfigValue {
      public:
        CLuaConfigExpressionVec2(Math::SExpressionVec2 def = {});
        virtual ~CLuaConfigExpressionVec2() = default;

        virtual SParseError           parse(lua_State* s);
        virtual const std::type_info* underlying();
        virtual void const*           data();
        virtual std::string           toString();
        virtual void                  push(lua_State* s);
        virtual void                  reset();

        const Math::SExpressionVec2&  parsed();

      private:
        Math::SExpressionVec2 m_default;
        Math::SExpressionVec2 m_data;
    };
};
