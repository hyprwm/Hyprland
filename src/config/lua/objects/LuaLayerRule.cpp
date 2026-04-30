#include "LuaLayerRule.hpp"

#include "../../../desktop/rule/Engine.hpp"

#include <string_view>

using namespace Config::Lua;

static constexpr const char* MT = "HL.LayerRule";

//
static int layerRuleEq(lua_State* L) {
    const auto* lhs = sc<WP<Desktop::Rule::CLayerRule>*>(luaL_checkudata(L, 1, MT));
    const auto* rhs = sc<WP<Desktop::Rule::CLayerRule>*>(luaL_checkudata(L, 2, MT));

    lua_pushboolean(L, lhs->lock() == rhs->lock());
    return 1;
}

static int layerRuleToString(lua_State* L) {
    const auto* ref  = sc<WP<Desktop::Rule::CLayerRule>*>(luaL_checkudata(L, 1, MT));
    const auto  rule = ref->lock();

    if (!rule)
        lua_pushstring(L, "HL.LayerRule(expired)");
    else
        lua_pushfstring(L, "HL.LayerRule(%p)", rule.get());

    return 1;
}

static int layerRuleSetEnabled(lua_State* L) {
    auto* ref = sc<WP<Desktop::Rule::CLayerRule>*>(luaL_checkudata(L, 1, MT));
    luaL_checktype(L, 2, LUA_TBOOLEAN);

    const auto rule = ref->lock();
    if (!rule)
        return 0;

    rule->setEnabled(lua_toboolean(L, 2));
    Desktop::Rule::ruleEngine()->updateAllRules();
    return 0;
}

static int layerRuleIsEnabled(lua_State* L) {
    auto*      ref = sc<WP<Desktop::Rule::CLayerRule>*>(luaL_checkudata(L, 1, MT));

    const auto rule = ref->lock();
    if (!rule) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushboolean(L, rule->isEnabled());
    return 1;
}

static int layerRuleIndex(lua_State* L) {
    luaL_checkudata(L, 1, MT);
    const std::string_view key = luaL_checkstring(L, 2);

    if (key == "set_enabled")
        lua_pushcfunction(L, layerRuleSetEnabled);
    else if (key == "is_enabled")
        lua_pushcfunction(L, layerRuleIsEnabled);
    else
        lua_pushnil(L);

    return 1;
}

void Objects::CLuaLayerRule::setup(lua_State* L) {
    registerMetatable(L, MT, layerRuleIndex, gcRef<WP<Desktop::Rule::CLayerRule>>, layerRuleEq, layerRuleToString);
}

void Objects::CLuaLayerRule::push(lua_State* L, const SP<Desktop::Rule::CLayerRule>& rule) {
    new (lua_newuserdata(L, sizeof(WP<Desktop::Rule::CLayerRule>))) WP<Desktop::Rule::CLayerRule>(rule);
    luaL_getmetatable(L, MT);
    lua_setmetatable(L, -2);
}
