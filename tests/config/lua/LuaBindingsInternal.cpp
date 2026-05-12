#include <config/lua/bindings/LuaBindingsInternal.hpp>
#include <config/lua/ConfigManager.hpp>

#include <Compositor.hpp>

#include <config/lua/types/LuaConfigInt.hpp>

#include <gtest/gtest.h>

extern "C" {
#include <lualib.h>
#include <lauxlib.h>
}

using namespace Config::Lua;
using namespace Config::Lua::Bindings;

namespace Config::Lua {
    class CConfigManagerPluginLuaTestAccessor {
      public:
        static void initializeLuaState(CConfigManager& mgr, lua_State* L) {
            mgr.m_lua = L;
            lua_pushlightuserdata(L, &mgr);
            lua_setfield(L, LUA_REGISTRYINDEX, "hl_lua_manager");
        }
    };
}

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

    int testPluginFn(lua_State* L) {
        lua_pushstring(L, "pong");
        return 1;
    }
}

TEST(ConfigLuaBindingsInternal, parseDirectionAliases) {
    EXPECT_EQ(Internal::parseDirectionStr("left"), Math::DIRECTION_LEFT);
    EXPECT_EQ(Internal::parseDirectionStr("l"), Math::DIRECTION_LEFT);
    EXPECT_EQ(Internal::parseDirectionStr("right"), Math::DIRECTION_RIGHT);
    EXPECT_EQ(Internal::parseDirectionStr("r"), Math::DIRECTION_RIGHT);
    EXPECT_EQ(Internal::parseDirectionStr("up"), Math::DIRECTION_UP);
    EXPECT_EQ(Internal::parseDirectionStr("t"), Math::DIRECTION_UP);
    EXPECT_EQ(Internal::parseDirectionStr("down"), Math::DIRECTION_DOWN);
    EXPECT_EQ(Internal::parseDirectionStr("b"), Math::DIRECTION_DOWN);
    EXPECT_EQ(Internal::parseDirectionStr("???"), Math::DIRECTION_DEFAULT);
}

TEST(ConfigLuaBindingsInternal, parseToggleAliases) {
    EXPECT_EQ(Internal::parseToggleStr(""), Config::Actions::TOGGLE_ACTION_TOGGLE);
    EXPECT_EQ(Internal::parseToggleStr("toggle"), Config::Actions::TOGGLE_ACTION_TOGGLE);
    EXPECT_EQ(Internal::parseToggleStr("enable"), Config::Actions::TOGGLE_ACTION_ENABLE);
    EXPECT_EQ(Internal::parseToggleStr("on"), Config::Actions::TOGGLE_ACTION_ENABLE);
    EXPECT_EQ(Internal::parseToggleStr("disable"), Config::Actions::TOGGLE_ACTION_DISABLE);
    EXPECT_EQ(Internal::parseToggleStr("off"), Config::Actions::TOGGLE_ACTION_DISABLE);
}

TEST(ConfigLuaBindingsInternal, argStrConvertsStringsAndNumbers) {
    CLuaState  S;
    const auto L = S.get();

    lua_pushstring(L, "abc");
    EXPECT_EQ(Internal::argStr(L, -1), "abc");
    lua_pop(L, 1);

    lua_pushnumber(L, 42);
    EXPECT_EQ(Internal::argStr(L, -1), "42");
    lua_pop(L, 1);
}

TEST(ConfigLuaBindingsInternal, tableOptHelpersReadOptionalFields) {
    CLuaState  S;
    const auto L = S.get();

    lua_createtable(L, 0, 5);
    lua_pushstring(L, "value");
    lua_setfield(L, -2, "s");
    lua_pushnumber(L, 5.5);
    lua_setfield(L, -2, "n");
    lua_pushboolean(L, true);
    lua_setfield(L, -2, "b");
    lua_pushstring(L, "not-number");
    lua_setfield(L, -2, "n2");
    lua_pushnil(L);
    lua_setfield(L, -2, "nilv");

    EXPECT_EQ(Internal::tableOptStr(L, -1, "s").value_or(""), "value");
    EXPECT_DOUBLE_EQ(Internal::tableOptNum(L, -1, "n").value_or(0), 5.5);
    EXPECT_EQ(Internal::tableOptBool(L, -1, "b").value_or(false), true);
    EXPECT_FALSE(Internal::tableOptNum(L, -1, "n2").has_value());
    EXPECT_FALSE(Internal::tableOptStr(L, -1, "missing").has_value());
    EXPECT_FALSE(Internal::tableOptBool(L, -1, "nilv").has_value());

    lua_pop(L, 1);
}

TEST(ConfigLuaBindingsInternal, selectorHelpersAcceptStringAndNumberSelectors) {
    CLuaState  S;
    const auto L = S.get();

    lua_createtable(L, 0, 4);
    lua_pushstring(L, "DP-1");
    lua_setfield(L, -2, "monitor");
    lua_pushnumber(L, 7);
    lua_setfield(L, -2, "workspace");
    lua_pushnumber(L, 1337);
    lua_setfield(L, -2, "window");

    EXPECT_EQ(Internal::tableOptMonitorSelector(L, -1, "monitor", "test.fn").value_or(""), "DP-1");
    EXPECT_EQ(Internal::tableOptWorkspaceSelector(L, -1, "workspace", "test.fn").value_or(""), "7");
    EXPECT_EQ(Internal::tableOptWindowSelector(L, -1, "window", "test.fn").value_or(""), "1337");

    EXPECT_FALSE(Internal::tableOptMonitorSelector(L, -1, "missing", "test.fn").has_value());
    EXPECT_FALSE(Internal::tableOptWorkspaceSelector(L, -1, "missing", "test.fn").has_value());
    EXPECT_FALSE(Internal::tableOptWindowSelector(L, -1, "missing", "test.fn").has_value());

    EXPECT_EQ(Internal::requireTableFieldMonitorSelector(L, -1, "monitor", "test.fn"), "DP-1");
    EXPECT_EQ(Internal::requireTableFieldWorkspaceSelector(L, -1, "workspace", "test.fn"), "7");
    EXPECT_EQ(Internal::requireTableFieldWindowSelector(L, -1, "window", "test.fn"), "1337");

    lua_pop(L, 1);
}

TEST(ConfigLuaBindingsInternal, pushWindowUpvalAcceptsNumberAndStringSelectors) {
    CLuaState  S;
    const auto L = S.get();

    lua_createtable(L, 0, 1);
    lua_pushnumber(L, 42);
    lua_setfield(L, -2, "window");

    Internal::pushWindowUpval(L, -1);
    ASSERT_TRUE(lua_isstring(L, -1));
    EXPECT_STREQ(lua_tostring(L, -1), "42");
    lua_pop(L, 1);

    lua_pushstring(L, "0xabc");
    lua_setfield(L, -2, "window");

    Internal::pushWindowUpval(L, -1);
    ASSERT_TRUE(lua_isstring(L, -1));
    EXPECT_STREQ(lua_tostring(L, -1), "0xabc");
    lua_pop(L, 1);

    lua_pushnil(L);
    lua_setfield(L, -2, "window");

    Internal::pushWindowUpval(L, -1);
    EXPECT_TRUE(lua_isnil(L, -1));
    lua_pop(L, 1);

    lua_pop(L, 1);
}

TEST(ConfigLuaBindingsInternal, parseTableFieldMissingFieldAndPrefixedErrors) {
    CLuaState     S;
    const auto    L = S.get();

    CLuaConfigInt parser(0);

    lua_newtable(L);
    auto err = Internal::parseTableField(L, -1, "required", parser);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_BAD_VALUE);
    EXPECT_NE(err.message.find("missing required field"), std::string::npos);
    lua_pop(L, 1);

    lua_createtable(L, 0, 1);
    lua_pushstring(L, "bad");
    lua_setfield(L, -2, "count");

    err = Internal::parseTableField(L, -1, "count", parser);
    EXPECT_EQ(err.errorCode, PARSE_ERROR_BAD_TYPE);
    EXPECT_NE(err.message.find("field \"count\":"), std::string::npos);
    lua_pop(L, 1);
}

TEST(ConfigLuaBindingsInternal, pluginBindingIsTableWithLoadFunction) {
    CLuaState  S;
    const auto L = S.get();

    lua_newtable(L);
    Internal::registerConfigRuleBindings(L, nullptr);

    lua_getfield(L, -1, "plugin");
    ASSERT_TRUE(lua_istable(L, -1));

    lua_getfield(L, -1, "load");
    EXPECT_TRUE(lua_isfunction(L, -1));
    lua_pop(L, 1);

    lua_pop(L, 2);
}

TEST(ConfigLuaBindingsInternal, pluginLuaFnIsUnloadedWithoutDanglingCall) {
    CLuaState  S;
    const auto L = S.get();

    auto       PREVCOMPOSITOR = std::move(g_pCompositor);
    g_pCompositor             = makeUnique<CCompositor>(true);

    CConfigManager mgr;
    CConfigManagerPluginLuaTestAccessor::initializeLuaState(mgr, L);

    lua_newtable(L);
    Internal::registerConfigRuleBindings(L, &mgr);
    lua_setglobal(L, "hl");

    const auto HANDLE = reinterpret_cast<void*>(0x1BADB002);

    const auto regResult = mgr.registerPluginLuaFunction(HANDLE, "demo", "ping", testPluginFn);
    ASSERT_TRUE(regResult.has_value()) << regResult.error();

    ASSERT_EQ(luaL_dostring(L, R"(
        local f = hl.plugin.demo.ping
        assert(type(f) == "function")
        captured = f
        local v = f()
        assert(v == "pong")
    )"),
              LUA_OK);

    mgr.onPluginUnload(HANDLE);

    ASSERT_EQ(luaL_dostring(L, R"(
        assert(hl.plugin.demo == nil)
    )"),
              LUA_OK);

    ASSERT_EQ(luaL_dostring(L, R"(
        local ok, err = pcall(captured)
        assert(ok == false)
        assert(type(err) == "string")
        assert(string.find(err, "no longer available", 1, true) ~= nil)
    )"),
              LUA_OK);

    g_pCompositor = std::move(PREVCOMPOSITOR);
}
