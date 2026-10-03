#include "LuaBindings.hpp"

#include "bindings/LuaBindingsInternal.hpp"

void Config::Lua::Bindings::registerBindings(lua_State* L, CConfigManager* mgr) {
    Internal::registerBindingsImpl(L, mgr);
}

namespace {
    std::vector<std::pair<std::string, std::string>> deprecated;
}

void Config::Lua::Bindings::warnDeprecated(std::string what, std::string why) {
    deprecated.emplace_back(what, why);
}

std::vector<std::pair<std::string, std::string>>& Config::Lua::Bindings::deprecations() {
    return deprecated;
}
