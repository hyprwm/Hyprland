#include "ConfigManager.hpp"
#include "LuaBindings.hpp"
#include "DefaultConfig.hpp"
#include "Emergency.hpp"

#include <climits>
#include <algorithm>
#include <cctype>
#include <functional>
#include <fstream>
#include <hyprutils/string/String.hpp>
#include <hyprutils/string/Numeric.hpp>

#include "types/LuaConfigUtils.hpp"
#include "types/LuaConfigBool.hpp"

#include "../values/ConfigValues.hpp"

#include "../supplementary/jeremy/Jeremy.hpp"
#include "../shared/workspace/WorkspaceRuleManager.hpp"
#include "../shared/monitor/MonitorRuleManager.hpp"
#include "../shared/animation/AnimationTree.hpp"
#include "../shared/inotify/ConfigWatcher.hpp"

#include "../../desktop/rule/Engine.hpp"
#include "../../helpers/MiscFunctions.hpp"

#include "../../event/EventBus.hpp"
#include "../../Compositor.hpp"
#include "../../managers/input/InputManager.hpp"
#include "../../managers/KeybindManager.hpp"
#include "../../helpers/Monitor.hpp"
#include "../../layout/space/Space.hpp"
#include "../../layout/supplementary/WorkspaceAlgoMatcher.hpp"
#include "../../render/Renderer.hpp"
#include "../../errorOverlay/Overlay.hpp"
#include "../../xwayland/XWayland.hpp"
#include "../../plugins/PluginSystem.hpp"
#include "../../managers/EventManager.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../managers/input/trackpad/TrackpadGestures.hpp"
#include "../../notification/NotificationOverlay.hpp"
#include "../../render/decorations/CHyprGroupBarDecoration.hpp"

using namespace Config;
using namespace Config::Lua;
using namespace Hyprutils::String;

static uint64_t nextPluginLuaFnID = 0x10000;

//
static bool isValidLuaIdentifier(const std::string& value) {
    if (value.empty())
        return false;

    if (!std::isalpha(value[0]) && value[0] != '_')
        return false;

    return std::ranges::all_of(value, [](const char& c) { return std::isalnum(c) || c == '_'; });
}

static int pluginLuaFunctionDispatcher(lua_State* L) {
    auto* mgr = CConfigManager::fromLuaState(L);
    if (!mgr)
        return luaL_error(L, "hl.plugin: internal error: config manager unavailable");

    if (!lua_isinteger(L, lua_upvalueindex(1)))
        return luaL_error(L, "hl.plugin: internal error: invalid callback id");

    const auto id = sc<uint64_t>(lua_tointeger(L, lua_upvalueindex(1)));
    return mgr->invokePluginLuaFunctionByID(id, L);
}

static void trackRequiredLuaModulePath(lua_State* L, CConfigManager* mgr, const std::string& moduleName) {
    if (!L || !mgr || moduleName.empty())
        return;

    const int stackTop = lua_gettop(L);

    lua_getglobal(L, "package");
    if (!lua_istable(L, -1)) {
        lua_settop(L, stackTop);
        return;
    }

    lua_getfield(L, -1, "searchpath");
    if (!lua_isfunction(L, -1)) {
        lua_settop(L, stackTop);
        return;
    }

    lua_pushstring(L, moduleName.c_str());
    lua_getfield(L, -3, "path");
    if (!lua_isstring(L, -1)) {
        lua_settop(L, stackTop);
        return;
    }

    if (lua_pcall(L, 2, 2, 0) == LUA_OK && lua_isstring(L, -2)) {
        const auto* resolvedPath = lua_tostring(L, -2);
        if (resolvedPath && std::ranges::find(mgr->m_configPaths, resolvedPath) == mgr->m_configPaths.end())
            mgr->m_configPaths.emplace_back(resolvedPath);
    }

    lua_settop(L, stackTop);
}

static int safeLuaRequire(lua_State* L) {
    const int   nargs = lua_gettop(L);

    std::string moduleName;
    if (lua_isstring(L, 1))
        moduleName = lua_tostring(L, 1);

    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);

    const int status = lua_pcall(L, nargs, LUA_MULTRET, 0);
    if (status == LUA_OK)
        return lua_gettop(L);

    std::string err;
    {
        size_t      len = 0;
        const char* str = luaL_tolstring(L, -1, &len);
        if (str)
            err.assign(str, len);
        lua_pop(L, 1);
    }

    if (auto* mgr = CConfigManager::fromLuaState(L); mgr) {
        if (!moduleName.empty()) {
            trackRequiredLuaModulePath(L, mgr, moduleName);
            mgr->addError(std::format("require(\"{}\"): {}", moduleName, err));
        } else
            mgr->addError(std::format("require: {}", err));
    }

    lua_pop(L, 1); // error object

    lua_newtable(L); // fallback module
    const int fallbackIdx = lua_gettop(L);

    if (!moduleName.empty()) {
        lua_getglobal(L, "package");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "loaded");
            if (lua_istable(L, -1)) {
                lua_pushstring(L, moduleName.c_str());
                lua_pushvalue(L, fallbackIdx);
                lua_settable(L, -3);
            }
            lua_pop(L, 1); // loaded
        }
        lua_pop(L, 1); // package
    }

    return 1;
}

WP<CConfigManager> Lua::mgr() {
    auto& mgr = Config::mgr();
    if (!mgr || mgr->type() != CONFIG_LUA)
        return nullptr;

    return dynamicPointerCast<Lua::CConfigManager>(WP<IConfigManager>(mgr));
}

CConfigManager::SDeviceConfig::SDeviceConfig() {
    auto mgr = Lua::mgr();

    for (const auto& valRaw : Values::CONFIG_DEVICE_VALUE_NAMES) {
        auto generic = std::ranges::find_if(Values::CONFIG_VALUES, [&valRaw](const auto& g) { return g->name() == valRaw; });

        RASSERT(generic != Values::CONFIG_VALUES.end(), "Lua: device value has no generic counterpart, what the fuck happened?");

        auto valLua = mgr->luaConfigValueName(valRaw);

        auto valDevice = valLua.contains('.') ? valLua.substr(valLua.find_last_of('.') + 1) : valLua;

        if (values.contains(valDevice))
            continue; // some are duped

        values.emplace(valDevice, fromGenericValue(*generic));
    }

    // emplace the special keybinds one
    values.emplace("keybinds", makeUnique<CLuaConfigBool>(true));
}

CConfigManager::CConfigManager() : m_mainConfigPath(Supplementary::Jeremy::getMainConfigPath()->path) {
    ;
}

CConfigManager* CConfigManager::fromLuaState(lua_State* L) {
    if (!L)
        return nullptr;

    lua_getfield(L, LUA_REGISTRYINDEX, "hl_lua_manager");
    auto* mgr = sc<CConfigManager*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return mgr;
}

void CConfigManager::watchdogHook(lua_State* L, lua_Debug* /*ar*/) {
    auto* mgr = fromLuaState(L);
    if (!mgr || !mgr->m_watchdogActive)
        return;

    if (std::chrono::steady_clock::now() <= mgr->m_watchdogDeadline)
        return;

    const auto& context = mgr->m_watchdogContext;
    if (context.empty())
        luaL_error(L, "[Lua] execution timed out");

    luaL_error(L, "[Lua] execution timed out in %s", context.c_str());
}

int CConfigManager::guardedPCall(int nargs, int nresults, int errfunc, int timeoutMs, std::string_view context) {
    if (!m_lua)
        return LUA_ERRERR;

    const auto                                  prevHook  = lua_gethook(m_lua);
    const int                                   prevMask  = lua_gethookmask(m_lua);
    const int                                   prevCount = lua_gethookcount(m_lua);

    const bool                                  prevWatchdogActive   = m_watchdogActive;
    const std::chrono::steady_clock::time_point prevWatchdogDeadline = m_watchdogDeadline;
    const std::string                           prevWatchdogContext  = m_watchdogContext;

    m_watchdogActive   = timeoutMs > 0;
    m_watchdogDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(timeoutMs, 1));
    m_watchdogContext  = context;

    lua_sethook(m_lua, &CConfigManager::watchdogHook, LUA_MASKCOUNT, LUA_WATCHDOG_INSTRUCTION_INTERVAL);
    const int result = lua_pcall(m_lua, nargs, nresults, errfunc);

    lua_sethook(m_lua, prevHook, prevMask, prevCount);
    m_watchdogActive   = prevWatchdogActive;
    m_watchdogDeadline = prevWatchdogDeadline;
    m_watchdogContext  = prevWatchdogContext;

    return result;
}

eConfigManagerType CConfigManager::type() {
    return CONFIG_LUA;
}

void CConfigManager::registerValue(const char* name, ILuaConfigValue* val) {
    m_configValues.emplace(name, UP<ILuaConfigValue>(val));
}

void CConfigManager::cleanTimers() {
    for (auto& t : m_luaTimers) {
        t.timer->cancel();
        g_pEventLoopManager->removeTimer(t.timer);

        if (m_lua && t.luaRef != LUA_NOREF) {
            luaL_unref(m_lua, LUA_REGISTRYINDEX, t.luaRef);
            t.luaRef = LUA_NOREF;
        }
    }
    m_luaTimers.clear();
}

void CConfigManager::reinitLuaState() {
    // Destroy the event handler first so its luaL_unref calls happen while m_lua is still valid.
    m_eventHandler.reset();

    cleanTimers();
    clearLuaLayoutProviders();

    if (m_lua) {
        lua_close(m_lua);
        m_lua = nullptr;
    }

    m_lua = luaL_newstate();
    luaL_openlibs(m_lua);

    lua_getglobal(m_lua, "debug");
    if (lua_istable(m_lua, -1)) {
        lua_pushnil(m_lua);
        lua_setfield(m_lua, -2, "sethook");
        lua_pushnil(m_lua);
        lua_setfield(m_lua, -2, "gethook");
    }
    lua_pop(m_lua, 1);

    lua_pushlightuserdata(m_lua, this);
    lua_setfield(m_lua, LUA_REGISTRYINDEX, "hl_lua_manager");

    std::filesystem::path configDir = std::filesystem::path(m_mainConfigPath).parent_path();
    const std::string     luaPath   = (configDir / "?.lua").string() + ";" + (configDir / "?/init.lua").string();
    lua_getglobal(m_lua, "package");
    lua_pushstring(m_lua, luaPath.c_str());
    lua_setfield(m_lua, -2, "path");
    lua_pop(m_lua, 1);

    lua_getglobal(m_lua, "require");
    if (lua_isfunction(m_lua, -1)) {
        lua_pushcclosure(m_lua, safeLuaRequire, 1);
        lua_setglobal(m_lua, "require");
    } else
        lua_pop(m_lua, 1);

    Bindings::registerBindings(m_lua, this);

    m_eventHandler = makeUnique<CLuaEventHandler>(m_lua);

    // Hook package.searchers[2] (the Lua file searcher) to track require()'d paths.
    lua_getglobal(m_lua, "package");
    lua_getfield(m_lua, -1, "searchers");
    lua_rawgeti(m_lua, -1, 2); // original file searcher
    lua_pushlightuserdata(m_lua, this);
    lua_pushcclosure(
        m_lua,
        [](lua_State* L) -> int {
            // upvalue 1: original searcher, upvalue 2: CConfigManager*
            lua_pushvalue(L, lua_upvalueindex(1));
            lua_pushvalue(L, 1); // module name
            lua_call(L, 1, 2);   // -> loader?, filename?
            if (lua_isfunction(L, -2) && lua_isstring(L, -1)) {
                auto* self = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(2)));
                self->m_configPaths.emplace_back(lua_tostring(L, -1));
            }
            return 2;
        },
        2);
    lua_rawseti(m_lua, -2, 2); // replace package.searchers[2]
    lua_pop(m_lua, 2);         // pop searchers, package
}

void CConfigManager::init() {
    reinitLuaState();

    for (const auto& v : Values::CONFIG_VALUES) {
        m_configValues.emplace(luaConfigValueName(v->name()), fromGenericValue(v));
    }

    m_configValues["autogenerated"] = fromGenericValue(makeShared<Values::CIntValue>("autogenerated", "whether the config is autogenerated or not", 0));

    Config::watcher()->setOnChange([this](const CConfigWatcher::SConfigWatchEvent& e) {
        Log::logger->log(Log::DEBUG, "[lua] file {} modified, reloading", e.file);
        reload();
    });

    reload();
}

void CConfigManager::reload() {
    Event::bus()->m_events.config.preReload.emit();

    Hyprutils::Utils::CScopeGuard x([this] {
        m_isParsingConfig = false;
        m_isFirstLaunch   = false;
    });
    m_isParsingConfig = true;

    m_mainConfigPath = Supplementary::Jeremy::getMainConfigPath()->path;

    // reset tracked paths; the searcher hook will re-populate them as require() runs
    m_configPaths.clear();
    m_configPaths.emplace_back(m_mainConfigPath);

    // phase 1: check syntax before clearing any state, so a broken syntax
    // doesn't entirely fucking nuke the config and leave the user
    // with no binds.
    //
    // this won't help if they are launching hyprland,
    // which will be handled with emergency pcall
    auto phase1Load = [this]() -> bool {
        // clear package.loaded for user modules so require() re-executes them and
        // the searcher hook can re-track their paths.
        lua_getglobal(m_lua, "package");
        lua_getfield(m_lua, -1, "loaded");
        static constexpr std::string_view STDLIB[] = {"_G", "coroutine", "debug", "io", "math", "os", "package", "string", "table", "utf8", "bit32", "jit"};
        lua_pushnil(m_lua);
        while (lua_next(m_lua, -2)) {
            lua_pop(m_lua, 1); // pop value, keep key
            if (lua_isstring(m_lua, -1)) {
                std::string_view mod  = lua_tostring(m_lua, -1);
                bool             skip = false;
                for (const auto& s : STDLIB) {
                    if (mod == s) {
                        skip = true;
                        break;
                    }
                }
                if (!skip) {
                    lua_pushvalue(m_lua, -1); // dup key
                    lua_pushnil(m_lua);
                    lua_settable(m_lua, -4); // package.loaded[key] = nil
                }
            }
        }
        lua_pop(m_lua, 2); // pop loaded, package

        if (luaL_loadfile(m_lua, m_mainConfigPath.c_str()) != LUA_OK) {
            m_errors.clear();
            addError(lua_tostring(m_lua, -1));
            lua_pop(m_lua, 1);
            return false;
        }

        return true;
    };

    if (!phase1Load()) {
        m_lastConfigVerificationWasSuccessful = false;
        postConfigReload();
        return;
    }

    // phase 2: syntax is valid, reset and load.
    Config::animationTree()->reset();
    Config::workspaceRuleMgr()->clear();
    Config::monitorRuleMgr()->clear();
    Desktop::Rule::ruleEngine()->clearAllRules();
    g_pTrackpadGestures->clearGestures();
    cleanTimers();
    clearLuaLayoutProviders();
    m_luaWindowRules.clear();
    m_luaLayerRules.clear();
    m_errors.clear();
    m_deviceConfigs.clear();
    m_registeredPlugins.clear();
    m_eventHandler->clearEvents();
    clearHeldLuaRefs();

    if (g_pKeybindManager) {
        for (const auto& kb : g_pKeybindManager->m_keybinds) {
            if (kb->handler == "__lua")
                luaL_unref(m_lua, LUA_REGISTRYINDEX, std::stoi(kb->arg));
        }
        g_pKeybindManager->clearKeybinds();
    }

    // refresh lua state so we get rid of any stale state
    reinitLuaState();

    // restart phase 1 load, because our state is gone now.
    if (!phase1Load()) {
        m_lastConfigVerificationWasSuccessful = false;
        postConfigReload();
        return;
    }

    // re-register plugin functions
    reregisterLuaPluginFns();

    for (const auto& v : m_configValues) {
        v.second->reset();
    }

    lua_pushcfunction(m_lua, [](lua_State* L) -> int {
        luaL_traceback(L, L, lua_tostring(L, 1), 1);
        return 1;
    });
    lua_insert(m_lua, 1);

    if (guardedPCall(0, 0, 1, LUA_TIMEOUT_CONFIG_RELOAD_MS, "config reload") != LUA_OK) {
        addError(lua_tostring(m_lua, -1));
        lua_pop(m_lua, 1);
        m_lastConfigVerificationWasSuccessful = false;
    } else
        m_lastConfigVerificationWasSuccessful = m_errors.empty();

    lua_remove(m_lua, 1);

    // collect stale userdata after reload so HL.Notification __gc can
    // promptly release pause leases from dropped references
    lua_gc(m_lua, LUA_GCCOLLECT, 0);

    postConfigReload();
}

void CConfigManager::postConfigReload() {
    static auto PSUPPRESSERRORS = CConfigValue<Config::INTEGER>("debug.suppress_errors");
    static auto PXWAYLAND       = CConfigValue<Config::INTEGER>("xwayland.enabled");
    static auto PMANUALCRASH    = CConfigValue<Config::INTEGER>("debug.manual_crash");
    static auto PENABLESTDOUT   = CConfigValue<Config::INTEGER>("debug.enable_stdout_logs");
    static auto PAUTOGENERATED  = CConfigValue<Config::INTEGER>("autogenerated");

    for (auto const& w : g_pCompositor->m_windows) {
        w->uncacheWindowDecos();
    }

    const bool emergencyModeTripped = !m_errors.empty() && g_pKeybindManager->m_keybinds.empty();

    if (emergencyModeTripped)
        luaL_dostring(m_lua, EMERGENCY_PCALL);

    // parseError will be displayed next frame
    if (!m_errors.empty() && !*PSUPPRESSERRORS) {
        std::string errorStr;

        if (emergencyModeTripped) {
            errorStr = "⚠ Emergency mode tripped: A lua config error resulted in no binds being registered. Emergency binds active: SUPER + Q → any known terminal, SUPER + R "
                       "→ hyprland-run, SUPER + M → Exit\n";
        }

        errorStr += "Your config has errors:\n";

        for (const auto& e : m_errors) {
            errorStr += e + "\n";

            if (std::ranges::count(errorStr, '\n') > 15) {
                errorStr += "... more";
                break;
            }
        }

        if (!errorStr.empty() && errorStr.back() == '\n')
            errorStr.pop_back();

        ErrorOverlay::overlay()->queueCreate(errorStr, ErrorOverlay::Colors::ERROR);
    } else if (*PAUTOGENERATED)
        ErrorOverlay::overlay()->queueCreate(
            "Warning: You're using an autogenerated config! Edit the config file to get rid of this message. (config file: " + getMainConfigPath() +
                " )\nSUPER+Q -> kitty (if it doesn't launch, make sure it's installed or choose a different terminal in the config)\nSUPER+M -> exit Hyprland",
            ErrorOverlay::Colors::WARNING);
    else
        ErrorOverlay::overlay()->destroy();

    // Set the modes for all monitors as we configured them
    // not on first launch because monitors might not exist yet
    // and they'll be taken care of in the newMonitor event
    if (!m_isFirstLaunch) {
        // check
        Config::monitorRuleMgr()->scheduleReload();
        Config::monitorRuleMgr()->ensureMonitorStatus();
        Config::monitorRuleMgr()->ensureVRR();
    }

#ifndef NO_XWAYLAND
    g_pCompositor->m_wantsXwayland = *PXWAYLAND;
    // enable/disable xwayland usage
    if (!m_isFirstLaunch &&
        g_pXWayland /* XWayland has to be initialized by CCompositor::initManagers for this to make sense, and it doesn't have to be (e.g. very early plugin load) */) {
        bool prevEnabledXwayland = g_pXWayland->enabled();
        if (g_pCompositor->m_wantsXwayland != prevEnabledXwayland)
            g_pXWayland = makeUnique<CXWayland>(g_pCompositor->m_wantsXwayland);
    } else
        g_pCompositor->m_wantsXwayland = *PXWAYLAND;
#endif

    if (*PMANUALCRASH && !m_manualCrashInitiated) {
        m_manualCrashInitiated = true;
        Notification::overlay()->addNotification("Manual crash has been set up. Set debug.manual_crash back to 0 in order to crash the compositor.", CHyprColor(0), 5000,
                                                 ICON_INFO);
    } else if (m_manualCrashInitiated && !*PMANUALCRASH) {
        // cowabunga it is
        g_pHyprRenderer->initiateManualCrash();
    }

    auto disableStdout = !*PENABLESTDOUT;
    if (disableStdout && m_isFirstLaunch)
        Log::logger->log(Log::DEBUG, "Disabling stdout logs! Check the log for further logs.");

    handlePluginLoads();

    Config::Supplementary::refresher()->scheduleRefresh(Supplementary::REFRESH_ALL);

    Event::bus()->m_events.config.reloaded.emit();
    if (g_pEventManager)
        g_pEventManager->postEvent(SHyprIPCEvent{"configreloaded", ""});
}

void CConfigManager::addError(std::string&& str) {
    if (m_isParsingConfig) {
        m_errors.emplace_back(std::move(str));
        return;
    }

    if (m_isEvaluating) {
        m_errors.emplace_back(std::move(str));
        return;
    }

    // pop a notification
    Notification::overlay()->addNotification(std::format("Runtime error in lua:\n{}", std::move(str)), 0, 5000, ICON_WARNING);
}

void CConfigManager::addEvalIssue(const Config::SConfigError& err) {
    if (!m_isEvaluating)
        return;

    if (err.level == eConfigErrorLevel::WARNING || err.level == eConfigErrorLevel::INFO)
        m_evalIssues.emplace_back(err);
}

std::optional<std::string> CConfigManager::eval(const std::string& code) {
    if (!m_lua)
        return "error: lua state not initialized";

    m_errors.clear();
    m_evalIssues.clear();
    m_isEvaluating = true;

    Hyprutils::Utils::CScopeGuard x([this] { m_isEvaluating = false; });

    if (luaL_loadstring(m_lua, code.c_str()) != LUA_OK) {
        std::string err = lua_tostring(m_lua, -1);
        lua_pop(m_lua, 1);
        return std::format("error: {}", err);
    }

    if (guardedPCall(0, 0, 0, LUA_TIMEOUT_EVAL_MS, "hyprctl eval") != LUA_OK) {
        std::string err = lua_tostring(m_lua, -1);
        lua_pop(m_lua, 1);
        return std::format("error: {}", err);
    }

    if (!m_errors.empty() || !m_evalIssues.empty()) {
        std::string out;
        out.reserve(256);

        for (size_t i = 0; i < m_errors.size(); ++i) {
            out += "error: ";
            out += m_errors.at(i);
            out += "\n";
        }

        for (const auto& issue : m_evalIssues) {
            out += std::format("{}: {}", Config::toString(issue.level), issue.message);
            out += "\n";
        }

        out.pop_back();

        return out;
    }

    m_errors.clear();
    m_evalIssues.clear();

    return std::nullopt;
}

std::string CConfigManager::verify() {
    if (m_errors.empty())
        return "config ok";

    std::string fullStr = "";
    for (const auto& e : m_errors) {
        fullStr += e;
        fullStr += "\n";
    }

    fullStr.pop_back();
    return fullStr;
}

static std::string normalizeDeviceName(const std::string& dev) {
    auto copy = dev;
    std::ranges::replace(copy, ' ', '-');
    return copy;
}

ILuaConfigValue* CConfigManager::findDeviceValue(const std::string& dev, const std::string& field) {
    const auto devIt = m_deviceConfigs.find(dev);
    if (devIt == m_deviceConfigs.end())
        return nullptr;
    const auto valIt = devIt->second.values.find(field);
    return valIt != devIt->second.values.end() ? valIt->second.get() : nullptr;
}

int CConfigManager::getDeviceInt(const std::string& dev, const std::string& field, const std::string& fb) {
    std::string fallback = luaConfigValueName(fb);
    if (auto* v = findDeviceValue(normalizeDeviceName(dev), luaConfigValueName(field)); v && v->setByUser())
        return v->asInt();
    if (!fallback.empty() && m_configValues.contains(fallback))
        return m_configValues.at(fallback)->asInt();
    return 0;
}

float CConfigManager::getDeviceFloat(const std::string& dev, const std::string& field, const std::string& fb) {
    std::string fallback = luaConfigValueName(fb);
    if (auto* v = findDeviceValue(normalizeDeviceName(dev), luaConfigValueName(field)); v && v->setByUser())
        return v->asFloat();
    if (!fallback.empty() && m_configValues.contains(fallback))
        return m_configValues.at(fallback)->asFloat();
    return 0.F;
}

Vector2D CConfigManager::getDeviceVec(const std::string& dev, const std::string& field, const std::string& fb) {
    std::string fallback = luaConfigValueName(fb);
    auto        toVec    = [](const Config::VEC2& v) -> Vector2D { return {v.x, v.y}; };
    if (auto* v = findDeviceValue(normalizeDeviceName(dev), luaConfigValueName(field)); v && v->setByUser())
        return toVec(v->asVec2());
    if (!fallback.empty() && m_configValues.contains(fallback))
        return toVec(m_configValues.at(fallback)->asVec2());
    return {0, 0};
}

std::string CConfigManager::getDeviceString(const std::string& dev, const std::string& field, const std::string& fb) {
    std::string fallback = luaConfigValueName(fb);
    auto        clean    = [](const Config::STRING& s) -> std::string { return s == STRVAL_EMPTY ? "" : s; };
    if (auto* v = findDeviceValue(normalizeDeviceName(dev), luaConfigValueName(field)); v && v->setByUser())
        return clean(v->asString());
    if (!fallback.empty() && m_configValues.contains(fallback))
        return clean(m_configValues.at(fallback)->asString());
    return "";
}

bool CConfigManager::deviceConfigExplicitlySet(const std::string& dev, const std::string& field) {
    auto v = findDeviceValue(normalizeDeviceName(dev), luaConfigValueName(field));
    return v && v->setByUser();
}

bool CConfigManager::deviceConfigExists(const std::string& dev) {
    return m_deviceConfigs.contains(normalizeDeviceName(dev));
}

SConfigOptionReply CConfigManager::getConfigValue(const std::string& s) {

    if (m_configValues.contains(s)) {

        auto& cv = m_configValues[s];

        m_configPtrMap[s] = cv->data();

        return SConfigOptionReply{.dataptr = cc<void* const*>(&m_configPtrMap.at(s)), .type = cv->underlying(), .setByUser = cv->setByUser()};
    }

    // try replacing all . with : unless col.
    std::string s2 = s;

    for (size_t i = 0; i < s2.length(); ++i) {
        if (s2[i] != ':')
            continue;

        if (i <= 3) {
            s2[i] = '.';
            continue;
        }

        if (s2[i - 1] == 'l' && s2[i - 2] == 'o' && s2[i - 3] == 'c')
            continue;

        s2[i] = '.';
    }

    if (!m_configValues.contains(s2))
        return SConfigOptionReply{.dataptr = nullptr};

    auto& cv = m_configValues[s2];

    m_configPtrMap[s2] = cv->data();

    return SConfigOptionReply{.dataptr = cc<void* const*>(&m_configPtrMap.at(s2)), .type = cv->underlying(), .setByUser = cv->setByUser()};
}

std::string CConfigManager::getMainConfigPath() {
    return m_mainConfigPath;
}

std::string CConfigManager::getErrors() {
    std::string errStr;
    for (const auto& e : m_errors) {
        errStr += e + "\n";
    }

    if (!errStr.empty())
        errStr.pop_back();

    return errStr;
}

std::string CConfigManager::getConfigString() {
    return "Not supported under lua";
}

std::string CConfigManager::currentConfigPath() {
    return m_mainConfigPath;
}

const std::vector<std::string>& CConfigManager::getConfigPaths() {
    return m_configPaths;
}

std::expected<void, std::string> CConfigManager::generateDefaultConfig(const std::filesystem::path& path, bool safeMode) {
    std::string parentPath = std::filesystem::path(path).parent_path();

    if (!parentPath.empty()) {
        std::error_code ec;
        bool            created = std::filesystem::create_directories(parentPath, ec);
        if (ec) {
            Log::logger->log(Log::ERR, "Couldn't create config home directory ({}): {}", ec.message(), parentPath);
            return std::unexpected("Config could not be generated.");
        }
        if (created)
            Log::logger->log(Log::WARN, "Creating config home directory");
    }

    Log::logger->log(Log::WARN, "No config file found; attempting to generate.");
    std::ofstream ofs;
    ofs.open(path, std::ios::trunc);

    if (!ofs.good())
        return std::unexpected("Config could not be generated.");

    if (!safeMode) {
        ofs << AUTOGENERATED_PREFIX_LUA;
        ofs << EXAMPLE_CONFIG_LUA;
    } else {
        std::string n = std::string{EXAMPLE_CONFIG_LUA};
        replaceInString(n, "\nlocal menu        = \"hyprlauncher\"\n", "\nlocal menu        = \"hyprland-run\"\n");
        ofs << n;
    }

    ofs.close();

    if (ofs.fail())
        return std::unexpected("Config could not be generated.");

    return {};
}

void CConfigManager::handlePluginLoads() {
    if (!g_pPluginSystem)
        return;

    bool pluginsChanged = false;
    g_pPluginSystem->updateConfigPlugins(m_registeredPlugins, pluginsChanged);

    if (pluginsChanged) {
        ErrorOverlay::overlay()->destroy();
        reload();
    }
}

bool CConfigManager::configVerifPassed() {
    return m_lastConfigVerificationWasSuccessful;
}

std::string CConfigManager::luaConfigValueName(const std::string& s) {
    std::string cpy = s;
    std::ranges::replace(cpy, ':', '.');
    std::ranges::replace(cpy, '-', '_');
    return cpy;
}

bool CConfigManager::isFirstLaunch() const {
    return m_isFirstLaunch;
}

std::expected<void, std::string> CConfigManager::registerPluginValue(void* handle, SP<Config::Values::IValue> value) {

    const auto NAME = luaConfigValueName(value->name());

    if (m_configValues.contains(NAME))
        return std::unexpected("name collision: already registered");

    auto val = fromGenericValue(value);

    if (!val)
        return std::unexpected("unsupported value type");

    m_configValues.emplace(NAME, std::move(val));

    m_pluginValues[handle].emplace_back(NAME);

    return {};
}

std::expected<void, std::string> CConfigManager::registerPluginLuaFunctionInState(uint64_t id, const std::string& namespace_, const std::string& name) {
    if (!m_lua)
        return std::unexpected("lua state not initialized");

    lua_getglobal(m_lua, "hl");
    if (!lua_istable(m_lua, -1)) {
        lua_pop(m_lua, 1);
        return std::unexpected("missing global table 'hl'");
    }

    lua_getfield(m_lua, -1, "plugin");
    if (!lua_istable(m_lua, -1)) {
        lua_pop(m_lua, 2);
        return std::unexpected("missing global table 'hl.plugin'");
    }

    const int pluginTableIdx = lua_gettop(m_lua);

    lua_getfield(m_lua, pluginTableIdx, namespace_.c_str());
    if (lua_isnil(m_lua, -1)) {
        lua_pop(m_lua, 1);
        lua_newtable(m_lua);
        lua_pushvalue(m_lua, -1);
        lua_setfield(m_lua, pluginTableIdx, namespace_.c_str());
    } else if (!lua_istable(m_lua, -1)) {
        lua_pop(m_lua, 3);
        return std::unexpected(std::format("hl.plugin.{} already exists and is not a namespace table", namespace_));
    }

    const int namespaceTableIdx = lua_gettop(m_lua);

    lua_getfield(m_lua, namespaceTableIdx, name.c_str());
    const bool exists = !lua_isnil(m_lua, -1);
    lua_pop(m_lua, 1);

    if (exists) {
        lua_pop(m_lua, 3);
        return std::unexpected(std::format("hl.plugin.{}.{} already exists", namespace_, name));
    }

    lua_pushinteger(m_lua, sc<lua_Integer>(id));
    lua_pushcclosure(m_lua, pluginLuaFunctionDispatcher, 1);
    lua_setfield(m_lua, namespaceTableIdx, name.c_str());

    lua_pop(m_lua, 3);
    return {};
}

std::expected<void, std::string> CConfigManager::unregisterPluginLuaFunctionInState(const std::string& namespace_, const std::string& name) {
    if (!m_lua)
        return std::unexpected("lua state not initialized");

    lua_getglobal(m_lua, "hl");
    if (!lua_istable(m_lua, -1)) {
        lua_pop(m_lua, 1);
        return {};
    }

    lua_getfield(m_lua, -1, "plugin");
    if (!lua_istable(m_lua, -1)) {
        lua_pop(m_lua, 2);
        return {};
    }

    const int pluginTableIdx = lua_gettop(m_lua);

    lua_getfield(m_lua, pluginTableIdx, namespace_.c_str());
    if (!lua_istable(m_lua, -1)) {
        lua_pop(m_lua, 3);
        return {};
    }

    const int namespaceTableIdx = lua_gettop(m_lua);

    lua_pushnil(m_lua);
    lua_setfield(m_lua, namespaceTableIdx, name.c_str());

    bool isEmpty = true;
    lua_pushnil(m_lua);
    if (lua_next(m_lua, namespaceTableIdx) != 0) {
        isEmpty = false;
        lua_pop(m_lua, 2);
    }

    if (isEmpty) {
        lua_pushnil(m_lua);
        lua_setfield(m_lua, pluginTableIdx, namespace_.c_str());
    }

    lua_pop(m_lua, 3);
    return {};
}

void CConfigManager::erasePluginLuaFunction(uint64_t id) {
    std::erase_if(m_pluginLuaFunctions, [&id](const SPluginLuaFunction& f) { return f.id == id; });
}

int CConfigManager::invokePluginLuaFunctionByID(uint64_t id, lua_State* L) {
    const auto REGIT = std::ranges::find_if(m_pluginLuaFunctions, [&id](const SPluginLuaFunction& r) { return r.id == id; });
    if (REGIT == m_pluginLuaFunctions.end())
        return luaL_error(L, "hl.plugin: this function is no longer available (plugin unloaded)");

    const auto FN = REGIT->fn;
    if (!FN)
        return luaL_error(L, "hl.plugin: this function is not callable");

    return FN(L);
}

std::expected<void, std::string> CConfigManager::registerPluginLuaFunction(void* handle, const std::string& namespace_, const std::string& name, Config::PLUGIN_LUA_FN fn) {
    if (!handle)
        return std::unexpected("invalid handle");

    if (!fn)
        return std::unexpected("function pointer cannot be null");

    if (!isValidLuaIdentifier(namespace_))
        return std::unexpected("namespace must match [A-Za-z_][A-Za-z0-9_]*");

    if (!isValidLuaIdentifier(name))
        return std::unexpected("name must match [A-Za-z_][A-Za-z0-9_]*");

    if (namespace_ == "load")
        return std::unexpected("namespace 'load' is reserved");

    const auto key = namespace_ + "." + name;
    if (std::ranges::find_if(m_pluginLuaFunctions, [&key](const SPluginLuaFunction& r) { return r.namespace_ + "." + r.name == key; }) != m_pluginLuaFunctions.end())
        return std::unexpected("name collision: already registered");

    const uint64_t id = nextPluginLuaFnID++;
    if (const auto REGISTERED = registerPluginLuaFunctionInState(id, namespace_, name); !REGISTERED)
        return REGISTERED;

    m_pluginLuaFunctions.emplace_back(SPluginLuaFunction{.id = id, .handle = handle, .namespace_ = namespace_, .name = name, .fn = fn});

    return {};
}

std::expected<void, std::string> CConfigManager::unregisterPluginLuaFunction(void* handle, const std::string& namespace_, const std::string& name) {
    if (!handle)
        return std::unexpected("invalid handle");

    const auto key = namespace_ + "." + name;
    auto       it  = std::ranges::find_if(m_pluginLuaFunctions, [&key](const SPluginLuaFunction& r) { return r.namespace_ + "." + r.name == key; });

    if (it == m_pluginLuaFunctions.end())
        return std::unexpected("no such function");

    if (it->handle != handle)
        return std::unexpected("function belongs to a different plugin");

    const auto removedFromState = unregisterPluginLuaFunctionInState(namespace_, name);
    erasePluginLuaFunction(it->id);

    if (!removedFromState)
        return removedFromState;

    return {};
}

void CConfigManager::onPluginUnload(void* handle) {
    if (!handle)
        return;

    if (const auto it = m_pluginValues.find(handle); it != m_pluginValues.end()) {
        for (const auto& name : it->second) {
            m_configValues.erase(name);
        }

        m_pluginValues.erase(it);
    }

    std::erase_if(m_pluginLuaFunctions, [&handle, this](const SPluginLuaFunction& f) {
        const bool NEEDS_REMOVE = f.handle == handle;

        if (NEEDS_REMOVE) // NOLINTNEXTLINE
            unregisterPluginLuaFunctionInState(f.namespace_, f.name);

        return NEEDS_REMOVE;
    });
}

void CConfigManager::registerLuaRef(int ref) {
    m_heldLuaRefs.emplace_back(ref);
}

void CConfigManager::callLuaFn(int ref) {
    lua_rawgeti(m_lua, LUA_REGISTRYINDEX, ref);

    int status = guardedPCall(0, 0, 0, CConfigManager::LUA_TIMEOUT_KEYBIND_CALLBACK_MS, "keybind callback");

    if (status != LUA_OK) {
        addError(std::format("error in callLuaFn: {}", lua_tostring(m_lua, -1)));
        lua_pop(m_lua, 1);
    }
}

void CConfigManager::clearHeldLuaRefs() {
    for (const auto& r : m_heldLuaRefs) {
        luaL_unref(m_lua, LUA_REGISTRYINDEX, r);
    }

    m_heldLuaRefs.clear();
}

bool CConfigManager::isDynamicParse() const {
    return !m_isParsingConfig || m_isEvaluating;
}

void CConfigManager::reregisterLuaPluginFns() {
    for (auto& fn : m_pluginLuaFunctions) {
        auto ret = registerPluginLuaFunctionInState(fn.id, fn.namespace_, fn.name);
        if (!ret)
            Log::logger->log(Log::ERR, "[lua] failed to reregister plugin fn for {}.{}: {}", fn.namespace_, fn.name, ret.error());
    }
}
