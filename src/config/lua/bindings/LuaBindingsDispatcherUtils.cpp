#include "LuaBindingsInternal.hpp"

using namespace Config::Lua::Bindings;

static constexpr const char* DISPATCHER_MT = "HL.Dispatcher";
static char                  DISPATCHER_TABLES_REGISTRY_KEY;

namespace {
    struct SDispatcherRef {
        int ref = LUA_NOREF;
    };
}

static int dispatcherGc(lua_State* L) {
    auto* dispatcher = sc<SDispatcherRef*>(luaL_checkudata(L, 1, DISPATCHER_MT));
    if (dispatcher->ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, dispatcher->ref);
        dispatcher->ref = LUA_NOREF;
    }

    return 0;
}

static int dispatcherCall(lua_State* L) {
    return Internal::configError(L, "dispatcher objects cannot be called directly; use hl.dispatch(dispatcher)");
}

static int dispatcherToString(lua_State* L) {
    lua_pushstring(L, "HL.Dispatcher");
    return 1;
}

static void ensureDispatcherMetatable(lua_State* L) {
    if (luaL_newmetatable(L, DISPATCHER_MT)) {
        lua_pushcfunction(L, dispatcherGc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, dispatcherCall);
        lua_setfield(L, -2, "__call");
        lua_pushcfunction(L, dispatcherToString);
        lua_setfield(L, -2, "__tostring");

        lua_pushstring(L, DISPATCHER_MT);
        lua_setfield(L, -2, "__metatable");
    }

    lua_pop(L, 1);
}

static bool isDispatcherTable(lua_State* L, int idx) {
    if (!lua_istable(L, idx))
        return false;

    idx = lua_absindex(L, idx);
    lua_pushlightuserdata(L, &DISPATCHER_TABLES_REGISTRY_KEY);
    lua_rawget(L, LUA_REGISTRYINDEX);

    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return false;
    }

    lua_pushvalue(L, idx);
    lua_rawget(L, -2);
    const bool result = lua_toboolean(L, -1);
    lua_pop(L, 2);
    return result;
}

static int dispatcherFactory(lua_State* L) {
    const int nargs = lua_gettop(L);

    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_call(L, nargs, LUA_MULTRET);

    const int nresults = lua_gettop(L);
    if (nresults == 1 && lua_isfunction(L, -1))
        return Internal::wrapDispatcher(L);

    return nresults;
}

void Internal::setFn(lua_State* L, const char* name, lua_CFunction fn) {
    if (isDispatcherTable(L, -1)) {
        lua_pushcfunction(L, fn);
        lua_pushcclosure(L, dispatcherFactory, 1);
    } else
        lua_pushcfunction(L, fn);

    lua_setfield(L, -2, name);
}

void Internal::markDispatcherTable(lua_State* L) {
    if (!lua_istable(L, -1))
        return;

    lua_pushlightuserdata(L, &DISPATCHER_TABLES_REGISTRY_KEY);
    lua_rawget(L, LUA_REGISTRYINDEX);

    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushlightuserdata(L, &DISPATCHER_TABLES_REGISTRY_KEY);
        lua_pushvalue(L, -2);
        lua_rawset(L, LUA_REGISTRYINDEX);
    }

    lua_pushvalue(L, -2);
    lua_pushboolean(L, true);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

int Internal::wrapDispatcher(lua_State* L) {
    luaL_checktype(L, -1, LUA_TFUNCTION);

    const int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    new (lua_newuserdata(L, sizeof(SDispatcherRef))) SDispatcherRef{.ref = ref};

    ensureDispatcherMetatable(L);
    luaL_getmetatable(L, DISPATCHER_MT);
    lua_setmetatable(L, -2);

    return 1;
}

bool Internal::pushDispatcherFunction(lua_State* L, int idx) {
    if (lua_isfunction(L, idx)) {
        lua_pushvalue(L, idx);
        return true;
    }

    auto* dispatcher = sc<SDispatcherRef*>(luaL_testudata(L, idx, DISPATCHER_MT));
    if (!dispatcher || dispatcher->ref == LUA_NOREF)
        return false;

    lua_rawgeti(L, LUA_REGISTRYINDEX, dispatcher->ref);
    if (lua_isfunction(L, -1))
        return true;

    lua_pop(L, 1);
    return false;
}
