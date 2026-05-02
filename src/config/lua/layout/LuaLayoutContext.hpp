#pragma once

#include "../../../helpers/math/Math.hpp"
#include "../../../helpers/memory/Memory.hpp"

#include <vector>

extern "C" {
#include <lua.h>
}

namespace Layout {
    class ITarget;
}

namespace Config::Lua::Layouts {
    void pushBox(lua_State* L, const CBox& box);
    bool boxFromTable(lua_State* L, int idx, CBox& box);
    void pushLayoutContext(lua_State* L, const std::vector<SP<Layout::ITarget>>& targets, const CBox& area);
}
