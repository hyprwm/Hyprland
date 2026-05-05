#pragma once

#include "../../../helpers/memory/Memory.hpp"

#include <cstddef>

extern "C" {
#include <lua.h>
}

namespace Layout {
    class ITarget;
}

namespace Config::Lua::Layouts {
    void setupLayoutTarget(lua_State* L);
    void pushLayoutTarget(lua_State* L, const SP<Layout::ITarget>& target, size_t index);
}
