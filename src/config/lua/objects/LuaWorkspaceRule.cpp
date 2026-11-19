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


// ERSTARR TODO: shared lua helpers - unify this and CLuaConfigCssGap::push(...)
static void pushCssGap(lua_State* L, const Config::CCssGapData& gap) {
    lua_createtable(L, 0, 4);

    lua_pushinteger(L, sc<lua_Integer>(gap.m_top));
    lua_setfield(L, -2, "top");
    lua_pushinteger(L, sc<lua_Integer>(gap.m_right));
    lua_setfield(L, -2, "right");
    lua_pushinteger(L, sc<lua_Integer>(gap.m_bottom));
    lua_setfield(L, -2, "bottom");
    lua_pushinteger(L, sc<lua_Integer>(gap.m_left));
    lua_setfield(L, -2, "left");
}

static void pushOptionalGap(lua_State* L, const std::optional<Config::CCssGapData>& gap) {
    if (!gap) {
        lua_pushnil(L);
        return;
    }

    pushCssGap(L, *gap);
}

static void pushOptionalBool(lua_State* L, const std::optional<bool>& value) {
    if (!value) {
        lua_pushnil(L);
        return;
    }

    lua_pushboolean(L, *value);
}

static void pushOptionalString(lua_State* L, const std::optional<std::string>& value) {
    if (!value) {
        lua_pushnil(L);
        return;
    }

    lua_pushstring(L, value->c_str());
}

static int workspaceRuleIndex(lua_State* L) {
    const auto* ref = sc<WP<Config::CWorkspaceRule>*>(luaL_checkudata(L, 1, MT));
    const std::string_view key = luaL_checkstring(L, 2);

    if (key == "set_enabled") {
        lua_pushcfunction(L, workspaceRuleSetEnabled);
        return 1;
    }

    if (key == "is_enabled") {
        lua_pushcfunction(L, workspaceRuleIsEnabled);
        return 1;
    }

    const auto rule = ref->lock();
    if (!rule) {
        lua_pushnil(L);
        return 1;
    }

    if (key == "workspace")
        lua_pushstring(L, rule->m_workspaceString.c_str());
    else if (key == "enabled")
        lua_pushboolean(L, rule->isEnabled());
    else if (key == "monitor") {
        if (rule->m_monitor.empty())
            lua_pushnil(L);
        else
            lua_pushstring(L, rule->m_monitor.c_str());
    } else if (key == "default")
        pushOptionalBool(L, rule->m_isDefault);
    else if (key == "persistent")
        pushOptionalBool(L, rule->m_isPersistent);
    else if (key == "decorate")
        pushOptionalBool(L, rule->m_decorate);
    else if (key == "no_border")
        pushOptionalBool(L, rule->m_noBorder);
    else if (key == "no_rounding")
        pushOptionalBool(L, rule->m_noRounding);
    else if (key == "no_shadow")
        pushOptionalBool(L, rule->m_noShadow);
    else if (key == "border_size") {
        if (rule->m_borderSize)
            lua_pushinteger(L, sc<lua_Integer>(*rule->m_borderSize));
        else
            lua_pushnil(L);
    } else if (key == "gaps_in")
        pushOptionalGap(L, rule->m_gapsIn);
    else if (key == "gaps_out")
        pushOptionalGap(L, rule->m_gapsOut);
    else if (key == "float_gaps")
        pushOptionalGap(L, rule->m_floatGaps);
    else if (key == "default_name")
        pushOptionalString(L, rule->m_defaultName);
    else if (key == "layout")
        pushOptionalString(L, rule->m_layout);
    else if (key == "animation")
        pushOptionalString(L, rule->m_animationStyle);
    else if (key == "on_created_empty")
        pushOptionalString(L, rule->m_onCreatedEmptyRunCmd);
    else if (key == "layout_opts") {
        if (rule->m_layoutopts.empty()) {
            lua_pushnil(L);
            return 1;
        }

        lua_newtable(L);
        for (const auto& [optKey, optValue] : rule->m_layoutopts) {
            lua_pushstring(L, optValue.c_str());
            lua_setfield(L, -2, optKey.c_str());
        }
    } else
        lua_pushnil(L);

    return 1;
}

void Objects::CLuaWorkspaceRule::setup(lua_State* L) {
    registerMetatable(L, MT, workspaceRuleIndex, gcRef<WP<Config::CWorkspaceRule>>, workspaceRuleEq, workspaceRuleToString);
}

void Objects::CLuaWorkspaceRule::push(lua_State* L, const SP<Config::CWorkspaceRule>& rule) {
    if (!rule) {
        lua_pushnil(L);
        return;
    }

    new (lua_newuserdata(L, sizeof(WP<Config::CWorkspaceRule>))) WP<Config::CWorkspaceRule>(rule);
    luaL_getmetatable(L, MT);
    lua_setmetatable(L, -2);
}
