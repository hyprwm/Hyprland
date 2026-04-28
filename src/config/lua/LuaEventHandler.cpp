#include "LuaEventHandler.hpp"
#include "ConfigManager.hpp"
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
#include <algorithm>

using namespace Config::Lua;
using namespace Config::Lua::Objects;

void CLuaEventHandler::dispatch(const std::string& name, int nargs, const std::function<void()>& pushArgs) {
    auto it = m_callbacks.find(name);
    if (it == m_callbacks.end() || it->second.empty())
        return;

    if (m_dispatchDepth >= MAX_DISPATCH_DEPTH) {
        Log::logger->log(Log::WARN, "[LuaEvents] max dispatch depth ({}) reached while handling '{}'", MAX_DISPATCH_DEPTH, name);
        return;
    }

    const auto handles = it->second;

    for (const auto handle : handles) {
        const auto sub = m_subscriptions.find(handle);
        if (sub == m_subscriptions.end())
            continue;

        if (m_activeHandles.contains(handle)) {
            if (m_reentrancyWarnedHandles.emplace(handle).second)
                Log::logger->log(Log::WARN, "[LuaEvents] suppressed recursive hl.on(\"{}\") callback invocation", name);
            continue;
        }

        struct SDispatchScope {
            CLuaEventHandler* self   = nullptr;
            uint64_t          handle = 0;

            ~SDispatchScope() {
                if (!self)
                    return;

                self->m_activeHandles.erase(handle);
                if (self->m_dispatchDepth > 0)
                    --self->m_dispatchDepth;
            }
        } dispatchScope{.self = this, .handle = handle};

        m_activeHandles.emplace(handle);
        ++m_dispatchDepth;

        lua_rawgeti(m_lua, LUA_REGISTRYINDEX, sub->second.luaRef);
        pushArgs();

        int status = LUA_OK;
        if (auto* mgr = CConfigManager::fromLuaState(m_lua); mgr)
            status = mgr->guardedPCall(nargs, 0, 0, CConfigManager::LUA_TIMEOUT_EVENT_CALLBACK_MS, std::format("hl.on(\"{}\") callback", name));
        else
            status = lua_pcall(m_lua, nargs, 0, 0);

        if (status != LUA_OK) {
            const char* err = lua_tostring(m_lua, -1);
            Config::Lua::mgr()->addError(std::format("hl.on(\"{}\") callback: {}", name, err ? err : "(unknown)"));
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
            lua_pushinteger(m_lua, sc<lua_Integer>(r));
        });
    }));
    m_listeners.push_back(bus()->m_events.window.urgent.listen([this](PHLWINDOW w) { dispatch("window.urgent", 1, [&] { CLuaWindow::push(m_lua, w); }); }));
    m_listeners.push_back(bus()->m_events.window.title.listen([this](PHLWINDOW w) { dispatch("window.title", 1, [&] { CLuaWindow::push(m_lua, w); }); }));
    m_listeners.push_back(bus()->m_events.window.class_.listen([this](PHLWINDOW w) { dispatch("window.class", 1, [&] { CLuaWindow::push(m_lua, w); }); }));
    m_listeners.push_back(bus()->m_events.window.pin.listen([this](PHLWINDOW w) { dispatch("window.pin", 1, [&] { CLuaWindow::push(m_lua, w); }); }));
    m_listeners.push_back(bus()->m_events.window.fullscreen.listen([this](PHLWINDOW w) { dispatch("window.fullscreen", 1, [&] { CLuaWindow::push(m_lua, w); }); }));
    m_listeners.push_back(bus()->m_events.window.updateRules.listen([this](PHLWINDOW w) { dispatch("window.update_rules", 1, [&] { CLuaWindow::push(m_lua, w); }); }));
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
            lua_pushinteger(m_lua, sc<lua_Integer>(type));
            lua_pushstring(m_lua, name.c_str());
        });
    }));

    m_listeners.push_back(bus()->m_events.start.listen([this]() { dispatch("hyprland.start", 0, [] {}); }));
    m_listeners.push_back(bus()->m_events.exit.listen([this]() { dispatch("hyprland.shutdown", 0, [] {}); }));
}

CLuaEventHandler::~CLuaEventHandler() {
    clearEvents();
}

std::optional<uint64_t> CLuaEventHandler::registerEvent(const std::string& name, int luaRef) {
    if (!knownEvents().contains(name))
        return std::nullopt;

    const auto handle = m_nextHandle++;
    m_callbacks[name].push_back(handle);
    m_subscriptions[handle] = {.eventName = name, .luaRef = luaRef};

    return handle;
}

bool CLuaEventHandler::unregisterEvent(uint64_t handle) {
    const auto it = m_subscriptions.find(handle);
    if (it == m_subscriptions.end())
        return false;

    luaL_unref(m_lua, LUA_REGISTRYINDEX, it->second.luaRef);

    auto callbacksIt = m_callbacks.find(it->second.eventName);
    if (callbacksIt != m_callbacks.end()) {
        std::erase(callbacksIt->second, handle);
        if (callbacksIt->second.empty())
            m_callbacks.erase(callbacksIt);
    }

    m_subscriptions.erase(it);
    m_activeHandles.erase(handle);
    m_reentrancyWarnedHandles.erase(handle);

    return true;
}

void CLuaEventHandler::clearEvents() {
    for (const auto& s : m_subscriptions) {
        luaL_unref(m_lua, LUA_REGISTRYINDEX, s.second.luaRef);
    }

    m_subscriptions.clear();
    m_activeHandles.clear();
    m_reentrancyWarnedHandles.clear();
    m_callbacks.clear();
}

const std::unordered_set<std::string>& CLuaEventHandler::knownEvents() {
    static const std::unordered_set<std::string> EVENTS = {
        "window.open",
        "window.open_early",
        "window.close",
        "window.destroy",
        "window.kill",
        "window.active",
        "window.urgent",
        "window.title",
        "window.class",
        "window.pin",
        "window.fullscreen",
        "window.update_rules",
        "window.move_to_workspace",
        "layer.opened",
        "layer.closed",
        "monitor.added",
        "monitor.removed",
        "monitor.focused",
        "monitor.layout_changed",
        "workspace.active",
        "workspace.created",
        "workspace.removed",
        "workspace.move_to_monitor",
        "config.reloaded",
        "keybinds.submap",
        "screenshare.state",
        "hyprland.start",
        "hyprland.shutdown",
    };
    return EVENTS;
}
