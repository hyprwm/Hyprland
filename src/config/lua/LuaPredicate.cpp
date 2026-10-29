#include "LuaPredicate.hpp"

#include "objects/LuaWorkspace.hpp"
#include "../../macros.hpp"

using namespace Config::Lua;

// calls fn in place, so fn keeps its own upvalues (the first NUPVALUES) and the caller's stack level
template <int NUPVALUES>
static int predicateForbidden(lua_State* L) {
    const auto* caller = CLuaCaller::fromLuaState(L);
    if (!caller || !caller->inPredicate())
        return lua_tocfunction(L, lua_upvalueindex(NUPVALUES + 1))(L);

    // lua_error longjmps, so nothing with a destructor may live here
    lua_Debug ar = {};
    luaL_where(L, 1);
    if (lua_getstack(L, 0, &ar) && lua_getinfo(L, "n", &ar) && ar.name)
        lua_pushfstring(L, "'%s' can't be used in a predicate, defer it with hl.timer", ar.name);
    else
        lua_pushliteral(L, "this function can't be used in a predicate, defer it with hl.timer");
    lua_concat(L, 2);
    return lua_error(L);
}

void Config::Lua::pushFn(lua_State* L, lua_CFunction fn, ePredicateAccess access, int nupvalues) {
    if (access == ALLOWED_IN_PREDICATE) {
        lua_pushcclosure(L, fn, nupvalues);
        return;
    }

    lua_pushcfunction(L, fn);

    switch (nupvalues) {
        case 0: lua_pushcclosure(L, predicateForbidden<0>, 1); break;
        case 1: lua_pushcclosure(L, predicateForbidden<1>, 2); break;
        default: RASSERT(false, "pushFn: unsupported upvalue count {}", nupvalues);
    }
}

void Config::Lua::pushArg(lua_State* L, const PHLWORKSPACE& workspace) {
    Objects::CLuaWorkspace::push(L, workspace);
}
