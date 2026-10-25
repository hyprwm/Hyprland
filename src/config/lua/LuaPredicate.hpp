#pragma once

#include "LuaCaller.hpp"

#include "../shared/complex/PredicateData.hpp"
#include "../../desktop/DesktopTypes.hpp"

#include <cstdint>
#include <string>

namespace Config::Lua {
    // whether a binding may run inside a predicate, which must not change compositor or config state
    enum ePredicateAccess : uint8_t {
        ALLOWED_IN_PREDICATE = 0,
        FORBIDDEN_IN_PREDICATE,
    };

    // pushes fn as a closure over the nupvalues values on top of the stack
    void pushFn(lua_State* L, lua_CFunction fn, ePredicateAccess access, int nupvalues = 0);

    void pushArg(lua_State* L, const PHLWORKSPACE& workspace);

    // runs fn through the caller of its state, so a reload can't leave it dangling
    template <typename... Args>
    CPredicateData<Args...> makePredicate(SP<CLuaRef> fn, std::string context) {
        return CPredicateData<Args...>([fn = std::move(fn), context = std::move(context)](const Args&... args) {
            auto* caller = CLuaCaller::fromLuaState(fn->state());
            if (!caller)
                return false;

            return caller->predicate(*fn, context, [&](lua_State* L) {
                (pushArg(L, args), ...);
                return sc<int>(sizeof...(Args));
            });
        });
    }
}
