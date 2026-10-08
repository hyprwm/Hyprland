#include "LuaBindings.hpp"

#include "bindings/LuaBindingsInternal.hpp"

void Config::Lua::Bindings::registerBindings(lua_State* L, CConfigManager* mgr) {
    Internal::registerBindingsImpl(L, mgr);
}
