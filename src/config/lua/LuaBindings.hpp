#pragma once

#include <string>
#include <vector>
extern "C" {
#include <lua.h>
}

namespace Config::Lua {
    class CConfigManager;
}

namespace Config::Lua::Bindings {
    void registerBindings(lua_State* L, CConfigManager* mgr);

    //
    void                                              warnDeprecated(std::string what, std::string why);
    std::vector<std::pair<std::string, std::string>>& deprecations();
}
