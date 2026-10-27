#include "LuaEventHandler.hpp"
#include "LuaCaller.hpp"
#include "objects/LuaWindow.hpp"
#include "objects/LuaWorkspace.hpp"
#include "objects/LuaGroup.hpp"
#include "objects/LuaMonitor.hpp"
#include "objects/LuaLayerSurface.hpp"

#include "../../event/EventBus.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../desktop/view/window/WindowStateTracker.hpp"
#include <expected>
#include <vector>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <format>

using namespace Config::Lua;
using namespace Config::Lua::Objects;

void CLuaEventHandler::dispatch(const std::string& name, int nargs, const std::function<void(lua_State*)>& pushArgs) {
    const auto it = m_callbacks.find(name);
    if (it == m_callbacks.end() || it->second.empty())
        return;

    const auto CONTEXT = std::format("hl.on(\"{}\") callback", name);
    const auto HANDLES = it->second;

    for (const auto handle : HANDLES) {
        const auto sub = m_subscriptions.find(handle);
        if (sub == m_subscriptions.end())
            continue;

        m_caller.call(sub->second.fn, CLuaCaller::LUA_TIMEOUT_EVENT_CALLBACK_MS, CONTEXT, [&](lua_State* L) {
            pushArgs(L);
            return nargs;
        });
    }
}

CLuaEventHandler::CLuaEventHandler(CLuaCaller& caller) : m_caller(caller) {
    const auto L = m_caller.state();

    CLuaWindow{}.setup(L);
    Objects::CLuaGroup{}.setup(L);
    CLuaWorkspace{}.setup(L);
    CLuaMonitor{}.setup(L);
    CLuaLayerSurface{}.setup(L);

    using namespace Event;

    // openLate so that actual things people expect to happen will happen.
    m_listeners.push_back(bus()->m_events.window.openLate.listen([this](PHLWINDOW w) { dispatch("window.open", 1, [&](lua_State* L) { CLuaWindow::push(L, w); }); }));
    m_listeners.push_back(bus()->m_events.window.openEarly.listen([this](PHLWINDOW w) { dispatch("window.open_early", 1, [&](lua_State* L) { CLuaWindow::push(L, w); }); }));
    m_listeners.push_back(bus()->m_events.window.close.listen([this](PHLWINDOW w) { dispatch("window.close", 1, [&](lua_State* L) { CLuaWindow::push(L, w); }); }));
    m_listeners.push_back(bus()->m_events.window.destroy.listen([this](PHLWINDOWREF w) { dispatch("window.destroy", 1, [&](lua_State* L) { CLuaWindow::push(L, w.lock()); }); }));
    m_listeners.push_back(bus()->m_events.window.kill.listen([this](PHLWINDOW w) { dispatch("window.kill", 1, [&](lua_State* L) { CLuaWindow::push(L, w); }); }));
    m_listeners.push_back(bus()->m_events.window.active.listen([this](PHLWINDOW w, Desktop::eFocusReason r) {
        dispatch("window.active", 2, [&](lua_State* L) {
            CLuaWindow::push(L, w);
            lua_pushinteger(L, sc<lua_Integer>(r));
        });
    }));
    m_listeners.push_back(bus()->m_events.window.urgent.listen([this](PHLWINDOW w) { dispatch("window.urgent", 1, [&](lua_State* L) { CLuaWindow::push(L, w); }); }));
    m_listeners.push_back(bus()->m_events.window.title.listen([this](PHLWINDOW w) { dispatch("window.title", 1, [&](lua_State* L) { CLuaWindow::push(L, w); }); }));
    m_listeners.push_back(bus()->m_events.window.class_.listen([this](PHLWINDOW w) { dispatch("window.class", 1, [&](lua_State* L) { CLuaWindow::push(L, w); }); }));
    m_listeners.push_back(bus()->m_events.window.pin.listen([this](PHLWINDOW w) { dispatch("window.pin", 1, [&](lua_State* L) { CLuaWindow::push(L, w); }); }));
    m_listeners.push_back(bus()->m_events.window.fullscreen.listen([this](PHLWINDOW w) { dispatch("window.fullscreen", 1, [&](lua_State* L) { CLuaWindow::push(L, w); }); }));
    m_listeners.push_back(bus()->m_events.window.updateState.listen([this](PHLWINDOW w, const Desktop::View::SWindowStateUpdate& changes) {
        dispatch("window.update_state", 2, [&](lua_State* L) {
            CLuaWindow::push(L, w);
            lua_createtable(L, 0, 2);
            if (changes.floating.has_value()) {
                lua_pushboolean(L, *changes.floating);
                lua_setfield(L, -2, "float");
            }
            if (changes.pinned.has_value()) {
                lua_pushboolean(L, *changes.pinned);
                lua_setfield(L, -2, "pin");
            }
        });
    }));
    m_listeners.push_back(bus()->m_events.window.updateRules.listen([this](PHLWINDOW w) { dispatch("window.update_rules", 1, [&](lua_State* L) { CLuaWindow::push(L, w); }); }));
    m_listeners.push_back(bus()->m_events.window.moveToWorkspace.listen([this](PHLWINDOW w, PHLWORKSPACE ws) {
        dispatch("window.move_to_workspace", 2, [&](lua_State* L) {
            CLuaWindow::push(L, w);
            CLuaWorkspace::push(L, ws);
        });
    }));
    m_listeners.push_back(
        bus()->m_events.window.bell.listen([this](PHLWINDOW w, Event::SCallbackInfo&) { dispatch("window.bell", 1, [&](lua_State* L) { CLuaWindow::push(L, w); }); }));
    m_listeners.push_back(bus()->m_events.window.minimize.listen([this](PHLWINDOW w, bool s) {
        dispatch("window.minimize", 2, [&](lua_State* L) {
            CLuaWindow::push(L, w);
            lua_pushboolean(L, s);
        });
    }));

    m_listeners.push_back(bus()->m_events.layer.opened.listen([this](PHLLS ls) { dispatch("layer.opened", 1, [&](lua_State* L) { CLuaLayerSurface::push(L, ls); }); }));
    m_listeners.push_back(bus()->m_events.layer.closed.listen([this](PHLLS ls) { dispatch("layer.closed", 1, [&](lua_State* L) { CLuaLayerSurface::push(L, ls); }); }));

    m_listeners.push_back(bus()->m_events.monitor.added.listen([this](PHLMONITOR mon) { dispatch("monitor.added", 1, [&](lua_State* L) { CLuaMonitor::push(L, mon); }); }));
    m_listeners.push_back(bus()->m_events.monitor.removed.listen([this](PHLMONITOR mon) { dispatch("monitor.removed", 1, [&](lua_State* L) { CLuaMonitor::push(L, mon); }); }));
    m_listeners.push_back(bus()->m_events.monitor.focused.listen([this](PHLMONITOR mon) { dispatch("monitor.focused", 1, [&](lua_State* L) { CLuaMonitor::push(L, mon); }); }));
    m_listeners.push_back(bus()->m_events.monitor.layoutChanged.listen([this] { dispatch("monitor.layout_changed", 0, [](lua_State* L) {}); }));

    m_listeners.push_back(bus()->m_events.workspace.active.listen([this](PHLWORKSPACE ws) { dispatch("workspace.active", 1, [&](lua_State* L) { CLuaWorkspace::push(L, ws); }); }));
    m_listeners.push_back(bus()->m_events.workspace.specialActive.listen([this](PHLWORKSPACE ws, PHLMONITOR mon) {
        dispatch("workspace.special_active", 2, [&](lua_State* L) {
            CLuaWorkspace::push(L, ws);
            CLuaMonitor::push(L, mon);
        });
    }));
    m_listeners.push_back(bus()->m_events.workspace.createdLate.listen([this](PHLWORKSPACEREF wsRef) {
        const auto ws = wsRef.lock();
        if (!ws)
            return;
        dispatch("workspace.created", 1, [&](lua_State* L) { CLuaWorkspace::push(L, ws); });
    }));
    m_listeners.push_back(bus()->m_events.workspace.createdEarly.listen([this](PHLWORKSPACEREF wsRef) {
        const auto ws = wsRef.lock();
        if (!ws)
            return;
        dispatch("workspace.created_early", 1, [&](lua_State* L) { CLuaWorkspace::push(L, ws); });
    }));
    m_listeners.push_back(bus()->m_events.workspace.removed.listen([this](PHLWORKSPACEREF wsRef) {
        if (!wsRef)
            return;
        dispatch("workspace.removed", 1, [&](lua_State* L) { CLuaWorkspace::push(L, wsRef); });
    }));
    m_listeners.push_back(bus()->m_events.workspace.moveToMonitor.listen([this](PHLWORKSPACE ws, PHLMONITOR mon) {
        dispatch("workspace.move_to_monitor", 2, [&](lua_State* L) {
            CLuaWorkspace::push(L, ws);
            CLuaMonitor::push(L, mon);
        });
    }));

    m_listeners.push_back(bus()->m_events.config.reloaded.listen([this] { dispatch("config.reloaded", 0, [](lua_State* L) {}); }));
    m_listeners.push_back(bus()->m_events.config.preReload.listen([this] { dispatch("config.unload", 0, [](lua_State* L) {}); }));
    m_listeners.push_back(bus()->m_events.config.props_refreshed.listen(
        [this](const bool execdAsScheduled) { dispatch("config.props_refreshed", 1, [&](lua_State* L) { lua_pushboolean(L, sc<lua_Integer>(execdAsScheduled)); }); }));

    m_listeners.push_back(
        bus()->m_events.keybinds.submap.listen([this](const std::string& submap) { dispatch("keybinds.submap", 1, [&](lua_State* L) { lua_pushstring(L, submap.c_str()); }); }));
    m_listeners.push_back(bus()->m_events.screenshare.state.listen([this](bool state, uint8_t type, const std::string& name) {
        dispatch("screenshare.state", 3, [&](lua_State* L) {
            lua_pushboolean(L, state);
            lua_pushinteger(L, sc<lua_Integer>(type));
            lua_pushstring(L, name.c_str());
        });
    }));

    m_listeners.push_back(bus()->m_events.start.listen([this]() { dispatch("hyprland.start", 0, [](lua_State* L) {}); }));
    m_listeners.push_back(bus()->m_events.exit.listen([this]() {
        dispatch("config.unload", 0, [](lua_State* L) {});
        dispatch("hyprland.shutdown", 0, [](lua_State* L) {});
    }));

    m_listeners.push_back(bus()->m_events.input.keyboard.key.listen([this](const IKeyboard::SKeyEvent& keyEvent, const SCallbackInfo& _) {
        dispatch("input.keyboard.key", 3, [&](lua_State* L) {
            lua_pushinteger(L, keyEvent.keycode + 8); // Because to xkbcommon it's +8 from libinput
            lua_pushinteger(L, keyEvent.timeMs);
            lua_pushinteger(L, keyEvent.state);
        });
    }));
}

CLuaEventHandler::~CLuaEventHandler() {
    clearEvents();
}

std::optional<uint64_t> CLuaEventHandler::registerEvent(const std::string& name, CLuaRef&& fn) {
    if (!knownEvents().contains(name))
        return std::nullopt;

    const auto handle = m_nextHandle++;
    m_callbacks[name].push_back(handle);
    m_subscriptions.emplace(handle, SSubscription{.eventName = name, .fn = std::move(fn)});

    return handle;
}

bool CLuaEventHandler::unregisterEvent(uint64_t handle) {
    const auto it = m_subscriptions.find(handle);
    if (it == m_subscriptions.end())
        return false;

    auto callbacksIt = m_callbacks.find(it->second.eventName);
    if (callbacksIt != m_callbacks.end()) {
        std::erase(callbacksIt->second, handle);
        if (callbacksIt->second.empty())
            m_callbacks.erase(callbacksIt);
    }

    m_subscriptions.erase(it);

    return true;
}

void CLuaEventHandler::clearEvents() {
    m_subscriptions.clear();
    m_callbacks.clear();
}

std::unordered_set<std::string> CLuaEventHandler::knownEvents() {
    std::unordered_set<std::string> EVENTS = {
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
        "window.update_state",
        "window.update_rules",
        "window.move_to_workspace",
        "window.bell",
        "window.minimize",
        "layer.opened",
        "layer.closed",
        "monitor.added",
        "monitor.removed",
        "monitor.focused",
        "monitor.layout_changed",
        "workspace.active",
        "workspace.special_active",
        "workspace.created",
        "workspace.created_early",
        "workspace.removed",
        "workspace.move_to_monitor",
        "config.reloaded",
        "config.props_refreshed",
        "config.unload",
        "keybinds.submap",
        "screenshare.state",
        "hyprland.start",
        "hyprland.shutdown",
        "input.keyboard.key",
    };
    return EVENTS;
}
