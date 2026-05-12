#include "LuaTimer.hpp"

#include <algorithm>
#include <string_view>

using namespace Config::Lua;

static constexpr const char* MT = "HL.Timer";

namespace {
    struct STimerRef {
        WP<CEventLoopTimer> timer;
        int                 timeoutMs = 0;
    };
}

static int timerEq(lua_State* L) {
    const auto* lhs = sc<STimerRef*>(luaL_checkudata(L, 1, MT));
    const auto* rhs = sc<STimerRef*>(luaL_checkudata(L, 2, MT));

    lua_pushboolean(L, lhs->timer.lock() == rhs->timer.lock());
    return 1;
}

static int timerToString(lua_State* L) {
    const auto* ref   = sc<STimerRef*>(luaL_checkudata(L, 1, MT));
    const auto  timer = ref->timer.lock();

    if (!timer)
        lua_pushstring(L, "HL.Timer(expired)");
    else
        lua_pushfstring(L, "HL.Timer(%p)", timer.get());

    return 1;
}

static int timerSetEnabled(lua_State* L) {
    auto* ref = sc<STimerRef*>(luaL_checkudata(L, 1, MT));
    luaL_checktype(L, 2, LUA_TBOOLEAN);

    const auto timer = ref->timer.lock();
    if (!timer)
        return 0;

    if (lua_toboolean(L, 2))
        timer->updateTimeout(std::chrono::milliseconds(std::max(ref->timeoutMs, 1)));
    else
        timer->updateTimeout(std::nullopt);

    return 0;
}

static int timerSetTimeout(lua_State* L) {
    auto* ref = sc<STimerRef*>(luaL_checkudata(L, 1, MT));
    luaL_checktype(L, 2, LUA_TNUMBER);

    const auto timer = ref->timer.lock();
    if (!timer)
        return 0;

    uint64_t x = lua_tointeger(L, 2);

    if (x < 1)
        return Config::Lua::Bindings::Internal::configError(L, "hl.timer:set_timeout: timeout must be greater or equal to 1ms");

    timer->updateTimeout(std::chrono::milliseconds(x));
    ref->timeoutMs = x;

    return 0;
}

static int timerIsEnabled(lua_State* L) {
    auto*      ref = sc<STimerRef*>(luaL_checkudata(L, 1, MT));

    const auto timer = ref->timer.lock();
    if (!timer) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushboolean(L, timer->armed());
    return 1;
}

static int timerIndex(lua_State* L) {
    luaL_checkudata(L, 1, MT);
    const std::string_view key = luaL_checkstring(L, 2);

    if (key == "set_enabled")
        lua_pushcfunction(L, timerSetEnabled);
    else if (key == "is_enabled")
        lua_pushcfunction(L, timerIsEnabled);
    else if (key == "set_timeout")
        lua_pushcfunction(L, timerSetTimeout);
    else
        lua_pushnil(L);

    return 1;
}

void Objects::CLuaTimer::setup(lua_State* L) {
    registerMetatable(L, MT, timerIndex, gcRef<STimerRef>, timerEq, timerToString);
}

void Objects::CLuaTimer::push(lua_State* L, const SP<CEventLoopTimer>& timer, int timeoutMs) {
    new (lua_newuserdata(L, sizeof(STimerRef))) STimerRef{WP<CEventLoopTimer>(timer), timeoutMs};
    luaL_getmetatable(L, MT);
    lua_setmetatable(L, -2);
}
