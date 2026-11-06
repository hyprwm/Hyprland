#pragma once

#include <cstdint>

extern "C" {
#include <lua.h>
}

namespace Config::Lua {
    // whether a binding may run inside a predicate, which must not change compositor or config state
    enum ePredicateAccess : uint8_t {
        ALLOWED_IN_PREDICATE = 0,
        FORBIDDEN_IN_PREDICATE,
    };

    // pushes fn as a closure over the nupvalues values on top of the stack
    void pushFn(lua_State* L, lua_CFunction fn, ePredicateAccess access, int nupvalues = 0);
}
