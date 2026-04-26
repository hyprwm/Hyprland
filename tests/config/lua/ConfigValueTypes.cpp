#include <config/lua/types/LuaConfigBool.hpp>
#include <config/lua/types/LuaConfigColor.hpp>
#include <config/lua/types/LuaConfigCssGap.hpp>
#include <config/lua/types/LuaConfigFloat.hpp>
#include <config/lua/types/LuaConfigFontWeight.hpp>
#include <config/lua/types/LuaConfigGradient.hpp>
#include <config/lua/types/LuaConfigInt.hpp>
#include <config/lua/types/LuaConfigString.hpp>
#include <config/lua/types/LuaConfigVec2.hpp>

#include <macros.hpp>

#include <gtest/gtest.h>

#include <numbers>

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
}

TEST(ConfigLuaValueTypes, boolParseAndReset) {
    CLuaState      S;
    const auto     L = S.get();

    CLuaConfigBool value(false);
    EXPECT_FALSE(value.parsed());
    EXPECT_FALSE(value.setByUser());

    lua_pushboolean(L, true);
    const auto err = value.parse(L);
    lua_pop(L, 1);

    EXPECT_EQ(err.errorCode, PARSE_ERROR_OK);
    EXPECT_TRUE(value.parsed());
    EXPECT_TRUE(value.setByUser());

    value.reset();
    EXPECT_FALSE(value.parsed());

    value.resetSetByUser();
    EXPECT_FALSE(value.setByUser());
}

TEST(ConfigLuaValueTypes, boolBadType) {
    CLuaState      S;
    const auto     L = S.get();

    CLuaConfigBool value(false);

    lua_pushinteger(L, 1);
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

    CLuaConfigString validated("default", std::optional<std::function<std::expected<void, std::string>(const STRING&)>>{[](const STRING& s) -> std::expected<void, std::string> {
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

    CLuaConfigVec2 value({0, 0}, std::optional<std::function<std::expected<void, std::string>(const VEC2&)>>{[](const VEC2& v) -> std::expected<void, std::string> {
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
