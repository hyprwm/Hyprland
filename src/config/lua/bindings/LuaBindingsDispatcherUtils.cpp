#include "LuaBindingsInternal.hpp"

using namespace Config::Lua::Bindings;

static constexpr const char* DISPATCHER_MT = "HL.Dispatcher";

namespace {
    struct SDispatcherRef {
        int ref     = LUA_NOREF;
        int nameref = LUA_NOREF;
    };
}

static int dispatcherGc(lua_State* L) {
    auto* dispatcher = sc<SDispatcherRef*>(luaL_checkudata(L, 1, DISPATCHER_MT));
    if (dispatcher->ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, dispatcher->ref);
        dispatcher->ref = LUA_NOREF;
    }
    if (dispatcher->nameref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, dispatcher->nameref);
        dispatcher->nameref = LUA_NOREF;
    }

    return 0;
}

static int dispatcherCall(lua_State* L) {
    return Internal::configError(L, "dispatcher objects cannot be called directly; use hl.dispatch(dispatcher)");
}

static int dispatcherToString(lua_State* L) {
    auto*       dispatcher = sc<SDispatcherRef*>(luaL_checkudata(L, 1, DISPATCHER_MT));
    std::string str;
    if (dispatcher->nameref != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, dispatcher->nameref);
        str = lua_tostring(L, -1);
        lua_pop(L, 1);
    } else
        str = "INVALID";
    lua_pushstring(L, std::format("HL.Dispatcher({})", str).c_str());
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

static int dispatcherFactory(lua_State* L) {
    const int nargs   = lua_gettop(L);
    const int maxArgs = sc<int>(lua_tointeger(L, lua_upvalueindex(3)));
    if (nargs > maxArgs)
        return Internal::configError(L, std::format("{}: expected at most {} argument{}, got {}", lua_tostring(L, lua_upvalueindex(2)), maxArgs, maxArgs == 1 ? "" : "s", nargs));

    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_call(L, nargs, LUA_MULTRET);

    const int nresults = lua_gettop(L);
    if (nresults == 1 && lua_isfunction(L, -1)) {
        lua_pushvalue(L, lua_upvalueindex(2));
        return Internal::wrapDispatcher(L);
    }

    return nresults;
}

void Internal::setFn(lua_State* L, const char* name, lua_CFunction fn) {
    lua_pushcfunction(L, fn);
    lua_setfield(L, -2, name);
}

void Internal::setDispatcherFn(lua_State* L, const char* name, lua_CFunction fn, int maxArgs) {
    lua_pushcfunction(L, fn);
    lua_pushstring(L, name);
    lua_pushinteger(L, maxArgs);
    lua_pushcclosure(L, dispatcherFactory, 3);
    lua_setfield(L, -2, name);
}

int Internal::wrapDispatcher(lua_State* L) {
    luaL_checktype(L, -1, LUA_TSTRING);
    const int nameref = luaL_ref(L, LUA_REGISTRYINDEX);

    luaL_checktype(L, -1, LUA_TFUNCTION);
    const int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    new (lua_newuserdata(L, sizeof(SDispatcherRef))) SDispatcherRef{.ref = ref, .nameref = nameref};

    ensureDispatcherMetatable(L);
    luaL_getmetatable(L, DISPATCHER_MT);
    lua_setmetatable(L, -2);

    return 1;
}

std::expected<void, std::string> Internal::pushDispatcherFunction(lua_State* L, int idx) {
    if (lua_iscfunction(L, idx) && lua_tocfunction(L, idx) == dispatcherFactory)
        return std::unexpected("dispatcher factory supplied instead of a dispatcher; call the factory first (missing parentheses?)");

    if (lua_isfunction(L, idx)) {
        lua_pushvalue(L, idx);
        return {};
    }

    auto* dispatcher = sc<SDispatcherRef*>(luaL_testudata(L, idx, DISPATCHER_MT));
    if (!dispatcher || dispatcher->ref == LUA_NOREF)
        return std::unexpected("expected a dispatcher (e.g. hl.dsp.window.close()) or a lua function");

    lua_rawgeti(L, LUA_REGISTRYINDEX, dispatcher->ref);
    if (lua_isfunction(L, -1))
        return {};

    lua_pop(L, 1);
    return std::unexpected("dispatcher has no valid function");
}
