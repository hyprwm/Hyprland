#include <config/lua/LuaRef.hpp>

#include <gtest/gtest.h>

#include <string_view>

extern "C" {
#include <lualib.h>
#include <lauxlib.h>
}

using namespace Config::Lua;

static bool registrySlotIsString(lua_State* L, int id, const char* expected) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, id);
    const bool MATCHES = lua_isstring(L, -1) && std::string_view{lua_tostring(L, -1)} == expected;
    lua_pop(L, 1);
    return MATCHES;
}

TEST(ConfigLuaRef, pushReturnsReferencedValue) {
    lua_State* L        = luaL_newstate();
    const auto LIFETIME = makeShared<SLuaStateLifetime>(L);

    lua_pushstring(L, "hello");
    CLuaRef ref{LIFETIME, L, -1};
    lua_pop(L, 1);

    EXPECT_EQ(ref.state(), L);
    ASSERT_TRUE(ref.push());
    EXPECT_STREQ(lua_tostring(L, -1), "hello");
    lua_pop(L, 1);

    LIFETIME->state = nullptr;
    lua_close(L);
}

TEST(ConfigLuaRef, destructionReleasesRegistrySlot) {
    lua_State* L        = luaL_newstate();
    const auto LIFETIME = makeShared<SLuaStateLifetime>(L);

    int        id = LUA_NOREF;
    {
        lua_pushstring(L, "owned");
        CLuaRef ref{LIFETIME, L, -1};
        lua_pop(L, 1);

        id = ref.id();
        EXPECT_TRUE(registrySlotIsString(L, id, "owned"));
    }

    EXPECT_FALSE(registrySlotIsString(L, id, "owned"));

    LIFETIME->state = nullptr;
    lua_close(L);
}

TEST(ConfigLuaRef, moveTransfersOwnership) {
    lua_State* L        = luaL_newstate();
    const auto LIFETIME = makeShared<SLuaStateLifetime>(L);

    lua_pushstring(L, "moved");
    CLuaRef a{LIFETIME, L, -1};
    lua_pop(L, 1);
    const int ID = a.id();

    CLuaRef   b = std::move(a);
    EXPECT_EQ(a.state(), nullptr); // NOLINT(bugprone-use-after-move)
    EXPECT_FALSE(a.push());        // NOLINT(bugprone-use-after-move)
    EXPECT_EQ(b.id(), ID);
    EXPECT_TRUE(registrySlotIsString(L, ID, "moved"));

    b = {};
    EXPECT_FALSE(registrySlotIsString(L, ID, "moved"));

    LIFETIME->state = nullptr;
    lua_close(L);
}

TEST(ConfigLuaRef, inertAfterStateClosed) {
    lua_State* L        = luaL_newstate();
    const auto LIFETIME = makeShared<SLuaStateLifetime>(L);

    lua_pushstring(L, "stale");
    auto ref = makeUnique<CLuaRef>(LIFETIME, L, -1);
    lua_pop(L, 1);

    LIFETIME->state = nullptr;
    lua_close(L);

    EXPECT_EQ(ref->state(), nullptr);
    EXPECT_FALSE(ref->push());

    ref.reset();
}

TEST(ConfigLuaRef, emptyRefIsInert) {
    CLuaRef ref;
    EXPECT_EQ(ref.state(), nullptr);
    EXPECT_FALSE(ref.push());
}

TEST(ConfigLuaRef, coroutineThreadSharesRegistry) {
    lua_State* L        = luaL_newstate();
    const auto LIFETIME = makeShared<SLuaStateLifetime>(L);
    lua_State* thread   = lua_newthread(L);

    lua_pushstring(thread, "from thread");
    CLuaRef ref{LIFETIME, thread, -1};
    lua_pop(thread, 1);

    EXPECT_EQ(ref.state(), L);
    EXPECT_TRUE(registrySlotIsString(L, ref.id(), "from thread"));

    ref = {};
    lua_pop(L, 1);
    LIFETIME->state = nullptr;
    lua_close(L);
}

TEST(ConfigLuaRef, staleRefNeverReachesReplacementState) {
    // what a config reload does
    lua_State* oldState = luaL_newstate();
    auto       lifetime = makeShared<SLuaStateLifetime>(oldState);

    lua_pushstring(oldState, "old");
    CLuaRef stale{lifetime, oldState, -1};
    lua_pop(oldState, 1);
    const int STALE_ID = stale.id();

    lifetime->state = nullptr;
    lua_close(oldState);

    lua_State* newState = luaL_newstate();
    lifetime            = makeShared<SLuaStateLifetime>(newState);

    lua_pushstring(newState, "new");
    CLuaRef fresh{lifetime, newState, -1};
    lua_pop(newState, 1);
    EXPECT_EQ(fresh.id(), STALE_ID);

    EXPECT_FALSE(stale.push());
    stale = {};
    EXPECT_TRUE(registrySlotIsString(newState, STALE_ID, "new"));

    fresh           = {};
    lifetime->state = nullptr;
    lua_close(newState);
}
