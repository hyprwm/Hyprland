#include "LuaEventHandler.hpp"
#include "ConfigManager.hpp"
#include "objects/LuaWindow.hpp"
#include "objects/LuaWorkspace.hpp"
#include "objects/LuaGroup.hpp"
#include "objects/LuaMonitor.hpp"
#include "objects/LuaLayerSurface.hpp"

#include "../../event/EventBus.hpp"
#include "../../desktop/state/FocusState.hpp"
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
        pushArgs(m_lua);

        auto* mgr = CConfigManager::fromLuaState(m_lua);

        int   status = LUA_OK;
        if (mgr)
            status = mgr->guardedPCall(nargs, 0, 0, CConfigManager::LUA_TIMEOUT_EVENT_CALLBACK_MS, std::format("hl.on(\"{}\") callback", name));
        else
            status = lua_pcall(m_lua, nargs, 0, 0);

        if (status != LUA_OK) {
            const char* err = lua_tostring(m_lua, -1);
            if (mgr)
                mgr->addError(std::format("hl.on(\"{}\") callback: {}", name, err ? err : "(unknown)"));
            lua_pop(m_lua, 1);
        }
    }
}

CLuaEventHandler::CLuaEventHandler(lua_State* L) : m_lua(L) {
    CLuaWindow{}.setup(L);
    Objects::CLuaGroup{}.setup(L);
    CLuaWorkspace{}.setup(L);
    CLuaMonitor{}.setup(L);
    CLuaLayerSurface{}.setup(L);

    using namespace Event;

    m_listeners.push_back(bus()->m_events.window.open.listen([this](PHLWINDOW w) { dispatch("window.open", 1, [&](lua_State* L) { CLuaWindow::push(L, w); }); }));
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
    m_listeners.push_back(bus()->m_events.window.updateRules.listen([this](PHLWINDOW w) { dispatch("window.update_rules", 1, [&](lua_State* L) { CLuaWindow::push(L, w); }); }));
    m_listeners.push_back(bus()->m_events.window.moveToWorkspace.listen([this](PHLWINDOW w, PHLWORKSPACE ws) {
        dispatch("window.move_to_workspace", 2, [&](lua_State* L) {
            CLuaWindow::push(L, w);
            CLuaWorkspace::push(L, ws);
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
    m_listeners.push_back(bus()->m_events.workspace.created.listen([this](PHLWORKSPACEREF wsRef) {
        const auto ws = wsRef.lock();
        if (!ws)
            return;
        dispatch("workspace.created", 1, [&](lua_State* L) { CLuaWorkspace::push(L, ws); });
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
    m_listeners.push_back(bus()->m_events.exit.listen([this]() { dispatch("hyprland.shutdown", 0, [](lua_State* L) {}); }));

    m_listeners.push_back(bus()->m_events.pluginEventAdded.listen([this](SP<Event::CEventBus::CCustomEvent> event) {
        auto ret = addCustomEvent(event);
        if (!ret)
            Log::logger->log(Log::ERR, "failed to register plugin event for lua {}: {}", event->m_name, ret.error());
    }));
    m_listeners.push_back(bus()->m_events.pluginEventRemoved.listen([this](const std::string& name) {
        auto ret = removeCustomEvent(name);
        if (!ret)
            Log::logger->log(Log::ERR, "failed to unregister plugin event for lua {}: {}", name, ret.error());
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

std::expected<void, std::string> CLuaEventHandler::addCustomEvent(SP<Event::CEventBus::CCustomEvent> event) {
    using namespace Event;

    auto listener = event->m_event.listen([this, event](const std::vector<CEventBus::CCustomEvent::ValidVariant>& args) {
        dispatch(event->m_name, event->m_argTypes.size(), [args](lua_State* L) {
            for (const auto& arg : args) {
                switch (arg.index()) {
                    case CEventBus::CCustomEvent::TYPE_BOOL: lua_pushboolean(L, std::get<bool>(arg)); break;
                    case CEventBus::CCustomEvent::TYPE_INT: lua_pushinteger(L, std::get<int>(arg)); break;
                    case CEventBus::CCustomEvent::TYPE_DOUBLE: lua_pushnumber(L, std::get<double>(arg)); break;
                    case CEventBus::CCustomEvent::TYPE_STRING: lua_pushstring(L, std::get<std::string>(arg).c_str()); break;
                    case CEventBus::CCustomEvent::TYPE_WINDOW: CLuaWindow::push(L, std::get<PHLWINDOWREF>(arg)); break;
                    case CEventBus::CCustomEvent::TYPE_WORKSPACE: CLuaWorkspace::push(L, std::get<PHLWORKSPACEREF>(arg)); break;
                    case CEventBus::CCustomEvent::TYPE_LAYER_SURFACE: CLuaLayerSurface::push(L, std::get<PHLLSREF>(arg)); break;
                    case CEventBus::CCustomEvent::TYPE_MONITOR: CLuaMonitor::push(L, std::get<PHLMONITORREF>(arg)); break;
                }
            }
        });
    });
    if (!m_pluginListeners.try_emplace(event->m_name, listener).second)
        return std::unexpected("event already exists.");

    return {};
}

std::expected<void, std::string> CLuaEventHandler::removeCustomEvent(const std::string& name) {
    if (!m_pluginListeners.erase(name))
        return std::unexpected("event not found.");
    return {};
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
        "window.update_rules",
        "window.move_to_workspace",
        "layer.opened",
        "layer.closed",
        "monitor.added",
        "monitor.removed",
        "monitor.focused",
        "monitor.layout_changed",
        "workspace.active",
        "workspace.special_active",
        "workspace.created",
        "workspace.removed",
        "workspace.move_to_monitor",
        "config.reloaded",
        "config.props_refreshed",
        "keybinds.submap",
        "screenshare.state",
        "hyprland.start",
        "hyprland.shutdown",
        "input.keyboard.key",
    };
    for (auto& kv : Event::bus()->m_events.plugin)
        EVENTS.emplace(kv.first);
    return EVENTS;
}
