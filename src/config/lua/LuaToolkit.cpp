#include "LuaToolkit.hpp"
#include "LuaCaller.hpp"

#include <hyprtoolkit-lua/Embedding.hpp>
#include <hyprtoolkit-lua/LuaBindings.hpp>
#include <hyprtoolkit/core/EmbeddedBackend.hpp>

#include <format>
#include <tuple>

#include "../../debug/log/Logger.hpp"
#include "../../hyprtoolkit/Host.hpp"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Config::Lua;

static int unavailableModule(lua_State* L) {
    return luaL_error(L, "hyprtoolkit is unavailable, check the log for details");
}

static void setPreload(lua_State* L, lua_CFunction loader) {
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");
    lua_pushcfunction(L, loader);
    lua_setfield(L, -2, "hyprtoolkit");
    lua_pop(L, 2);
}

CLuaToolkit::CLuaToolkit(lua_State* L, CLuaCaller& caller, FErrorCallback&& onError) : m_lua(L), m_onError(std::move(onError)) {
    // the preload always wins over package.cpath, which could find a standalone module.
    // That one would happily create its own backend: a wayland client of this very compositor.
    setPreload(m_lua, unavailableModule);

    // opening the module once registers the bindings, they have to exist to be reserved
    lua_pushcfunction(m_lua, Hyprtoolkit::Lua::openModule);
    if (lua_pcall(m_lua, 0, 0, 0) != LUA_OK) {
        LOG(Log::ERR, "[Lua] failed to register hyprtoolkit bindings: {}", lua_tostring(m_lua, -1));
        lua_pop(m_lua, 1);
        return;
    }

    if (const auto RES = Hyprtoolkit::Lua::reserveBackend(m_lua); !RES) {
        LOG(Log::ERR, "[Lua] failed to reserve the hyprtoolkit backend: {}", RES.error());
        std::ignore = Hyprtoolkit::Lua::retireState(m_lua);
        return;
    }

    // run toolkit callbacks under the config's watchdog
    Hyprtoolkit::Lua::SCallbackHooks hooks;
    hooks.protectedCall = [&caller](lua_State* L, int nargs, int nresults) {
        return caller.guardedPCall(L, nargs, nresults, 0, CLuaCaller::LUA_TIMEOUT_TOOLKIT_CALLBACK_MS, "hyprtoolkit callback");
    };
    hooks.reportError = [this](std::string_view error) { reportError(error); };

    std::ignore = Hyprtoolkit::Lua::setCallbackHooks(m_lua, std::move(hooks));

    m_active = true;
    setPreload(m_lua, Hyprtoolkit::Lua::openModule);

    if (!Toolkit::host())
        return;

    m_listeners.hostReady   = Toolkit::host()->m_events.ready.listen([this] { attach(); });
    m_listeners.hostDestroy = Toolkit::host()->m_events.destroy.listen([this] { retire(); });

    attach();
}

CLuaToolkit::~CLuaToolkit() {
    retire();
}

void CLuaToolkit::attach() {
    if (!m_active || !Toolkit::host() || !Toolkit::host()->backend())
        return;

    if (const auto RES = Hyprtoolkit::Lua::attachBackend(m_lua, Toolkit::host()->backend()); !RES)
        LOG(Log::ERR, "[Lua] failed to attach the hyprtoolkit backend: {}", RES.error());
}

void CLuaToolkit::retire() {
    if (!m_active)
        return;

    m_active = false;

    if (const auto RES = Hyprtoolkit::Lua::retireState(m_lua); !RES)
        LOG(Log::ERR, "[Lua] failed to retire hyprtoolkit bindings: {}", RES.error());
}

void CLuaToolkit::reportError(std::string_view error) {
    if (!m_reportedErrors.emplace(error).second)
        return;

    LOG(Log::ERR, "[Lua] hyprtoolkit callback: {}", error);
    m_onError(std::format("hyprtoolkit callback: {}", error));
}
