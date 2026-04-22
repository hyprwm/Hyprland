#include "LuaBindingsInternal.hpp"

#include "../objects/LuaEventSubscription.hpp"
#include "../objects/LuaKeybind.hpp"
#include "../objects/LuaLayerRule.hpp"
#include "../objects/LuaNotification.hpp"
#include "../objects/LuaTimer.hpp"
#include "../objects/LuaWindowRule.hpp"

using namespace Config;
using namespace Config::Lua;
using namespace Config::Lua::Bindings;

static int hlPrint(lua_State* L) {
    const int   n = lua_gettop(L);
    std::string out;
    for (int i = 1; i <= n; i++) {
        size_t      len = 0;
        const char* s   = luaL_tolstring(L, i, &len);
        if (i > 1)
            out += '\t';
        out.append(s, len);
        lua_pop(L, 1);
    }
    Log::logger->log(Log::INFO, "[Lua] {}", out);
    return 0;
}

void Internal::registerBindingsImpl(lua_State* L, CConfigManager* mgr) {
    Objects::CLuaTimer{}.setup(L);
    Objects::CLuaEventSubscription{}.setup(L);
    Objects::CLuaWindowRule{}.setup(L);
    Objects::CLuaLayerRule{}.setup(L);
    Objects::CLuaKeybind{}.setup(L);
    Objects::CLuaNotification{}.setup(L);

    g_pKeybindManager->m_dispatchers["__lua"] = [L](std::string arg) -> SDispatchResult {
        int ref = std::stoi(arg);
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

        int status = LUA_OK;
        if (auto* mgr = CConfigManager::fromLuaState(L); mgr)
            status = mgr->guardedPCall(0, 0, 0, CConfigManager::LUA_TIMEOUT_KEYBIND_CALLBACK_MS, "keybind callback");
        else
            status = lua_pcall(L, 0, 0, 0);

        if (status != LUA_OK) {
            Config::Lua::mgr()->addError(std::format("error in keybind lambda: {}", lua_tostring(L, -1)));
            lua_pop(L, 1);
        }
        return {};
    };

    lua_newtable(L);

    Internal::registerConfigRuleBindings(L, mgr);
    Internal::registerToplevelBindings(L, mgr);
    Internal::registerQueryBindings(L);
    Internal::registerDispatcherBindings(L);
    Internal::registerNotificationBindings(L);

    lua_setglobal(L, "hl");

    lua_pushcfunction(L, hlPrint);
    lua_setglobal(L, "print");
}
