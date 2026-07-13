#include "LuaWorkspaceRule.hpp"

#include "../../supplementary/propRefresher/PropRefresher.hpp"

#include <string_view>

using namespace Config::Lua;

static constexpr const char* MT = "HL.WorkspaceRule";

//
static int workspaceRuleEq(lua_State* L) {
    const auto* lhs = sc<WP<Config::CWorkspaceRule>*>(luaL_checkudata(L, 1, MT));
    const auto* rhs = sc<WP<Config::CWorkspaceRule>*>(luaL_checkudata(L, 2, MT));

    lua_pushboolean(L, lhs->lock() == rhs->lock());
    return 1;
}

static int workspaceRuleToString(lua_State* L) {
    const auto* ref  = sc<WP<Config::CWorkspaceRule>*>(luaL_checkudata(L, 1, MT));
    const auto  rule = ref->lock();

    if (!rule)
        lua_pushstring(L, "HL.WorkspaceRule(expired)");
    else
        lua_pushfstring(L, "HL.WorkspaceRule(%p)", rule.get());

    return 1;
}

static int workspaceRuleSetEnabled(lua_State* L) {
    auto* ref = sc<WP<Config::CWorkspaceRule>*>(luaL_checkudata(L, 1, MT));
    luaL_checktype(L, 2, LUA_TBOOLEAN);

    const auto rule = ref->lock();
    if (!rule)
        return 0;

    rule->setEnabled(lua_toboolean(L, 2));
    Config::Supplementary::refresher()->scheduleRefresh(Config::Supplementary::REFRESH_MONITOR_STATES | Config::Supplementary::REFRESH_WINDOW_STATES);
    return 0;
}

static int workspaceRuleIsEnabled(lua_State* L) {
    auto*      ref = sc<WP<Config::CWorkspaceRule>*>(luaL_checkudata(L, 1, MT));

    const auto rule = ref->lock();
    if (!rule) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushboolean(L, rule->isEnabled());
    return 1;
}

static int workspaceRuleIndex(lua_State* L) {
    luaL_checkudata(L, 1, MT);
    const std::string_view key = luaL_checkstring(L, 2);

    if (key == "set_enabled")
        lua_pushcfunction(L, workspaceRuleSetEnabled);
    else if (key == "is_enabled")
        lua_pushcfunction(L, workspaceRuleIsEnabled);
    else
        lua_pushnil(L);

    return 1;
}

void Objects::CLuaWorkspaceRule::setup(lua_State* L) {
    registerMetatable(L, MT, workspaceRuleIndex, gcRef<WP<Config::CWorkspaceRule>>, workspaceRuleEq, workspaceRuleToString);
}

void Objects::CLuaWorkspaceRule::push(lua_State* L, const SP<Config::CWorkspaceRule>& rule) {
    new (lua_newuserdata(L, sizeof(WP<Config::CWorkspaceRule>))) WP<Config::CWorkspaceRule>(rule);
    luaL_getmetatable(L, MT);
    lua_setmetatable(L, -2);
}
