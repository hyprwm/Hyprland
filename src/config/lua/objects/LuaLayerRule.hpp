#pragma once

#include "LuaObjectHelpers.hpp"
#include "../../../desktop/rule/layerRule/LayerRule.hpp"

namespace Config::Lua::Objects {
    class CLuaLayerRule : public ILuaObjectWrapper {
      public:
        void        setup(lua_State* L) override;
        static void push(lua_State* L, const SP<Desktop::Rule::CLayerRule>& rule);
    };
}
