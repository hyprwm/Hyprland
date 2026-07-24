#include "LuaBindingsInternal.hpp"
#include "Check.hpp"

#include "../objects/LuaEventSubscription.hpp"
#include "../objects/LuaKeybind.hpp"
#include "../objects/LuaTimer.hpp"

#include "../../supplementary/executor/Executor.hpp"

#include "../../../devices/IKeyboard.hpp"
#include "../../../managers/eventLoop/EventLoopManager.hpp"
#include "../../../managers/SessionLockManager.hpp"
#include "../../../plugins/PluginSystem.hpp"
#include "keybinds/Manager.hpp"
#include "keybinds/Resolver.hpp"

#include <hyprutils/string/Numeric.hpp>
#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList.hpp>

using namespace Config;
using namespace Config::Lua;
using namespace Config::Lua::Bindings;
using namespace Hyprutils::String;

extern "C" {
#include <lua.h>
#include <xkbcommon/xkbcommon.h>
}

static std::expected<std::vector<std::string>, std::string> parseKeyString(std::string_view value) {
    CVarList2                list(value, 0, '+', true);
    std::vector<std::string> keys;
    keys.reserve(list.size());

    for (const auto& entry : list) {
        auto key = Hyprutils::String::trim(entry);
        if (key.empty())
            return std::unexpected("Empty key in key list");

        keys.emplace_back(std::move(key));
    }

    if (keys.empty())
        return std::unexpected("A bind requires a key");

    return keys;
}

class CLuaBindRef {
  public:
    CLuaBindRef(SP<SLuaStateLifetime> lifetime, int ref) : m_lifetime(std::move(lifetime)), m_ref(ref) {
        ;
    }

    ~CLuaBindRef() {
        if (m_lifetime && m_lifetime->state && m_ref != LUA_NOREF && m_ref != LUA_REFNIL)
            luaL_unref(m_lifetime->state, LUA_REGISTRYINDEX, m_ref);
    }

    int ref() const {
        return m_ref;
    }

  private:
    SP<SLuaStateLifetime> m_lifetime;
    int                   m_ref = LUA_NOREF;
};

static int hlBind(lua_State* L) {
    auto* mgr = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

    auto  str = Check::string(L, 1);
    if (!str)
        return Internal::configError(L, std::format("bind: bad argument 1: {}", str.error()));

    const std::string_view DISPLAY_KEYS = *str;
    auto                   keys         = parseKeyString(DISPLAY_KEYS);
    if (!keys)
        return Internal::configError(L, std::format("hl.bind: failed to parse key string: {}", keys.error()));

    if (!Internal::pushDispatcherFunction(L, 2))
        return Internal::configError(L, "hl.bind: dispatcher must be a dispatcher (e.g. hl.dsp.window.close()) or a lua function");

    if (DISPLAY_KEYS == "catchall" && mgr->m_currentSubmap.empty())
        return Internal::configError(L, "hl.bind: catchall keybinds are only allowed in submaps.");

    const auto               LUA_LIFETIME = mgr->luaStateLifetime();
    const auto               LUA_MANAGER  = Config::Lua::mgr();
    const auto               LUA_REF      = makeShared<CLuaBindRef>(LUA_LIFETIME, luaL_ref(L, LUA_REGISTRYINDEX));

    Keybinds::BindFlags      flags = 0;
    Keybinds::SExtraBindArgs args{
        .metadata =
            {
                .displayKey  = std::string{DISPLAY_KEYS},
                .handler     = "__lua",
                .argument    = std::to_string(LUA_REF->ref()),
                .submap      = mgr->m_currentSubmap,
                .submapReset = mgr->m_currentSubmapReset,
            },
    };

    int optsIdx = 3;

    if (lua_istable(L, optsIdx)) {
        auto getBool = [&](const char* field) -> bool {
            lua_getfield(L, optsIdx, field);
            const bool v = lua_toboolean(L, -1);
            lua_pop(L, 1);
            return v;
        };

        auto readOptString = [&](const char* field) -> std::optional<std::string> {
            lua_getfield(L, optsIdx, field);

            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                return std::nullopt;
            }

            if (!lua_isstring(L, -1)) {
                lua_pop(L, 1);
                Internal::configError(L, "hl.bind: opts.{} must be a string", field);
                return std::nullopt;
            }

            std::string result = lua_tostring(L, -1);
            lua_pop(L, 1);
            return result;
        };

        const bool REPEATING      = getBool("repeating");
        const bool LOCKED         = getBool("locked");
        const bool RELEASE        = getBool("release");
        const bool NON_CONSUMING  = getBool("non_consuming");
        const bool AUTO_CONSUMING = getBool("auto_consuming");
        const bool TRANSPARENT    = getBool("transparent");
        const bool IGNORE_MODS    = getBool("ignore_mods");
        const bool DONT_INHIBIT   = getBool("dont_inhibit");
        const bool LONG_PRESS     = getBool("long_press");
        const bool UNIVERSAL      = getBool("submap_universal");
        const bool MOUSE          = getBool("mouse");

        flags |= REPEATING ? Keybinds::BIND_FLAG_REPEAT : 0;
        flags |= LOCKED ? Keybinds::BIND_FLAG_LOCKED : 0;
        flags |= RELEASE ? Keybinds::BIND_FLAG_RELEASE : 0;
        flags |= NON_CONSUMING ? Keybinds::BIND_FLAG_NON_CONSUMING : 0;
        flags |= AUTO_CONSUMING ? Keybinds::BIND_FLAG_AUTO_CONSUMING : 0;
        flags |= TRANSPARENT ? Keybinds::BIND_FLAG_TRANSPARENT : 0;
        flags |= IGNORE_MODS ? Keybinds::BIND_FLAG_IGNORE_MODS : 0;
        flags |= DONT_INHIBIT ? Keybinds::BIND_FLAG_DONT_INHIBIT : 0;
        flags |= LONG_PRESS ? Keybinds::BIND_FLAG_LONG_PRESS : 0;
        flags |= UNIVERSAL ? Keybinds::BIND_FLAG_SUBMAP_UNIVERSAL : 0;
        flags |= MOUSE ? Keybinds::BIND_FLAG_MOUSE : 0;

        if (auto description = readOptString("description"); description.has_value()) {
            args.metadata.description = std::move(*description);
        } else if (auto desc = readOptString("desc"); desc.has_value()) {
            args.metadata.description = std::move(*desc);
        }

        bool click = false;
        bool drag  = false;

        if (getBool("click")) {
            click = true;
            flags |= Keybinds::BIND_FLAG_RELEASE;
        }

        if (getBool("drag")) {
            drag = true;
            flags |= Keybinds::BIND_FLAG_RELEASE;
        }

        if (click && drag)
            return Internal::configError(L, "hl.bind: click and drag are exclusive");

        if ((LONG_PRESS || (flags & Keybinds::BIND_FLAG_RELEASE)) && REPEATING)
            return Internal::configError(L, "hl.bind: long_press / release is incompatible with repeat");

        if (MOUSE && (REPEATING || (flags & Keybinds::BIND_FLAG_RELEASE) || LOCKED))
            return Internal::configError(L, "hl.bind: mouse is exclusive");

        flags |= click ? Keybinds::BIND_FLAG_CLICK : 0;
        flags |= drag ? Keybinds::BIND_FLAG_DRAG : 0;

        lua_getfield(L, optsIdx, "device");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "inclusive");
            const bool INCLUSIVE = lua_isnil(L, -1) ? true : lua_toboolean(L, -1);
            flags |= INCLUSIVE ? Keybinds::BIND_FLAG_DEVICE_INCLUSIVE : 0;
            lua_pop(L, 1);

            lua_getfield(L, -1, "list");
            if (lua_istable(L, -1)) {
                lua_pushnil(L);
                while (lua_next(L, -2)) {
                    if (lua_isstring(L, -1))
                        args.devices.emplace(lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);
        }
        flags |= getBool("allow_input_capture") ? Keybinds::BIND_FLAG_ALLOW_INPUT_CAPTURE : 0;
        lua_pop(L, 1);
    }

    if (DISPLAY_KEYS == "catchall")
        flags |= Keybinds::BIND_FLAG_CATCH_ALL;

    auto bind = Keybinds::CBind::make(
        std::move(*keys), flags,
        [LUA_LIFETIME, mgr = LUA_MANAGER, LUA_REF] {
            if (!mgr || !LUA_LIFETIME || !LUA_LIFETIME->state)
                return Keybinds::SBindResult{.success = false, .error = "Lua keybind belongs to an expired config state"};

            const auto result = mgr->callLuaFnBind(LUA_REF->ref());
            return Keybinds::SBindResult{
                .passEvent = result.passEvent,
                .success   = result.success,
                .error     = result.error,
                .followUp  = result.requestRelease ? Keybinds::BIND_FOLLOW_UP_TRIGGER_RELEASE : Keybinds::BIND_FOLLOW_UP_NONE,
            };
        },
        std::move(args));
    if (!bind) {
        return Internal::configError(L, std::format("hl.bind: failed to create bind: {}", bind.error()));
    }

    const auto BIND = Keybinds::mgr()->addBind(std::move(*bind));
    Objects::CLuaKeybind::push(L, BIND);
    return 1;
}

static int hlDefineSubmap(lua_State* L) {
    auto* mgr  = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));
    auto  name = Check::string(L, 1);
    if (!name)
        return Internal::configError(L, std::format("define_submap: bad argument 1: {}", name.error()));

    std::string reset;
    int         fnIdx = 2;
    if (lua_gettop(L) >= 3 && lua_isstring(L, 2)) {
        reset = lua_tostring(L, 2);
        fnIdx = 3;
    }

    luaL_checktype(L, fnIdx, LUA_TFUNCTION);

    std::string prev          = mgr->m_currentSubmap;
    std::string prevReset     = mgr->m_currentSubmapReset;
    mgr->m_currentSubmap      = *name;
    mgr->m_currentSubmapReset = reset;

    lua_pushvalue(L, fnIdx);
    if (mgr->guardedPCall(0, 0, 0, CConfigManager::LUA_TIMEOUT_DISPATCH_MS, std::format("hl.define_submap(\"{}\")", *name)) != LUA_OK) {
        mgr->addError(std::format("hl.define_submap: error in submap \"{}\": {}", *name, lua_tostring(L, -1)));
        lua_pop(L, 1);
    }

    mgr->m_currentSubmap      = prev;
    mgr->m_currentSubmapReset = prevReset;
    return 0;
}

static int hlVersion(lua_State* L) {
    lua_pushstring(L, HYPRLAND_VERSION);
    return 1;
}

static int hlGetPlugins(lua_State* L) {
    if (!g_pPluginSystem) {
        lua_newtable(L);
        return 1;
    }

    const auto PLUGINS = g_pPluginSystem->getAllPlugins();

    lua_createtable(L, PLUGINS.size(), 0);

    int i = 1;
    for (const auto& plugin : PLUGINS) {
        lua_createtable(L, 0, 4);
        lua_pushstring(L, plugin->m_name.c_str());
        lua_setfield(L, -2, "name");
        lua_pushstring(L, plugin->m_author.c_str());
        lua_setfield(L, -2, "author");
        lua_pushstring(L, plugin->m_version.c_str());
        lua_setfield(L, -2, "version");
        lua_pushstring(L, plugin->m_description.c_str());
        lua_setfield(L, -2, "description");
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

static int hlExecCmd(lua_State* L) {
    auto cmd = Internal::argStr(L, 1);

    if (cmd.empty())
        return Internal::configError(L, "hl.exec_cmd: expected command as first argument");

    auto rule = Internal::buildRuleFromTable(L, 2);

    if (!rule)
        return rule.error();

    Config::Supplementary::executor()->spawn(Supplementary::SExecRequest{cmd, !*rule, *rule});

    return 0;
}

static int hlClearCrashedLockscreen(lua_State* L) {
    if (!g_pSessionLockManager)
        return Internal::configError(L, "hl.clear_crashed_lockscreen: sessionLockMgr not init'd yet");

    if (!g_pSessionLockManager->isSessionLocked())
        return Internal::configError(L, "hl.clear_crashed_lockscreen: session is not locked");

    if (g_pSessionLockManager->clientLocked() || g_pSessionLockManager->clientDenied())
        return Internal::configError(L, "hl.clear_crashed_lockscreen: session is locked with a client, refusing to unlock");

    g_pSessionLockManager->forceUnlock();

    return 0;
}

static int hlDispatch(lua_State* L) {
    if (!Internal::pushDispatcherFunction(L, 1))
        return Internal::configError(L, "hl.dispatch: expected a dispatcher (e.g. hl.dsp.window.close())");

    int status = LUA_OK;
    if (auto* mgr = CConfigManager::fromLuaState(L); mgr)
        status = mgr->guardedPCall(0, 1, 0, CConfigManager::LUA_TIMEOUT_DISPATCH_MS, "hl.dispatch");
    else
        status = lua_pcall(L, 0, 1, 0);

    if (status != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        lua_pop(L, 1);
        return Internal::dispatcherError(L, std::format("hl.dispatch: {}", err ? err : "unknown error"), Config::Actions::eActionErrorLevel::ERROR,
                                         Config::Actions::eActionErrorCode::LUA_ERROR);
    }

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return Internal::pushSuccessResult(L);
    }

    return 1;
}

static int hlOn(lua_State* L) {
    auto* mgr    = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));
    auto  evName = Check::string(L, 1);
    if (!evName)
        return Internal::configError(L, std::format("on: bad argument 1: {}", evName.error()));
    luaL_checktype(L, 2, LUA_TFUNCTION);

    lua_pushvalue(L, 2);
    int        ref = luaL_ref(L, LUA_REGISTRYINDEX);

    const auto handle = mgr->m_eventHandler->registerEvent(*evName, ref);
    if (!handle.has_value()) {
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        const auto& known = CLuaEventHandler::knownEvents();
        std::string list;
        for (const auto& e : known) {
            list += e + ", ";
        }
        list.pop_back();
        list.pop_back();
        return Internal::configError(L, "hl.on: unknown event \"{}\". Known events:{}", *evName, list);
    }

    Objects::CLuaEventSubscription::push(L, mgr->m_eventHandler.get(), *handle);
    return 1;
}

static int hlUnbind(lua_State* L) {
    if (lua_isstring(L, 1) && std::string_view(lua_tostring(L, 1)) == "all" && lua_gettop(L) == 1) {
        Keybinds::mgr()->clearBinds();
        return 0;
    }

    auto str = Check::string(L, 1);
    if (!str)
        return Internal::configError(L, std::format("unbind: bad argument 1: {}", str.error()));
    Keybinds::mgr()->removeBinds(*str);

    return 0;
}

static int hlIsKeyDown(lua_State* L) {
    if (lua_isinteger(L, 1)) {
        // Confirm code is valid
        auto keycode = lua_tointeger(L, 1);
        if (!xkb_keycode_is_legal_x11(keycode) && !xkb_keycode_is_legal_ext(keycode))
            return Internal::configError(L, std::format("hl.is_key_down: invalid keycode {}", keycode));

        // Return whether it's pressed or not
        lua_pushboolean(L, Keybinds::mgr()->inputState().isKeycodeDown(sc<xkb_keycode_t>(keycode)));
        return 1;
    } else if (lua_isstring(L, 1)) {
        // Parse keysym
        auto key = std::string(lua_tostring(L, 1));
        auto sym = xkb_keysym_from_name(key.c_str(), XKB_KEYSYM_NO_FLAGS);
        if (sym == XKB_KEY_NoSymbol) {
            if (key == "Enter")
                return Internal::configError(L, std::format(R"(Unknown keysym: "{}", did you mean "Return"?)", key));

            return Internal::configError(L, std::format("Unknown keysym: \"{}\"", key));
        }

        // Return whether it's pressed or not
        lua_pushboolean(L, Keybinds::mgr()->inputState().isKeysymDown(sym));
        return 1;
    }
    return Internal::configError(L, std::format("hl.is_key_down: bad argument 1: expected integer or string"));
}

static int hlTimer(lua_State* L) {
    auto* mgr = sc<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

    luaL_checktype(L, 1, LUA_TFUNCTION);
    luaL_checktype(L, 2, LUA_TTABLE);

    lua_getfield(L, 2, "timeout");
    if (!lua_isnumber(L, -1))
        return Internal::configError(L, "hl.timer: opts.timeout must be a number (ms)");
    int timeoutMs = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);

    if (timeoutMs <= 0)
        return Internal::configError(L, "hl.timer: opts.timeout must be > 0");

    lua_getfield(L, 2, "type");
    if (!lua_isstring(L, -1))
        return Internal::configError(L, "hl.timer: opts.type must be \"repeat\" or \"oneshot\"");
    std::string type = lua_tostring(L, -1);
    lua_pop(L, 1);

    bool repeat = false;
    if (type == "repeat")
        repeat = true;
    else if (type != "oneshot")
        return Internal::configError(L, "hl.timer: opts.type must be \"repeat\" or \"oneshot\"");

    lua_pushvalue(L, 1);
    int  ref = luaL_ref(L, LUA_REGISTRYINDEX);

    auto timer = makeShared<CEventLoopTimer>(
        std::chrono::milliseconds(timeoutMs),
        [L, ref, repeat, timeoutMs, mgr](SP<CEventLoopTimer> self, void* data) {
            // update repeat already so that if we call set_timeout inside
            // our timer it doesn't get overwritten
            if (repeat)
                self->updateTimeout(std::chrono::milliseconds(timeoutMs));

            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            int status = LUA_OK;
            if (mgr)
                status = mgr->guardedPCall(0, 0, 0, CConfigManager::LUA_TIMEOUT_TIMER_CALLBACK_MS, "hl.timer callback");
            else
                status = lua_pcall(L, 0, 0, 0);

            if (status != LUA_OK) {
                Log::logger->log(Log::ERR, "[Lua] error in timer callback: {}", lua_tostring(L, -1));
                lua_pop(L, 1);
            }

            if (!repeat) {
                const auto HAS = std::ranges::find_if(mgr->m_luaTimers, [&self](const auto& lt) { return lt.timer == self; }) != mgr->m_luaTimers.end();

                // avoid double-unref if this timer triggered a reload
                if (HAS) {
                    luaL_unref(L, LUA_REGISTRYINDEX, ref);
                    std::erase_if(mgr->m_luaTimers, [&self](const auto& lt) { return lt.timer == self; });
                }
            }
        },
        nullptr);

    mgr->m_luaTimers.emplace_back(CConfigManager::SLuaTimer{timer, ref});
    if (g_pEventLoopManager)
        g_pEventLoopManager->addTimer(timer);

    Objects::CLuaTimer::push(L, timer, timeoutMs);
    return 1;
}

static int hlExecuteScheduledRefreshImmediately(lua_State* L) {

    return Supplementary::refresher()->executeScheduledRefreshImmediately();
}

void Internal::registerToplevelBindings(lua_State* L, CConfigManager* mgr) {
    Internal::setMgrFn(L, mgr, "on", hlOn);
    Internal::setMgrFn(L, mgr, "bind", hlBind);
    Internal::setMgrFn(L, mgr, "define_submap", hlDefineSubmap);
    Internal::setMgrFn(L, mgr, "timer", hlTimer);

    Internal::setFn(L, "dispatch", hlDispatch);
    Internal::setFn(L, "version", hlVersion);
    Internal::setFn(L, "get_loaded_plugins", hlGetPlugins);
    Internal::setFn(L, "exec_cmd", hlExecCmd);

    Internal::setFn(L, "clear_crashed_lockscreen", hlClearCrashedLockscreen);

    Internal::setFn(L, "exec_scheduled_prop_refresh_immediately", hlExecuteScheduledRefreshImmediately);

    Internal::setFn(L, "unbind", hlUnbind);

    Internal::setFn(L, "is_key_down", hlIsKeyDown);
}
