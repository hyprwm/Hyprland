#include "LuaEventHandler.hpp"
#include "objects/LuaWindow.hpp"
#include "objects/LuaWorkspace.hpp"
#include "objects/LuaMonitor.hpp"
#include "objects/LuaLayerSurface.hpp"

#include "../../defines.hpp"
#include "../../event/EventBus.hpp"
#include "../../desktop/state/FocusState.hpp"

extern "C" {
#include <lauxlib.h>
}

#include <format>

using namespace Config::Lua;
using namespace Config::Lua::Objects;

void CLuaEventHandler::dispatch(const std::string& name, int nargs, const std::function<void()>& pushArgs) {
    auto it = m_callbacks.find(name);
    if (it == m_callbacks.end() || it->second.empty())
        return;

    for (int ref : it->second) {
        lua_rawgeti(m_lua, LUA_REGISTRYINDEX, ref);
        pushArgs();
        if (lua_pcall(m_lua, nargs, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(m_lua, -1);
            Log::logger->log(Log::WARN, std::format("[LuaEvents] error in hl.on(\"{}\") callback: {}", name, err ? err : "(unknown)"));
            lua_pop(m_lua, 1);
        }
    }
}

CLuaEventHandler::CLuaEventHandler(lua_State* L) : m_lua(L) {
    CLuaWindow{}.setup(L);
    CLuaWorkspace{}.setup(L);
    CLuaMonitor{}.setup(L);
    CLuaLayerSurface{}.setup(L);

    using namespace Event;

    m_listeners.push_back(bus()->m_events.window.open.listen([this](PHLWINDOW w) { dispatch("window.open", 1, [&] { CLuaWindow::push(m_lua, w); }); }));
    m_listeners.push_back(bus()->m_events.window.openEarly.listen([this](PHLWINDOW w) { dispatch("window.open_early", 1, [&] { CLuaWindow::push(m_lua, w); }); }));
    m_listeners.push_back(bus()->m_events.window.close.listen([this](PHLWINDOW w) { dispatch("window.close", 1, [&] { CLuaWindow::push(m_lua, w); }); }));
    m_listeners.push_back(bus()->m_events.window.destroy.listen([this](PHLWINDOW w) { dispatch("window.destroy", 1, [&] { CLuaWindow::push(m_lua, w); }); }));
    m_listeners.push_back(bus()->m_events.window.kill.listen([this](PHLWINDOW w) { dispatch("window.kill", 1, [&] { CLuaWindow::push(m_lua, w); }); }));
    m_listeners.push_back(bus()->m_events.window.active.listen([this](PHLWINDOW w, Desktop::eFocusReason r) {
        dispatch("window.active", 2, [&] {
            CLuaWindow::push(m_lua, w);
            lua_pushinteger(m_lua, static_cast<lua_Integer>(r));
        });
    }));
    m_listeners.push_back(bus()->m_events.window.urgent.listen([this](PHLWINDOW w) { dispatch("window.urgent", 1, [&] { CLuaWindow::push(m_lua, w); }); }));
    m_listeners.push_back(bus()->m_events.window.title.listen([this](PHLWINDOW w) { dispatch("window.title", 1, [&] { CLuaWindow::push(m_lua, w); }); }));
    m_listeners.push_back(bus()->m_events.window.class_.listen([this](PHLWINDOW w) { dispatch("window.class", 1, [&] { CLuaWindow::push(m_lua, w); }); }));
    m_listeners.push_back(bus()->m_events.window.pin.listen([this](PHLWINDOW w) { dispatch("window.pin", 1, [&] { CLuaWindow::push(m_lua, w); }); }));
    m_listeners.push_back(bus()->m_events.window.fullscreen.listen([this](PHLWINDOW w) { dispatch("window.fullscreen", 1, [&] { CLuaWindow::push(m_lua, w); }); }));
    m_listeners.push_back(bus()->m_events.window.moveToWorkspace.listen([this](PHLWINDOW w, PHLWORKSPACE ws) {
        dispatch("window.move_to_workspace", 2, [&] {
            CLuaWindow::push(m_lua, w);
            CLuaWorkspace::push(m_lua, ws);
        });
    }));

    m_listeners.push_back(bus()->m_events.layer.opened.listen([this](PHLLS ls) { dispatch("layer.opened", 1, [&] { CLuaLayerSurface::push(m_lua, ls); }); }));
    m_listeners.push_back(bus()->m_events.layer.closed.listen([this](PHLLS ls) { dispatch("layer.closed", 1, [&] { CLuaLayerSurface::push(m_lua, ls); }); }));

    m_listeners.push_back(bus()->m_events.monitor.added.listen([this](PHLMONITOR mon) { dispatch("monitor.added", 1, [&] { CLuaMonitor::push(m_lua, mon); }); }));
    m_listeners.push_back(bus()->m_events.monitor.removed.listen([this](PHLMONITOR mon) { dispatch("monitor.removed", 1, [&] { CLuaMonitor::push(m_lua, mon); }); }));
    m_listeners.push_back(bus()->m_events.monitor.focused.listen([this](PHLMONITOR mon) { dispatch("monitor.focused", 1, [&] { CLuaMonitor::push(m_lua, mon); }); }));
    m_listeners.push_back(bus()->m_events.monitor.layoutChanged.listen([this] { dispatch("monitor.layout_changed", 0, [] {}); }));

    m_listeners.push_back(bus()->m_events.workspace.active.listen([this](PHLWORKSPACE ws) { dispatch("workspace.active", 1, [&] { CLuaWorkspace::push(m_lua, ws); }); }));
    m_listeners.push_back(bus()->m_events.workspace.created.listen([this](PHLWORKSPACEREF wsRef) {
        const auto ws = wsRef.lock();
        if (!ws)
            return;
        dispatch("workspace.created", 1, [&] { CLuaWorkspace::push(m_lua, ws); });
    }));
    m_listeners.push_back(bus()->m_events.workspace.removed.listen([this](PHLWORKSPACEREF wsRef) {
        const auto ws = wsRef.lock();
        if (!ws)
            return;
        dispatch("workspace.removed", 1, [&] { CLuaWorkspace::push(m_lua, ws); });
    }));
    m_listeners.push_back(bus()->m_events.workspace.moveToMonitor.listen([this](PHLWORKSPACE ws, PHLMONITOR mon) {
        dispatch("workspace.move_to_monitor", 2, [&] {
            CLuaWorkspace::push(m_lua, ws);
            CLuaMonitor::push(m_lua, mon);
        });
    }));

    m_listeners.push_back(bus()->m_events.config.reloaded.listen([this] { dispatch("config.reloaded", 0, [] {}); }));
    m_listeners.push_back(
        bus()->m_events.keybinds.submap.listen([this](const std::string& submap) { dispatch("keybinds.submap", 1, [&] { lua_pushstring(m_lua, submap.c_str()); }); }));
    m_listeners.push_back(bus()->m_events.screenshare.state.listen([this](bool state, uint8_t type, const std::string& name) {
        dispatch("screenshare.state", 3, [&] {
            lua_pushboolean(m_lua, state);
            lua_pushinteger(m_lua, static_cast<lua_Integer>(type));
            lua_pushstring(m_lua, name.c_str());
        });
    }));
}

CLuaEventHandler::~CLuaEventHandler() {
    for (auto& [_, refs] : m_callbacks)
        for (int ref : refs)
            luaL_unref(m_lua, LUA_REGISTRYINDEX, ref);
}

bool CLuaEventHandler::registerEvent(const std::string& name, int luaRef) {
    if (!knownEvents().contains(name))
        return false;
    m_callbacks[name].push_back(luaRef);
    return true;
}

const std::unordered_set<std::string>& CLuaEventHandler::knownEvents() {
    static const std::unordered_set<std::string> EVENTS = {
        "window.open",       "window.open_early",
        "window.close",      "window.destroy",
        "window.kill",       "window.active",
        "window.urgent",     "window.title",
        "window.class",      "window.pin",
        "window.fullscreen", "window.move_to_workspace",
        "layer.opened",      "layer.closed",
        "monitor.added",     "monitor.removed",
        "monitor.focused",   "monitor.layout_changed",
        "workspace.active",  "workspace.created",
        "workspace.removed", "workspace.move_to_monitor",
        "config.reloaded",   "keybinds.submap",
        "screenshare.state",
    };
    return EVENTS;
}
