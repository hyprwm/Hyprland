#include <config/lua/types/LuaConfigBool.hpp>
#include <config/lua/types/LuaConfigColor.hpp>
#include <config/lua/types/LuaConfigCssGap.hpp>
#include <config/lua/types/LuaConfigExpressionVec2.hpp>
#include <config/lua/types/LuaConfigFloat.hpp>
#include <config/lua/types/LuaConfigFontWeight.hpp>
#include <config/lua/types/LuaConfigGradient.hpp>
#include <config/lua/types/LuaConfigInt.hpp>
#include <config/lua/types/LuaConfigString.hpp>
#include <config/lua/types/LuaConfigVec2.hpp>

#include <macros.hpp>

#include <gtest/gtest.h>

#include <format>
#include <numbers>
#include <limits>

extern "C" {
#include <lualib.h>
#include <lauxlib.h>
}

using namespace Config;
using namespace Config::Lua;

namespace {
    class CLuaState {
      public:
        CLuaState() : m_lua(luaL_newstate()) {
            luaL_openlibs(m_lua);
        }

        ~CLuaState() {
            if (m_lua)
                lua_close(m_lua);
        }

        lua_State* get() const {
            return m_lua;
        }

      private:
        lua_State* m_lua = nullptr;
    };

    void pushVec2Table(lua_State* L, double x, double y) {
        lua_createtable(L, 2, 2);
        lua_pushnumber(L, x);
        lua_rawseti(L, -2, 1);
        lua_pushnumber(L, y);
        lua_rawseti(L, -2, 2);
    }

    void pushExpressionVec2Table(lua_State* L, const char* x, const char* y) {
        lua_createtable(L, 2, 2);
        lua_pushstring(L, x);
        lua_rawseti(L, -2, 1);
        lua_pushstring(L, y);
        lua_rawseti(L, -2, 2);
    }
}

TEST(ConfigLuaValueTypes, boolParseAndReset) {
    CLuaState      S;
    const auto     L = S.get();

    CLuaConfigBool value(false);
    const auto*    setByUser = value.setByUserPtr();
    ASSERT_NE(setByUser, nullptr);
    EXPECT_FALSE(value.parsed());
    EXPECT_FALSE(value.setByUser());
    EXPECT_FALSE(*setByUser);

    lua_pushboolean(L, true);
    const auto err = value.parse(L);
    lua_pop(L, 1);

    EXPECT_EQ(err.errorCode, PARSE_ERROR_OK);
    EXPECT_TRUE(value.parsed());
    EXPECT_TRUE(value.setByUser());
    EXPECT_EQ(value.setByUserPtr(), setByUser);
    EXPECT_TRUE(*setByUser);

    value.reset();
    EXPECT_FALSE(value.parsed());
    EXPECT_TRUE(value.setByUser());
    EXPECT_TRUE(*setByUser);

    value.resetSetByUser();
    EXPECT_FALSE(value.setByUser());
    EXPECT_EQ(value.setByUserPtr(), setByUser);
    EXPECT_FALSE(*setByUser);
}

TEST(ConfigLuaValueTypes, boolBadType) {
    CLuaState      S;
    const auto     L = S.get();

    CLuaConfigBool value(false);

    lua_pushinteger(L, 2);
    const auto err = value.parse(L);
    lua_pop(L, 1);

    EXPECT_EQ(err.errorCode, PARSE_ERROR_BAD_TYPE);
    EXPECT_NE(err.message.find("requires a bool"), std::string::npos);
}

TEST(ConfigLuaValueTypes, intBooleanCastAndRangeAndMap) {
    CLuaState     S;
    const auto    L = S.get();

    CLuaConfigInt ranged(0, 0, 10);

    lua_pushboolean(L, true);
    auto err = ranged.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(ranged.parsed(), 1);

    lua_pushinteger(L, 11);
    err = ranged.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OUT_OF_RANGE);

    CLuaConfigInt mapped(0, std::nullopt, std::nullopt, std::unordered_map<std::string, INTEGER>{{"auto", -1}, {"on", 1}});

    lua_pushstring(L, "auto");
    err = mapped.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(mapped.parsed(), -1);

    lua_pushstring(L, "missing");
    err = mapped.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_BAD_VALUE);
    EXPECT_NE(err.message.find("unknown string value"), std::string::npos);
}

TEST(ConfigLuaValueTypes, floatParseAndRange) {
    CLuaState       S;
    const auto      L = S.get();

    CLuaConfigFloat value(0.F, -1.F, 1.F);

    lua_pushnumber(L, 0.5);
    auto err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OK);
    EXPECT_FLOAT_EQ(value.parsed(), 0.5F);

    lua_pushinteger(L, 2);
    err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OUT_OF_RANGE);
}

TEST(ConfigLuaValueTypes, stringValidatorAndEmptyPush) {
    CLuaState        S;
    const auto       L = S.get();

    CLuaConfigString validated("default", std::function<std::expected<void, std::string>(const STRING&)>{[](const STRING& s) -> std::expected<void, std::string> {
                                   if (s.starts_with("ok:"))
                                       return {};
                                   return std::unexpected("must start with ok:");
                               }});

    lua_pushstring(L, "ok:value");
    auto err = validated.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(validated.parsed(), "ok:value");

    lua_pushstring(L, "bad");
    err = validated.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_BAD_VALUE);
    EXPECT_NE(err.message.find("must start with ok:"), std::string::npos);

    CLuaConfigString emptyDefault(STRVAL_EMPTY);
    emptyDefault.push(L);
    ASSERT_TRUE(lua_isstring(L, -1));
    EXPECT_STREQ(lua_tostring(L, -1), "");
    lua_pop(L, 1);
}

TEST(ConfigLuaValueTypes, colorParseAndPush) {
    CLuaState       S;
    const auto      L = S.get();

    CLuaConfigColor value(0);

    lua_pushstring(L, "0xFF00FF00");
    auto err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(value.parsed(), 0xFF00FF00);

    value.push(L);
    ASSERT_TRUE(lua_isstring(L, -1));
    EXPECT_STREQ(lua_tostring(L, -1), "0xFF00FF00");
    lua_pop(L, 1);

    lua_pushstring(L, "@@@notacolor@@@");
    err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_BAD_VALUE);
}

TEST(ConfigLuaValueTypes, vec2ParseValidateAndPush) {
    CLuaState      S;
    const auto     L = S.get();

    CLuaConfigVec2 value({0, 0}, std::function<std::expected<void, std::string>(const VEC2&)>{[](const VEC2& v) -> std::expected<void, std::string> {
                             if (v.x < 0 || v.y < 0)
                                 return std::unexpected("must be non-negative");
                             return {};
                         }});

    pushVec2Table(L, 10, 20);
    auto err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OK);
    EXPECT_FLOAT_EQ(value.parsed().x, 10.F);
    EXPECT_FLOAT_EQ(value.parsed().y, 20.F);

    value.push(L);
    ASSERT_TRUE(lua_istable(L, -1));
    lua_rawgeti(L, -1, 1);
    EXPECT_DOUBLE_EQ(lua_tonumber(L, -1), 10);
    lua_pop(L, 1);
    lua_getfield(L, -1, "y");
    EXPECT_DOUBLE_EQ(lua_tonumber(L, -1), 20);
    lua_pop(L, 2);

    pushVec2Table(L, -1, 0);
    err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_BAD_VALUE);
    EXPECT_NE(err.message.find("must be non-negative"), std::string::npos);

    lua_createtable(L, 3, 0);
    lua_pushnumber(L, 1);
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, 2);
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, 3);
    lua_rawseti(L, -2, 3);
    err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_BAD_VALUE);
}

TEST(ConfigLuaValueTypes, expressionVec2AcceptsStringNumberAndTableForms) {
    CLuaState                S;
    const auto               L = S.get();

    CLuaConfigExpressionVec2 value;

    lua_pushstring(L, "monitor_w*0.5 monitor_h*0.25");
    auto err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(value.parsed().x, "monitor_w*0.5");
    EXPECT_EQ(value.parsed().y, "monitor_h*0.25");

    pushVec2Table(L, 150, 200);
    err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(value.parsed().x, "150");
    EXPECT_EQ(value.parsed().y, "200");

    pushExpressionVec2Table(L, "monitor_h * 0.5", "monitor_h * 0.25");
    err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(value.parsed().x, "monitor_h * 0.5");
    EXPECT_EQ(value.parsed().y, "monitor_h * 0.25");

    value.push(L);
    ASSERT_TRUE(lua_istable(L, -1));
    lua_rawgeti(L, -1, 1);
    EXPECT_STREQ(lua_tostring(L, -1), "monitor_h * 0.5");
    lua_pop(L, 1);
    lua_getfield(L, -1, "y");
    EXPECT_STREQ(lua_tostring(L, -1), "monitor_h * 0.25");
    lua_pop(L, 2);

    lua_createtable(L, 1, 0);
    lua_pushnumber(L, 1);
    lua_rawseti(L, -2, 1);
    err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_BAD_VALUE);
}

TEST(ConfigLuaValueTypes, cssGapParseFormsAndRange) {
    CLuaState        S;
    const auto       L = S.get();

    CLuaConfigCssGap value(1, 0, 10);

    lua_pushinteger(L, 7);
    auto err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(value.parsed().m_top, 7);
    EXPECT_EQ(value.parsed().m_right, 7);
    EXPECT_EQ(value.parsed().m_bottom, 7);
    EXPECT_EQ(value.parsed().m_left, 7);

    lua_createtable(L, 0, 2);
    lua_pushinteger(L, 3);
    lua_setfield(L, -2, "top");
    lua_pushinteger(L, 4);
    lua_setfield(L, -2, "left");
    err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(value.parsed().m_top, 3);
    EXPECT_EQ(value.parsed().m_right, 0);
    EXPECT_EQ(value.parsed().m_bottom, 0);
    EXPECT_EQ(value.parsed().m_left, 4);

    lua_createtable(L, 0, 1);
    lua_pushinteger(L, 100);
    lua_setfield(L, -2, "top");
    err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OUT_OF_RANGE);

    lua_createtable(L, 0, 1);
    lua_pushstring(L, "bad");
    lua_setfield(L, -2, "top");
    err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_BAD_TYPE);
}

TEST(ConfigLuaValueTypes, cssGapPreservesFractionsAndRoundTrips) {
    CLuaState        S;
    const auto       L = S.get();
    CLuaConfigCssGap value(1.25);

    for (const double gap : {2.5, -0.8, -1.25}) {
        lua_pushnumber(L, gap);
        ASSERT_EQ(value.parse(L).errorCode, PARSE_ERROR_OK);
        lua_pop(L, 1);
        EXPECT_DOUBLE_EQ(value.parsed().m_top, gap);
        EXPECT_DOUBLE_EQ(value.parsed().m_right, gap);
        EXPECT_DOUBLE_EQ(value.parsed().m_bottom, gap);
        EXPECT_DOUBLE_EQ(value.parsed().m_left, gap);
    }

    lua_createtable(L, 0, 2);
    lua_pushnumber(L, 2.5);
    lua_setfield(L, -2, "top");
    lua_pushnumber(L, -0.8);
    lua_setfield(L, -2, "left");
    ASSERT_EQ(value.parse(L).errorCode, PARSE_ERROR_OK);
    lua_pop(L, 1);

    value.push(L);
    CLuaConfigCssGap roundTrip(0);
    ASSERT_EQ(roundTrip.parse(L).errorCode, PARSE_ERROR_OK);
    lua_pop(L, 1);
    EXPECT_DOUBLE_EQ(roundTrip.parsed().m_top, 2.5);
    EXPECT_DOUBLE_EQ(roundTrip.parsed().m_right, 0);
    EXPECT_DOUBLE_EQ(roundTrip.parsed().m_bottom, 0);
    EXPECT_DOUBLE_EQ(roundTrip.parsed().m_left, -0.8);
    EXPECT_EQ(roundTrip.toString(), "2.5 0 0 -0.8");

    value.reset();
    EXPECT_EQ(value.toString(), "1.25 1.25 1.25 1.25");
}

TEST(ConfigLuaValueTypes, cssGapRejectsNonfiniteValuesWithoutChangingStoredData) {
    CLuaState        S;
    const auto       L = S.get();
    CLuaConfigCssGap value(2.5);

    for (const double invalid : {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        lua_pushnumber(L, invalid);
        EXPECT_EQ(value.parse(L).errorCode, PARSE_ERROR_OUT_OF_RANGE);
        lua_pop(L, 1);

        lua_createtable(L, 0, 2);
        lua_pushnumber(L, 1.25);
        lua_setfield(L, -2, "top");
        lua_pushnumber(L, invalid);
        lua_setfield(L, -2, "left");
        EXPECT_EQ(value.parse(L).errorCode, PARSE_ERROR_OUT_OF_RANGE);
        lua_pop(L, 1);

        EXPECT_EQ(value.toString(), "2.5 2.5 2.5 2.5");
        EXPECT_EQ(lua_gettop(L), 0);
    }
}

TEST(ConfigLuaValueTypes, cssGapTextShorthandPreservesFractions) {
    CCssGapData value;
    value.parseGapData(Hyprutils::String::CVarList2("2.5 -0.8", 0, ' '));
    EXPECT_EQ(value.toString(), "2.5 -0.8 2.5 -0.8");
    value.parseGapData(Hyprutils::String::CVarList2("1.25 2.5 3.75 -1", 0, ' '));
    EXPECT_EQ(value.toString(), "1.25 2.5 3.75 -1");
    EXPECT_THROW(value.parseGapData(Hyprutils::String::CVarList2("nan", 0, ' ')), std::invalid_argument);
    EXPECT_THROW(value.parseGapData(Hyprutils::String::CVarList2("inf", 0, ' ')), std::invalid_argument);
}

TEST(ConfigLuaValueTypes, fontWeightParse) {
    CLuaState            S;
    const auto           L = S.get();

    CLuaConfigFontWeight value(400);

    lua_pushstring(L, "bold");
    auto err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(value.parsed().m_value, 700);

    lua_pushinteger(L, -1);
    err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OUT_OF_RANGE);

    lua_pushstring(L, "definitely-not-a-weight");
    err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_BAD_VALUE);
}

TEST(ConfigLuaValueTypes, gradientParseAndPush) {
    CLuaState          S;
    const auto         L = S.get();

    CLuaConfigGradient value(CHyprColor(0xFF000000));

    lua_pushstring(L, "0xFF112233");
    auto err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OK);
    ASSERT_EQ(value.parsed().m_colors.size(), 1);

    lua_createtable(L, 0, 2);
    lua_createtable(L, 2, 0);
    lua_pushstring(L, "0xFF0000FF");
    lua_rawseti(L, -2, 1);
    lua_pushstring(L, "0xFFFF0000");
    lua_rawseti(L, -2, 2);
    lua_setfield(L, -2, "colors");
    lua_pushnumber(L, 90);
    lua_setfield(L, -2, "angle");

    err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_OK);
    ASSERT_EQ(value.parsed().m_colors.size(), 2);
    EXPECT_NEAR(value.parsed().m_angle, std::numbers::pi_v<float> / 2.F, 0.0001F);

    value.push(L);
    ASSERT_TRUE(lua_istable(L, -1));
    lua_getfield(L, -1, "colors");
    ASSERT_TRUE(lua_istable(L, -1));
    lua_rawgeti(L, -1, 1);
    EXPECT_STREQ(lua_tostring(L, -1), "0xFF0000FF");
    lua_pop(L, 1);
    lua_pop(L, 1);
    lua_getfield(L, -1, "angle");
    EXPECT_NEAR(lua_tonumber(L, -1), 90.0, 0.001);
    lua_pop(L, 2);

    lua_createtable(L, 0, 0);
    err = value.parse(L);
    lua_pop(L, 1);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_BAD_VALUE);
}

TEST(ConfigLuaValueTypes, gradientWholeMonitorRoundTrip) {
    CLuaState  state;
    const auto lua = state.get();

    for (const bool wholeMonitor : {false, true}) {
        SCOPED_TRACE(wholeMonitor);
        CLuaConfigGradient value(CHyprColor(0xFF000000), true);
        ASSERT_EQ(luaL_dostring(lua, "return { colors = { '0xFF112233', '0xFF445566' }, angle = 90 }"), LUA_OK);
        lua_pushboolean(lua, wholeMonitor);
        lua_setfield(lua, -2, "whole_monitor");
        ASSERT_EQ(value.parse(lua).errorCode, PARSE_ERROR_OK);
        lua_pop(lua, 1);

        EXPECT_EQ(value.parsed().m_wholeMonitor, wholeMonitor);
        EXPECT_EQ(value.toString(), wholeMonitor ? "0xFF112233 0xFF445566 whole_monitor 90deg" : "0xFF112233 0xFF445566 90deg");
        auto data = value.parsed();
        EXPECT_EQ(data.toString(), wholeMonitor ? "ff112233 ff445566 whole_monitor 90deg" : "ff112233 ff445566 90deg");

        value.push(lua);
        lua_getfield(lua, -1, "whole_monitor");
        ASSERT_TRUE(lua_isboolean(lua, -1));
        EXPECT_EQ(lua_toboolean(lua, -1), wholeMonitor);
        lua_pop(lua, 1);

        CLuaConfigGradient roundTripped(CHyprColor(0xFFFFFFFF), true);
        ASSERT_EQ(roundTripped.parse(lua).errorCode, PARSE_ERROR_OK);
        lua_pop(lua, 1);
        EXPECT_EQ(roundTripped.parsed(), value.parsed());
        EXPECT_EQ(roundTripped.parsed().m_colorsOkLabA, value.parsed().m_colorsOkLabA);
        EXPECT_EQ(lua_gettop(lua), 0);
    }
}

TEST(ConfigLuaValueTypes, gradientWholeMonitorRejectsInvalidTypesWithoutMutation) {
    CLuaState  state;
    const auto lua = state.get();

    for (const bool supported : {false, true}) {
        CLuaConfigGradient value(CHyprColor(0xFF000000), supported);
        ASSERT_EQ(luaL_dostring(lua, "return { colors = { '0xFF112233', '0xFF445566' }, angle = 90 }"), LUA_OK);
        lua_pushboolean(lua, supported);
        lua_setfield(lua, -2, "whole_monitor");
        ASSERT_EQ(value.parse(lua).errorCode, PARSE_ERROR_OK);
        lua_pop(lua, 1);
        const auto original = value.parsed();
        value.resetSetByUser();

        for (const auto* invalid : {"0", "1", "'true'", "'false'", "{}", "function() end"}) {
            SCOPED_TRACE(std::format("supported={}, whole_monitor={}", supported, invalid));
            const auto code = std::format("return {{ colors = {{ '0xFF778899' }}, angle = 180, whole_monitor = {} }}", invalid);
            ASSERT_EQ(luaL_dostring(lua, code.c_str()), LUA_OK);
            const auto err = value.parse(lua);
            EXPECT_EQ(lua_gettop(lua), 1);
            lua_pop(lua, 1);

            EXPECT_EQ(err.errorCode, PARSE_ERROR_BAD_TYPE);
            EXPECT_NE(err.message.find("whole_monitor"), std::string::npos);
            EXPECT_NE(err.message.find("boolean"), std::string::npos);
            EXPECT_EQ(value.parsed(), original);
            EXPECT_EQ(value.parsed().m_colorsOkLabA, original.m_colorsOkLabA);
            EXPECT_FALSE(value.setByUser());
        }
    }
}

TEST(ConfigLuaValueTypes, gradientWholeMonitorUnsupportedRejectsTrueAndAcceptsFalse) {
    CLuaState          state;
    const auto         lua = state.get();
    CLuaConfigGradient value(CHyprColor(0xFF112233));
    const auto         original = value.parsed();

    ASSERT_EQ(luaL_dostring(lua, "return { colors = { '0xFF445566' }, angle = 90, whole_monitor = true }"), LUA_OK);
    const auto err = value.parse(lua);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_BAD_VALUE);
    EXPECT_NE(err.message.find("whole_monitor"), std::string::npos);
    EXPECT_NE(err.message.find("not supported"), std::string::npos);
    EXPECT_EQ(value.parsed(), original);
    EXPECT_EQ(value.parsed().m_colorsOkLabA, original.m_colorsOkLabA);
    EXPECT_FALSE(value.setByUser());
    ASSERT_EQ(lua_gettop(lua), 1);

    lua_pushboolean(lua, false);
    lua_setfield(lua, -2, "whole_monitor");
    ASSERT_EQ(value.parse(lua).errorCode, PARSE_ERROR_OK);
    lua_pop(lua, 1);
    EXPECT_EQ(value.toString(), "0xFF445566 90deg");
    EXPECT_FALSE(value.parsed().m_wholeMonitor);
    EXPECT_TRUE(value.setByUser());

    value.push(lua);
    CLuaConfigGradient roundTripped(CHyprColor(0xFF000000));
    EXPECT_EQ(roundTripped.parse(lua).errorCode, PARSE_ERROR_OK);
    lua_pop(lua, 1);
    EXPECT_EQ(roundTripped.parsed(), value.parsed());
}

TEST(ConfigLuaValueTypes, gradientWholeMonitorReplacementAndReset) {
    CLuaState          state;
    const auto         lua = state.get();
    CLuaConfigGradient value(CHyprColor(0xFF778899), true);

    for (const auto* replacement : {"return { colors = { '0xFF112233' } }", "return { colors = { '0xFF112233' }, whole_monitor = false }", "return '0xFF112233'"}) {
        SCOPED_TRACE(replacement);
        ASSERT_EQ(luaL_dostring(lua, "return { colors = { '0xFF445566', '0xFF778899' }, angle = 90, whole_monitor = true }"), LUA_OK);
        ASSERT_EQ(value.parse(lua).errorCode, PARSE_ERROR_OK);
        lua_pop(lua, 1);
        const auto original = value.parsed();

        for (const auto* invalid : {"return { colors = { 'invalid' }, whole_monitor = false }", "return { colors = { '0xFF112233' }, angle = false }"}) {
            ASSERT_EQ(luaL_dostring(lua, invalid), LUA_OK);
            EXPECT_NE(value.parse(lua).errorCode, PARSE_ERROR_OK);
            lua_pop(lua, 1);
            EXPECT_EQ(value.parsed(), original);
            EXPECT_EQ(value.parsed().m_colorsOkLabA, original.m_colorsOkLabA);
        }

        ASSERT_EQ(luaL_dostring(lua, replacement), LUA_OK);
        ASSERT_EQ(value.parse(lua).errorCode, PARSE_ERROR_OK);
        lua_pop(lua, 1);
        EXPECT_EQ(value.parsed(), CGradientValueData(CHyprColor(0xFF112233)));
        EXPECT_FALSE(value.parsed().m_wholeMonitor);
        EXPECT_EQ(value.toString(), "0xFF112233 0deg");
    }

    ASSERT_EQ(luaL_dostring(lua, "return { colors = { '0xFF112233', '0xFF445566' }, angle = 90, whole_monitor = true }"), LUA_OK);
    ASSERT_EQ(value.parse(lua).errorCode, PARSE_ERROR_OK);
    lua_pop(lua, 1);
    value.reset();
    EXPECT_EQ(value.parsed(), CGradientValueData(CHyprColor(0xFF778899)));
    EXPECT_FALSE(value.parsed().m_wholeMonitor);
    EXPECT_EQ(value.toString(), "0xFF778899 0deg");
    EXPECT_EQ(lua_gettop(lua), 0);
}
