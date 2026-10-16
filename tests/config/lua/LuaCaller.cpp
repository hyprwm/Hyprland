#include <config/lua/LuaCaller.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

extern "C" {
#include <lualib.h>
#include <lauxlib.h>
}

using namespace Config::Lua;

namespace {
    class CConfigLuaCaller : public testing::Test {
      protected:
        void SetUp() override {
            m_lua = luaL_newstate();
            luaL_openlibs(m_lua);
            m_lifetime = makeShared<SLuaStateLifetime>(m_lua);
            m_caller   = makeUnique<CLuaCaller>(m_lua, [this](std::string&& e) { m_errors.emplace_back(std::move(e)); });
        }

        void TearDown() override {
            m_refs.clear();
            m_caller.reset();
            m_lifetime->state = nullptr;
            lua_close(m_lua);
        }

        CLuaRef& ref(const char* code) {
            EXPECT_EQ(luaL_dostring(m_lua, code), LUA_OK) << lua_tostring(m_lua, -1);
            EXPECT_TRUE(lua_isfunction(m_lua, -1));
            m_refs.emplace_back(makeUnique<CLuaRef>(m_lifetime, m_lua, -1));
            lua_pop(m_lua, 1);
            return *m_refs.back();
        }

        int global(const char* name) {
            lua_getglobal(m_lua, name);
            const int VAL = sc<int>(lua_tointeger(m_lua, -1));
            lua_pop(m_lua, 1);
            return VAL;
        }

        lua_State*               m_lua = nullptr;
        SP<SLuaStateLifetime>    m_lifetime;
        UP<CLuaCaller>           m_caller;
        std::vector<UP<CLuaRef>> m_refs;
        std::vector<std::string> m_errors;
    };

    // lets Lua code call back into the caller, like hl.dispatch -> event -> callback does
    struct SReentry {
        CLuaCaller*           caller = nullptr;
        std::vector<CLuaRef*> refs;
        int                   reads = 0;
    };
}

static int reenter(lua_State* L) {
    auto*     reentry = sc<SReentry*>(lua_touserdata(L, lua_upvalueindex(1)));
    const int IDX     = sc<int>(luaL_checkinteger(L, 1));
    if (IDX < 1 || IDX > sc<int>(reentry->refs.size()))
        return 0;

    lua_pushboolean(L, reentry->caller->call(*reentry->refs[IDX - 1], 0, "reentry", {}, [reentry](lua_State*) { ++reentry->reads; }).has_value());
    return 1;
}

static void installReentry(lua_State* L, SReentry* reentry) {
    lua_pushlightuserdata(L, reentry);
    lua_pushcclosure(L, reenter, 1);
    lua_setglobal(L, "reenter");
}

TEST_F(CConfigLuaCaller, passesArgsAndReadsResults) {
    auto& fn = ref("return function(a, b) return a + b end");

    int   sum    = 0;
    auto  result = m_caller->call(
        fn, 0, "sum",
        [](lua_State* L) {
            lua_pushinteger(L, 2);
            lua_pushinteger(L, 3);
            return 2;
        },
        [&sum](lua_State* L) { sum = sc<int>(lua_tointeger(L, -1)); });

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(sum, 5);
    EXPECT_EQ(lua_gettop(m_lua), 0);
    EXPECT_TRUE(m_errors.empty());
}

TEST_F(CConfigLuaCaller, errorIsReportedOncePerMessage) {
    auto& fn       = ref("return function(msg) error(msg, 0) end");
    auto  callWith = [&](const char* msg) {
        return m_caller->call(fn, 0, "ctx", [msg](lua_State* L) {
            lua_pushstring(L, msg);
            return 1;
        });
    };

    const auto FIRST = callWith("a");
    ASSERT_FALSE(FIRST.has_value());
    EXPECT_EQ(FIRST.error(), "ctx: a");
    EXPECT_FALSE(callWith("a").has_value());
    EXPECT_FALSE(callWith("b").has_value());
    EXPECT_FALSE(callWith("a").has_value());

    EXPECT_EQ(m_errors, (std::vector<std::string>{"ctx: a", "ctx: b"}));
    EXPECT_EQ(lua_gettop(m_lua), 0);
}

TEST_F(CConfigLuaCaller, sameMessageFromAnotherFunctionIsReported) {
    auto& a = ref("return function() error('boom', 0) end");
    auto& b = ref("return function() error('boom', 0) end");

    EXPECT_FALSE(m_caller->call(a, 0, "ctx").has_value());
    EXPECT_FALSE(m_caller->call(b, 0, "ctx").has_value());

    EXPECT_EQ(m_errors.size(), 2);
}

TEST_F(CConfigLuaCaller, nonStringErrorObjectsAreDescribed) {
    auto& table   = ref("return function() error({}) end");
    auto& boolean = ref("return function() error(true) end");
    auto& custom  = ref("return function() error(setmetatable({}, { __tostring = function() return 'custom' end })) end");
    auto& hostile = ref("return function() error(setmetatable({}, { __tostring = function() error('nope') end })) end");

    EXPECT_TRUE(m_caller->call(table, 0, "ctx").error().starts_with("ctx: table: "));
    EXPECT_EQ(m_caller->call(boolean, 0, "ctx").error(), "ctx: true");
    EXPECT_EQ(m_caller->call(custom, 0, "ctx").error(), "ctx: custom");
    EXPECT_EQ(m_caller->call(hostile, 0, "ctx").error(), "ctx: (error object is a table value)");
    EXPECT_EQ(lua_gettop(m_lua), 0);
}

TEST_F(CConfigLuaCaller, staleRefIsNotCalled) {
    auto&      fn = ref("return function() called = 1 end");

    lua_State* L      = m_lua;
    m_lifetime->state = nullptr;

    EXPECT_FALSE(m_caller->call(fn, 0, "ctx").has_value());
    EXPECT_TRUE(m_errors.empty());

    m_lifetime->state = L;
    EXPECT_EQ(global("called"), 0);
}

TEST_F(CConfigLuaCaller, selfReentryIsSkipped) {
    SReentry reentry{.caller = m_caller.get()};
    installReentry(m_lua, &reentry);

    reentry.refs.push_back(&ref("return function() count = (count or 0) + 1; nested = reenter(1) end"));

    EXPECT_TRUE(m_caller->call(*reentry.refs[0], 0, "ctx").has_value());
    EXPECT_EQ(global("count"), 1);

    // a skip is not a failure, doesn't read a result, and is logged rather than reported to the user
    lua_getglobal(m_lua, "nested");
    EXPECT_TRUE(lua_toboolean(m_lua, -1));
    lua_pop(m_lua, 1);
    EXPECT_EQ(reentry.reads, 0);
    EXPECT_TRUE(m_errors.empty());
    EXPECT_EQ(lua_gettop(m_lua), 0);
}

TEST_F(CConfigLuaCaller, otherCallbacksMayNest) {
    SReentry reentry{.caller = m_caller.get()};
    installReentry(m_lua, &reentry);

    reentry.refs.push_back(&ref("return function() reenter(2) end"));
    reentry.refs.push_back(&ref("return function() inner = 1 end"));

    EXPECT_TRUE(m_caller->call(*reentry.refs[0], 0, "ctx").has_value());
    EXPECT_EQ(global("inner"), 1);
}

TEST_F(CConfigLuaCaller, nestingDepthIsCapped) {
    SReentry reentry{.caller = m_caller.get()};
    installReentry(m_lua, &reentry);

    ASSERT_EQ(luaL_dostring(m_lua, "function make(n) return function() deepest = n; reenter(n + 1) end end"), LUA_OK);
    for (int i = 1; i <= 40; ++i) {
        lua_getglobal(m_lua, "make");
        lua_pushinteger(m_lua, i);
        ASSERT_EQ(lua_pcall(m_lua, 1, 1, 0), LUA_OK);
        m_refs.emplace_back(makeUnique<CLuaRef>(m_lifetime, m_lua, -1));
        lua_pop(m_lua, 1);
        reentry.refs.push_back(m_refs.back().get());
    }

    EXPECT_TRUE(m_caller->call(*reentry.refs[0], 0, "ctx").has_value());
    EXPECT_EQ(global("deepest"), sc<int>(CLuaCaller::MAX_CALL_DEPTH));
    EXPECT_EQ(lua_gettop(m_lua), 0);
}

TEST_F(CConfigLuaCaller, watchdogStopsRunawayCallback) {
    auto&      fn = ref("return function() while true do end end");

    const auto RESULT = m_caller->call(fn, 10, "spin");
    ASSERT_FALSE(RESULT.has_value());
    EXPECT_NE(RESULT.error().find("execution timed out in spin"), std::string::npos);
    EXPECT_EQ(m_errors.size(), 1);
    EXPECT_EQ(lua_gettop(m_lua), 0);
}

TEST_F(CConfigLuaCaller, guardedPCallRunsOnTheGivenThread) {
    lua_State* thread = lua_newthread(m_lua);
    ASSERT_EQ(luaL_loadstring(thread, "return 42"), LUA_OK);

    ASSERT_EQ(m_caller->guardedPCall(thread, 0, 1, 0, 0, "thread"), LUA_OK);
    EXPECT_EQ(lua_tointeger(thread, -1), 42);

    // only the thread object itself is on the main stack
    EXPECT_EQ(lua_gettop(m_lua), 1);
    lua_pop(m_lua, 1);
}
