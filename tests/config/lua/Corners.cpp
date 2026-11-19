#include <config/lua/types/LuaConfigCorners.hpp>
#include <config/lua/types/LuaConfigCssGap.hpp>
#include <config/lua/types/LuaConfigInt.hpp>

#include <gtest/gtest.h>

extern "C" {
#include <lauxlib.h>
}

using namespace Config;
using namespace Config::Lua;

class CConfigLuaCorners : public testing::Test {
  protected:
    void SetUp() override {
        m_lua = luaL_newstate();
    }

    void TearDown() override {
        lua_close(m_lua);
    }

    lua_State* m_lua = nullptr;
};

TEST_F(CConfigLuaCorners, scalarAcceptanceMatchesOldIntegerOption) {
    for (const auto expression : {"0", "100", "true", "false", "-1", "101", "1.0", "1.5", "'4'", "nil"}) {
        SCOPED_TRACE(expression);
        CLuaConfigCorners corners(0, 0, 100);
        CLuaConfigInt     integer(0, 0, 100);
        ASSERT_EQ(luaL_dostring(m_lua, std::format("return {}", expression).c_str()), LUA_OK);
        const auto expected = integer.parse(m_lua);
        EXPECT_EQ(corners.parse(m_lua).errorCode, expected.errorCode);
        EXPECT_EQ(lua_gettop(m_lua), 1);
        if (expected.errorCode == PARSE_ERROR_OK) {
            EXPECT_TRUE(corners.parsed().isUniform());
            EXPECT_EQ(corners.parsed().m_topLeft, integer.parsed());
            corners.push(m_lua);
            EXPECT_TRUE(lua_isinteger(m_lua, -1));
            EXPECT_EQ(lua_tointeger(m_lua, -1), integer.parsed());
            lua_pop(m_lua, 1);
        }
        lua_pop(m_lua, 1);
    }
}

TEST_F(CConfigLuaCorners, namedFieldsMapClockwiseAndRoundTrip) {
    CLuaConfigCorners corners(0, 0, 100);
    ASSERT_EQ(luaL_dostring(m_lua, "return {top_left = 1, top_right = 2, bottom_right = 3, bottom_left = 4}"), LUA_OK);
    ASSERT_EQ(corners.parse(m_lua).errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(lua_gettop(m_lua), 1);
    lua_pop(m_lua, 1);
    EXPECT_EQ(corners.toString(), "1 2 3 4");
    corners.push(m_lua);
    ASSERT_TRUE(lua_istable(m_lua, -1));
    int64_t expected = 1;
    for (const auto field : {"top_left", "top_right", "bottom_right", "bottom_left"}) {
        lua_getfield(m_lua, -1, field);
        EXPECT_TRUE(lua_isinteger(m_lua, -1));
        EXPECT_EQ(lua_tointeger(m_lua, -1), expected++);
        lua_pop(m_lua, 1);
    }
    for (const auto field : {"top", "right", "bottom", "left"}) {
        lua_getfield(m_lua, -1, field);
        EXPECT_TRUE(lua_isnil(m_lua, -1));
        lua_pop(m_lua, 1);
    }
    CLuaConfigCorners copy(0);
    EXPECT_EQ(copy.parse(m_lua).errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(lua_gettop(m_lua), 1);
    EXPECT_EQ(copy.toString(), "1 2 3 4");
}

TEST_F(CConfigLuaCorners, namedFieldsAcceptIntegerConvertibleValues) {
    CLuaConfigCorners corners(0, 0, 100);
    ASSERT_EQ(luaL_dostring(m_lua, "return {top_left = '1', top_right = 2.0, bottom_right = '3.0', bottom_left = 4}"), LUA_OK);
    ASSERT_EQ(corners.parse(m_lua).errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(lua_gettop(m_lua), 1);
    EXPECT_EQ(corners.toString(), "1 2 3 4");
}

TEST_F(CConfigLuaCorners, omittedFieldsResetToZeroAndEqualFieldsPushScalar) {
    CLuaConfigCorners corners(9, 0, 100);
    ASSERT_EQ(luaL_dostring(m_lua, "return {top_right = 100}"), LUA_OK);
    ASSERT_EQ(corners.parse(m_lua).errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(corners.toString(), "0 100 0 0");
    lua_pop(m_lua, 1);

    ASSERT_EQ(luaL_dostring(m_lua, "return {}"), LUA_OK);
    ASSERT_EQ(corners.parse(m_lua).errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(corners.toString(), "0");
    lua_pop(m_lua, 1);

    ASSERT_EQ(luaL_dostring(m_lua, "return {top_left = 4, top_right = 4, bottom_right = 4, bottom_left = 4}"), LUA_OK);
    ASSERT_EQ(corners.parse(m_lua).errorCode, PARSE_ERROR_OK);
    lua_pop(m_lua, 1);
    corners.push(m_lua);
    EXPECT_TRUE(lua_isinteger(m_lua, -1));
    EXPECT_EQ(lua_tointeger(m_lua, -1), 4);
    lua_pop(m_lua, 1);

    corners.reset();
    corners.push(m_lua);
    EXPECT_TRUE(lua_isinteger(m_lua, -1));
    EXPECT_EQ(lua_tointeger(m_lua, -1), 9);
}

TEST_F(CConfigLuaCorners, invalidValuesAndKeysPreserveDataSetFlagAndStack) {
    lua_pushliteral(m_lua, "sentinel");
    for (const bool setByUser : {false, true}) {
        CLuaConfigCorners corners(7, 0, 100);
        if (setByUser) {
            ASSERT_EQ(luaL_dostring(m_lua, "return {top_left = 1, top_right = 2, bottom_right = 3, bottom_left = 4}"), LUA_OK);
            ASSERT_EQ(corners.parse(m_lua).errorCode, PARSE_ERROR_OK);
            lua_pop(m_lua, 1);
        }

        for (const auto expression : {
                 "-1",
                 "101",
                 "'bad'",
                 "{top_left = 1, top_right = 2, bottom_right = 3, bottom_left = -1}",
                 "{top_left = 1, top_right = 101}",
                 "{top_left = 1, bottom_right = 'bad'}",
                 "{bottom_left = 0.5}",
                 "{bottom_left = -0.5}",
                 "{bottom_left = 1e100}",
                 "{bottom_left = true}",
                 "{1, 2, 3, 4}",
                 "{[5] = 4, top_left = 1}",
                 "{['1'] = 4}",
                 "{[true] = 4}",
                 "{top = 1}",
                 "{right = 2}",
                 "{bottom = 3}",
                 "{left = 4}",
                 "{unknown = 5}",
                 "{top_left = 8, top_right = 9, bottom_right = 10, bottom_left = 11, unknown = 5}",
             }) {
            SCOPED_TRACE(expression);
            SCOPED_TRACE(setByUser);
            ASSERT_EQ(luaL_dostring(m_lua, std::format("return {}", expression).c_str()), LUA_OK);
            EXPECT_NE(corners.parse(m_lua).errorCode, PARSE_ERROR_OK);
            ASSERT_EQ(lua_gettop(m_lua), 2);
            EXPECT_STREQ(lua_tostring(m_lua, -2), "sentinel");
            EXPECT_EQ(corners.toString(), setByUser ? "1 2 3 4" : "7");
            EXPECT_EQ(corners.setByUser(), setByUser);
            lua_pop(m_lua, 1);
        }
    }
}

TEST_F(CConfigLuaCorners, unboundedCornersPreserveSignedAndLargeIntegers) {
    CLuaConfigCorners corners(0);
    ASSERT_EQ(luaL_dostring(m_lua, "return {top_left = -20, top_right = 200, bottom_right = 9223372036854775807, bottom_left = -9223372036854775807 - 1}"), LUA_OK);
    ASSERT_EQ(corners.parse(m_lua).errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(lua_gettop(m_lua), 1);
    EXPECT_EQ(corners.toString(), "-20 200 9223372036854775807 -9223372036854775808");
    lua_pop(m_lua, 1);

    corners.push(m_lua);
    CLuaConfigCorners copy(0);
    ASSERT_EQ(copy.parse(m_lua).errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(copy.toString(), corners.toString());
}

TEST_F(CConfigLuaCorners, genericGapsKeepNegativeValuesAndTableOutput) {
    CLuaConfigCssGap gaps(0);
    ASSERT_EQ(luaL_dostring(m_lua, "return {top = -1, right = -2, bottom = -3, left = -4}"), LUA_OK);
    ASSERT_EQ(gaps.parse(m_lua).errorCode, PARSE_ERROR_OK);
    EXPECT_EQ(gaps.toString(), "-1 -2 -3 -4");
    gaps.reset();
    gaps.push(m_lua);
    EXPECT_TRUE(lua_istable(m_lua, -1));
}
