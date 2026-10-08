#pragma once

extern "C" {
#include <lua.h>
}

namespace Config::Lua {
    class CConfigManager;
}

namespace Config::Lua::Bindings {
    void registerBindings(lua_State* L, CConfigManager* mgr);
}
