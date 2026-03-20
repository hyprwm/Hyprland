#include "LuaBindings.hpp"
#include "ConfigManager.hpp"
#include "LuaEventHandler.hpp"
#include "devices/IKeyboard.hpp"
#include "objects/LuaWindow.hpp"
#include "objects/LuaWorkspace.hpp"
#include "objects/LuaMonitor.hpp"
#include "objects/LuaLayerSurface.hpp"
#include "types/LuaConfigFloat.hpp"
#include "types/LuaConfigInt.hpp"
#include "types/LuaConfigString.hpp"
#include "types/LuaConfigBool.hpp"

#include "../../Compositor.hpp"
#include "../../helpers/Monitor.hpp"
#include "../../managers/KeybindManager.hpp"
#include "../../managers/animation/AnimationManager.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../managers/input/trackpad/TrackpadGestures.hpp"
#include "../../managers/input/trackpad/gestures/DispatcherGesture.hpp"
#include "../../managers/input/trackpad/gestures/WorkspaceSwipeGesture.hpp"
#include "../../managers/input/trackpad/gestures/ResizeGesture.hpp"
#include "../../managers/input/trackpad/gestures/MoveGesture.hpp"
#include "../../managers/input/trackpad/gestures/SpecialWorkspaceGesture.hpp"
#include "../../managers/input/trackpad/gestures/CloseGesture.hpp"
#include "../../managers/input/trackpad/gestures/FloatGesture.hpp"
#include "../../managers/input/trackpad/gestures/FullscreenGesture.hpp"
#include "../../managers/input/trackpad/gestures/CursorZoomGesture.hpp"
#include "../supplementary/executor/Executor.hpp"
#include "../shared/animation/AnimationTree.hpp"
#include "../shared/actions/ConfigActions.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../helpers/MiscFunctions.hpp"
#include "../../managers/SeatManager.hpp"

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList.hpp>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Config::Lua::Bindings;
using namespace Config::Lua;
using namespace Hyprutils::String;

namespace CA = Config::Actions;

// converts a Lua string-or-number at stack position idx to std::string.
static std::string argStr(lua_State* L, int idx) {
    if (lua_type(L, idx) == LUA_TNUMBER)
        return std::to_string((long long)lua_tonumber(L, idx));
    size_t      n = 0;
    const char* s = luaL_checklstring(L, idx, &n);
    return {s, n};
}

// -- Table helpers --------------------------------------------------------------

static std::optional<std::string> tableOptStr(lua_State* L, int idx, const char* field) {
    lua_getfield(L, idx, field);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return std::nullopt;
    }
    const char* s = lua_tostring(L, -1);
    lua_pop(L, 1);
    return s ? std::optional(std::string(s)) : std::nullopt;
}

static std::optional<double> tableOptNum(lua_State* L, int idx, const char* field) {
    lua_getfield(L, idx, field);
    if (lua_isnil(L, -1) || !lua_isnumber(L, -1)) {
        lua_pop(L, 1);
        return std::nullopt;
    }
    double v = lua_tonumber(L, -1);
    lua_pop(L, 1);
    return v;
}

static std::optional<bool> tableOptBool(lua_State* L, int idx, const char* field) {
    lua_getfield(L, idx, field);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return std::nullopt;
    }
    bool v = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return v;
}

// -- Parsing helpers ------------------------------------------------------------

static Math::eDirection parseDirectionStr(const std::string& str) {
    if (str == "left" || str == "l")
        return Math::DIRECTION_LEFT;
    if (str == "right" || str == "r")
        return Math::DIRECTION_RIGHT;
    if (str == "up" || str == "u" || str == "t")
        return Math::DIRECTION_UP;
    if (str == "down" || str == "d" || str == "b")
        return Math::DIRECTION_DOWN;
    return Math::DIRECTION_DEFAULT;
}

static CA::eTogglableAction parseToggleStr(const std::string& str) {
    if (str.empty() || str == "toggle")
        return CA::TOGGLE_ACTION_TOGGLE;
    if (str == "enable" || str == "on")
        return CA::TOGGLE_ACTION_ENABLE;
    if (str == "disable" || str == "off")
        return CA::TOGGLE_ACTION_DISABLE;
    return CA::TOGGLE_ACTION_TOGGLE;
}

// -- Forward declarations for dispatch closures used across sections -----------
static int dsp_moveToWorkspace(lua_State* L);

// -- Dispatch-time resolution helpers -------------------------------------------

static std::optional<PHLWINDOW> windowFromUpval(lua_State* L, int idx) {
    if (lua_isnil(L, lua_upvalueindex(idx)))
        return std::nullopt;
    return g_pCompositor->getWindowByRegex(lua_tostring(L, lua_upvalueindex(idx)));
}

static void pushWindowUpval(lua_State* L, int tableIdx) {
    if (lua_istable(L, tableIdx)) {
        lua_getfield(L, tableIdx, "window");
        if (!lua_isstring(L, -1)) {
            lua_pop(L, 1);
            lua_pushnil(L);
        }
    } else
        lua_pushnil(L);
}

static void checkResult(lua_State* L, const CA::ActionResult& r) {
    if (!r)
        luaL_error(L, "%s", r.error().c_str());
}

static PHLWORKSPACE resolveWorkspaceStr(const std::string& args) {
    const auto& [id, name, isAutoID] = getWorkspaceIDNameFromString(args);
    if (id == WORKSPACE_INVALID)
        return nullptr;
    auto ws = g_pCompositor->getWorkspaceByID(id);
    if (!ws) {
        const auto PMONITOR = Desktop::focusState()->monitor();
        if (PMONITOR)
            ws = g_pCompositor->createNewWorkspace(id, PMONITOR->m_id, name, false);
    }
    return ws;
}

static std::optional<eKeyboardModifiers> modFromSv(std::string_view sv) {
    if (sv == "SHIFT")
        return HL_MODIFIER_SHIFT;
    if (sv == "CAPS")
        return HL_MODIFIER_CAPS;
    if (sv == "CTRL" || sv == "CONTROL")
        return HL_MODIFIER_CTRL;
    if (sv == "ALT" || sv == "MOD1")
        return HL_MODIFIER_ALT;
    if (sv == "MOD2")
        return HL_MODIFIER_MOD2;
    if (sv == "MOD3")
        return HL_MODIFIER_MOD3;
    if (sv == "SUPER" || sv == "WIN" || sv == "LOGO" || sv == "MOD4" || sv == "META")
        return HL_MODIFIER_META;
    if (sv == "MOD5")
        return HL_MODIFIER_MOD5;

    return std::nullopt;
}

static bool isSymSpecial(std::string_view sv) {
    if (sv == "mouse_down" || sv == "mouse_up" || sv == "mouse_left" || sv == "mouse_right")
        return true;

    return sv.starts_with("switch:") || sv.starts_with("mouse:");
}

// tries to parse a key string
static std::expected<void, std::string> parseKeyString(SKeybind& kb, std::string_view sv) {
    bool                      modsEnded = false, specialSym = false;

    CVarList2                 vl(sv, 0, '+', true);

    uint32_t                  modMask = 0;
    std::vector<xkb_keysym_t> keysyms;

    for (const auto& a : vl) {
        auto arg = Hyprutils::String::trim(a);

        auto mask = modFromSv(arg);

        if (!mask)
            modsEnded = true;

        if (modsEnded && mask)
            return std::unexpected("Modifiers must come first in the list");

        if (mask) {
            modMask |= *mask;
            continue;
        }

        if (specialSym)
            return std::unexpected("Cannot combine special syms (e.g. mouse_down + Q)");

        if (isSymSpecial(arg)) {
            if (!keysyms.empty())
                return std::unexpected("Cannot combine special syms (e.g. mouse_down + Q)");

            specialSym = true;
            kb.key     = arg;
            continue;
        }

        auto sym = xkb_keysym_from_name(std::string{arg}.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);

        if (sym == XKB_KEY_NoSymbol) {
            if (arg.contains(' '))
                return std::unexpected(std::format("Unknown keysym: \"{}\", did you forget a +?", arg));

            if (arg == "Enter")
                return std::unexpected(std::format(R"(Unknown keysym: "{}", did you mean "Return"?)", arg));

            return std::unexpected(std::format("Unknown keysym: \"{}\"", arg));
        }

        keysyms.emplace_back(sym);
    }

    kb.modmask = modMask;
    kb.sMkKeys = std::move(keysyms);

    return {};
}

static int hlBind(lua_State* L) {
    auto*            mgr = static_cast<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

    std::string_view keys = luaL_checkstring(L, 1);

    SKeybind         kb;
    kb.submap.name = mgr->m_currentSubmap;

    if (auto res = parseKeyString(kb, keys); !res)
        return luaL_error(L, std::format("hl.bind: failed to parse key string: {}", res.error()).c_str());

    if (!lua_isfunction(L, 2))
        return luaL_error(L, "hl.bind: dispatcher must be a function (e.g. hl.window.close()) or a lua function");

    lua_pushvalue(L, 2);
    int ref    = luaL_ref(L, LUA_REGISTRYINDEX);
    kb.handler = "__lua";
    kb.arg     = std::to_string(ref);

    int optsIdx = 3;

    if (lua_istable(L, optsIdx)) {
        auto getBool = [&](const char* field) -> bool {
            lua_getfield(L, optsIdx, field);
            bool v = lua_toboolean(L, -1);
            lua_pop(L, 1);
            return v;
        };
        kb.mouse           = getBool("mouse");
        kb.repeat          = getBool("repeating");
        kb.locked          = getBool("locked");
        kb.release         = getBool("release");
        kb.nonConsuming    = getBool("non_consuming");
        kb.transparent     = getBool("transparent");
        kb.ignoreMods      = getBool("ignore_mods");
        kb.dontInhibit     = getBool("dont_inhibit");
        kb.longPress       = getBool("long_press");
        kb.submapUniversal = getBool("submap_universal");

        bool click = false;
        bool drag  = false;

        if (getBool("click")) {
            click      = true;
            kb.release = true;
        }

        if (getBool("drag")) {
            drag       = true;
            kb.release = true;
        }

        if (click && drag)
            luaL_error(L, "hl.bind: click and drag are exclusive");

        if ((kb.longPress || kb.release) && kb.repeat)
            return luaL_error(L, "hl.bind: long_press / release is incompatible with repeat");

        if (kb.mouse && (kb.repeat || kb.release || kb.locked))
            return luaL_error(L, "hl.bind: mouse is exclusive");

        kb.click = click;
        kb.drag  = drag;

        if (kb.mouse)
            kb.handler = "mouse";

        lua_getfield(L, optsIdx, "device");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "inclusive");
            kb.deviceInclusive = lua_isnil(L, -1) ? true : lua_toboolean(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "list");
            if (lua_istable(L, -1)) {
                lua_pushnil(L);
                while (lua_next(L, -2)) {
                    if (lua_isstring(L, -1))
                        kb.devices.emplace(lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }

    g_pKeybindManager->addKeybind(kb);
    return 0;
}

static int hlDefineSubmap(lua_State* L) {
    auto*       mgr  = static_cast<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));
    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    std::string prev     = mgr->m_currentSubmap;
    mgr->m_currentSubmap = name;

    lua_pushvalue(L, 2);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        mgr->addError(std::string("hl.define_submap: error in submap \"") + name + "\": " + lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    mgr->m_currentSubmap = prev;
    return 0;
}

// -- Dispatch closures ----------------------------------------------------------
// Each closure is called at keypress / hl.dispatch() time.
// Upvalues carry parameters captured at config time.

// upval 1: cmd string
static int dsp_execCmd(lua_State* L) {
    auto proc = Config::Supplementary::executor()->spawn(lua_tostring(L, lua_upvalueindex(1)));
    if (!proc.has_value())
        return luaL_error(L, "Failed to start process");
    return 0;
}

static int dsp_execRaw(lua_State* L) {
    auto proc = Config::Supplementary::executor()->spawnRaw(lua_tostring(L, lua_upvalueindex(1)));
    if (!proc || !*proc)
        return luaL_error(L, "Failed to start process");
    return 0;
}

static int dsp_exit(lua_State* L) {
    checkResult(L, CA::exit());
    return 0;
}

// upval 1: submap name
static int dsp_submap(lua_State* L) {
    checkResult(L, CA::setSubmap(lua_tostring(L, lua_upvalueindex(1))));
    return 0;
}

// upval 1: window selector (string, required)
static int dsp_pass(lua_State* L) {
    const auto PWINDOW = g_pCompositor->getWindowByRegex(lua_tostring(L, lua_upvalueindex(1)));
    if (!PWINDOW)
        return luaL_error(L, "hl.pass: window not found");
    checkResult(L, CA::pass(PWINDOW));
    return 0;
}

// upval 1: msg string
static int dsp_layoutMsg(lua_State* L) {
    checkResult(L, CA::layoutMessage(lua_tostring(L, lua_upvalueindex(1))));
    return 0;
}

// upval 1: toggle action (int), upval 2: monitor selector or nil
static int dsp_dpms(lua_State* L) {
    auto                      action = sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1)));
    std::optional<PHLMONITOR> mon    = std::nullopt;
    if (!lua_isnil(L, lua_upvalueindex(2))) {
        auto m = g_pCompositor->getMonitorFromString(lua_tostring(L, lua_upvalueindex(2)));
        if (m)
            mon = m;
    }
    checkResult(L, CA::dpms(action, mon));
    return 0;
}

// upval 1: data string
static int dsp_event(lua_State* L) {
    checkResult(L, CA::event(lua_tostring(L, lua_upvalueindex(1))));
    return 0;
}

static int dsp_global(lua_State* L) {
    checkResult(L, CA::global(lua_tostring(L, lua_upvalueindex(1))));
    return 0;
}

static int dsp_forceRendererReload(lua_State* L) {
    checkResult(L, CA::forceRendererReload());
    return 0;
}

// upval 1: seconds (number)
static int dsp_forceIdle(lua_State* L) {
    checkResult(L, CA::forceIdle((float)lua_tonumber(L, lua_upvalueindex(1))));
    return 0;
}

// -- Top-level factory functions ------------------------------------------------

static int hlExecCmd(lua_State* L) {
    lua_pushstring(L, luaL_checkstring(L, 1));
    lua_pushcclosure(L, dsp_execCmd, 1);
    return 1;
}

static int hlExecRaw(lua_State* L) {
    lua_pushstring(L, luaL_checkstring(L, 1));
    lua_pushcclosure(L, dsp_execRaw, 1);
    return 1;
}

static int hlExit(lua_State* L) {
    lua_pushcclosure(L, dsp_exit, 0);
    return 1;
}

static int hlSubmap(lua_State* L) {
    lua_pushstring(L, luaL_checkstring(L, 1));
    lua_pushcclosure(L, dsp_submap, 1);
    return 1;
}

static int hlPass(lua_State* L) {
    // pass({ window = "class:..." })  — window is required
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.pass: expected a table { window }");
    auto w = tableOptStr(L, 1, "window");
    if (!w)
        return luaL_error(L, "hl.pass: 'window' is required");
    lua_pushstring(L, w->c_str());
    lua_pushcclosure(L, dsp_pass, 1);
    return 1;
}

static int hlLayout(lua_State* L) {
    lua_pushstring(L, luaL_checkstring(L, 1));
    lua_pushcclosure(L, dsp_layoutMsg, 1);
    return 1;
}

static int hlDpms(lua_State* L) {
    // dpms({ action?, monitor? })
    CA::eTogglableAction       action = CA::TOGGLE_ACTION_TOGGLE;
    std::optional<std::string> monStr;
    if (lua_istable(L, 1)) {
        auto a = tableOptStr(L, 1, "action");
        if (a)
            action = parseToggleStr(*a);
        monStr = tableOptStr(L, 1, "monitor");
    }
    lua_pushnumber(L, (int)action);
    if (monStr)
        lua_pushstring(L, monStr->c_str());
    else
        lua_pushnil(L);
    lua_pushcclosure(L, dsp_dpms, 2);
    return 1;
}

static int hlEvent(lua_State* L) {
    lua_pushstring(L, luaL_checkstring(L, 1));
    lua_pushcclosure(L, dsp_event, 1);
    return 1;
}

static int hlGlobal(lua_State* L) {
    lua_pushstring(L, luaL_checkstring(L, 1));
    lua_pushcclosure(L, dsp_global, 1);
    return 1;
}

static int hlForceRendererReload(lua_State* L) {
    lua_pushcclosure(L, dsp_forceRendererReload, 0);
    return 1;
}

static int hlForceIdle(lua_State* L) {
    lua_pushnumber(L, luaL_checknumber(L, 1));
    lua_pushcclosure(L, dsp_forceIdle, 1);
    return 1;
}

// -- Key resolution helper (dispatch-time) --------------------------------------

static std::expected<uint32_t, std::string> resolveKeycode(const std::string& key) {
    if (isNumber(key) && std::stoi(key) > 9)
        return (uint32_t)std::stoi(key);

    if (key.starts_with("code:") && isNumber(key.substr(5)))
        return (uint32_t)std::stoi(key.substr(5));

    if (key.starts_with("mouse:") && isNumber(key.substr(6))) {
        uint32_t code = std::stoi(key.substr(6));
        if (code < 272)
            return std::unexpected("invalid mouse button");
        return code;
    }

    const auto KEYSYM = xkb_keysym_from_name(key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);

    const auto KB = g_pSeatManager->m_keyboard;
    if (!KB)
        return std::unexpected("no keyboard");

    const auto KEYPAIRSTRING = std::format("{}{}", rc<uintptr_t>(KB.get()), key);

    if (g_pKeybindManager->m_keyToCodeCache.contains(KEYPAIRSTRING))
        return g_pKeybindManager->m_keyToCodeCache[KEYPAIRSTRING];

    xkb_keymap*   km          = KB->m_xkbKeymap;
    xkb_state*    ks          = KB->m_xkbState;
    xkb_keycode_t keycode_min = xkb_keymap_min_keycode(km);
    xkb_keycode_t keycode_max = xkb_keymap_max_keycode(km);
    uint32_t      keycode     = 0;

    for (xkb_keycode_t kc = keycode_min; kc <= keycode_max; ++kc) {
        xkb_keysym_t sym = xkb_state_key_get_one_sym(ks, kc);
        if (sym == KEYSYM) {
            keycode                                            = kc;
            g_pKeybindManager->m_keyToCodeCache[KEYPAIRSTRING] = keycode;
        }
    }

    if (!keycode)
        return std::unexpected("key not found");

    return keycode;
}

// upval 1: mods string, upval 2: key string, upval 3: window or nil
static int dsp_sendShortcut(lua_State* L) {
    const uint32_t    modMask = g_pKeybindManager->stringToModMask(lua_tostring(L, lua_upvalueindex(1)));
    const std::string key     = lua_tostring(L, lua_upvalueindex(2));

    auto              keycodeResult = resolveKeycode(key);
    if (!keycodeResult)
        return luaL_error(L, "hl.send_shortcut: %s", keycodeResult.error().c_str());

    PHLWINDOW window = nullptr;
    if (!lua_isnil(L, lua_upvalueindex(3))) {
        window = g_pCompositor->getWindowByRegex(lua_tostring(L, lua_upvalueindex(3)));
        if (!window)
            return luaL_error(L, "hl.send_shortcut: window not found");
    }

    checkResult(L, CA::pass(modMask, *keycodeResult, window));
    return 0;
}

// upval 1: mods string, upval 2: key string, upval 3: state (int: 0=up, 1=down, 2=repeat), upval 4: window or nil
static int dsp_sendKeyState(lua_State* L) {
    const uint32_t    modMask  = g_pKeybindManager->stringToModMask(lua_tostring(L, lua_upvalueindex(1)));
    const std::string key      = lua_tostring(L, lua_upvalueindex(2));
    const uint32_t    keyState = (uint32_t)lua_tonumber(L, lua_upvalueindex(3));

    auto              keycodeResult = resolveKeycode(key);
    if (!keycodeResult)
        return luaL_error(L, "hl.send_key_state: %s", keycodeResult.error().c_str());

    PHLWINDOW window = nullptr;
    if (!lua_isnil(L, lua_upvalueindex(4))) {
        window = g_pCompositor->getWindowByRegex(lua_tostring(L, lua_upvalueindex(4)));
        if (!window)
            return luaL_error(L, "hl.send_key_state: window not found");
    }

    checkResult(L, CA::sendKeyState(modMask, *keycodeResult, keyState, window));
    return 0;
}

static int hlSendShortcut(lua_State* L) {
    // send_shortcut({ mods, key, window? })
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.send_shortcut: expected a table { mods, key, window? }");
    auto mods = tableOptStr(L, 1, "mods");
    auto key  = tableOptStr(L, 1, "key");
    if (!mods || !key)
        return luaL_error(L, "hl.send_shortcut: 'mods' and 'key' are required");
    lua_pushstring(L, mods->c_str());
    lua_pushstring(L, key->c_str());
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_sendShortcut, 3);
    return 1;
}

static int hlSendKeyState(lua_State* L) {
    // send_key_state({ mods, key, state = "down"|"up"|"repeat", window? })
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.send_key_state: expected a table { mods, key, state, window? }");
    auto mods     = tableOptStr(L, 1, "mods");
    auto key      = tableOptStr(L, 1, "key");
    auto stateStr = tableOptStr(L, 1, "state");
    if (!mods || !key || !stateStr)
        return luaL_error(L, "hl.send_key_state: 'mods', 'key', and 'state' are required");

    uint32_t keyState = 0;
    if (*stateStr == "down")
        keyState = 1;
    else if (*stateStr == "repeat")
        keyState = 2;
    else if (*stateStr != "up")
        return luaL_error(L, "hl.send_key_state: 'state' must be \"down\", \"up\", or \"repeat\"");

    lua_pushstring(L, mods->c_str());
    lua_pushstring(L, key->c_str());
    lua_pushnumber(L, keyState);
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_sendKeyState, 4);
    return 1;
}

static int hlPrint(lua_State* L) {
    const int   n = lua_gettop(L);
    std::string out;
    for (int i = 1; i <= n; i++) {
        size_t      len = 0;
        const char* s   = luaL_tolstring(L, i, &len); // honours __tostring
        if (i > 1)
            out += '\t';
        out.append(s, len);
        lua_pop(L, 1);
    }
    Log::logger->log(Log::INFO, "[Lua] {}", out);
    return 0;
}

static int hlDispatch(lua_State* L) {
    if (!lua_isfunction(L, 1))
        return luaL_error(L, "hl.dispatch: expected a dispatcher function (e.g. hl.window.close())");

    lua_pushvalue(L, 1);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        lua_pop(L, 1);
        return luaL_error(L, "hl.dispatch: %s", err);
    }
    return 0;
}

static int hlGetWindows(lua_State* L) {
    lua_newtable(L);
    int i = 1;
    for (const auto& w : g_pCompositor->m_windows) {
        if (!w->m_isMapped)
            continue;
        Objects::CLuaWindow::push(L, w);
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

static int hlGetWorkspaces(lua_State* L) {
    lua_newtable(L);
    int i = 1;
    for (const auto& wsRef : g_pCompositor->getWorkspaces()) {
        const auto ws = wsRef.lock();
        if (!ws || ws->inert())
            continue;
        Objects::CLuaWorkspace::push(L, ws);
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

static int hlGetMonitors(lua_State* L) {
    lua_newtable(L);
    int i = 1;
    for (const auto& mon : g_pCompositor->m_monitors) {
        Objects::CLuaMonitor::push(L, mon);
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

static int hlGetLayers(lua_State* L) {
    lua_newtable(L);
    int i = 1;
    for (const auto& mon : g_pCompositor->m_monitors) {
        for (const auto& level : mon->m_layerSurfaceLayers) {
            for (const auto& lsRef : level) {
                const auto ls = lsRef.lock();
                if (!ls)
                    continue;
                Objects::CLuaLayerSurface::push(L, ls);
                lua_rawseti(L, -2, i++);
            }
        }
    }
    return 1;
}

static int hlOn(lua_State* L) {
    auto*       mgr       = static_cast<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));
    const char* eventName = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    if (!mgr->m_eventHandler->registerEvent(eventName, ref)) {
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        const auto& known = CLuaEventHandler::knownEvents();
        std::string list;
        for (const auto& e : known)
            list += "\n  " + e;
        return luaL_error(L, "%s", (std::string("hl.on: unknown event \"") + eventName + "\". Known events:" + list).c_str());
    }

    return 0;
}

// -- Window dispatch closures ---------------------------------------------------

// upval 1: window selector or nil
static int dsp_closeWindow(lua_State* L) {
    checkResult(L, CA::closeWindow(windowFromUpval(L, 1)));
    return 0;
}

static int dsp_killWindow(lua_State* L) {
    checkResult(L, CA::killWindow(windowFromUpval(L, 1)));
    return 0;
}

// upval 1: signal (int), upval 2: window or nil
static int dsp_signalWindow(lua_State* L) {
    checkResult(L, CA::signalWindow((int)lua_tonumber(L, lua_upvalueindex(1)), windowFromUpval(L, 2)));
    return 0;
}

// upval 1: toggle action (int), upval 2: window or nil
static int dsp_floatWindow(lua_State* L) {
    checkResult(L, CA::floatWindow(sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1))), windowFromUpval(L, 2)));
    return 0;
}

// upval 1: mode (int), upval 2: window or nil
static int dsp_fullscreenWindow(lua_State* L) {
    checkResult(L, CA::fullscreenWindow(sc<eFullscreenMode>((int)lua_tonumber(L, lua_upvalueindex(1))), windowFromUpval(L, 2)));
    return 0;
}

// upval 1: internal mode (int), upval 2: client mode (int), upval 3: window or nil
static int dsp_fullscreenState(lua_State* L) {
    checkResult(L,
                CA::fullscreenWindow(sc<eFullscreenMode>((int)lua_tonumber(L, lua_upvalueindex(1))), sc<eFullscreenMode>((int)lua_tonumber(L, lua_upvalueindex(2))),
                                     windowFromUpval(L, 3)));
    return 0;
}

// upval 1: toggle action (int), upval 2: window or nil
static int dsp_pseudoWindow(lua_State* L) {
    checkResult(L, CA::pseudoWindow(sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1))), windowFromUpval(L, 2)));
    return 0;
}

// upval 1: direction (int), upval 2: window or nil
static int dsp_moveInDirection(lua_State* L) {
    checkResult(L, CA::moveInDirection(sc<Math::eDirection>((int)lua_tonumber(L, lua_upvalueindex(1))), windowFromUpval(L, 2)));
    return 0;
}

static int dsp_swapInDirection(lua_State* L) {
    checkResult(L, CA::swapInDirection(sc<Math::eDirection>((int)lua_tonumber(L, lua_upvalueindex(1))), windowFromUpval(L, 2)));
    return 0;
}

static int dsp_center(lua_State* L) {
    checkResult(L, CA::center(windowFromUpval(L, 1)));
    return 0;
}

// upval 1: next (bool), upval 2: tiled (int: -1=nullopt, 0=false, 1=true), upval 3: floating (int), upval 4: window or nil
static int dsp_cycleNext(lua_State* L) {
    bool                next        = lua_toboolean(L, lua_upvalueindex(1));
    int                 tiledRaw    = (int)lua_tonumber(L, lua_upvalueindex(2));
    int                 floatingRaw = (int)lua_tonumber(L, lua_upvalueindex(3));
    std::optional<bool> tiled       = tiledRaw < 0 ? std::nullopt : std::optional(tiledRaw > 0);
    std::optional<bool> floating    = floatingRaw < 0 ? std::nullopt : std::optional(floatingRaw > 0);
    checkResult(L, CA::cycleNext(next, tiled, floating, windowFromUpval(L, 4)));
    return 0;
}

// upval 1: next (bool), upval 2: window or nil
static int dsp_swapNext(lua_State* L) {
    checkResult(L, CA::swapNext(lua_toboolean(L, lua_upvalueindex(1)), windowFromUpval(L, 2)));
    return 0;
}

// upval 1: window selector (string, required)
static int dsp_focusWindow(lua_State* L) {
    const auto PWINDOW = g_pCompositor->getWindowByRegex(lua_tostring(L, lua_upvalueindex(1)));
    if (!PWINDOW)
        return luaL_error(L, "hl.window.focus: window not found");
    checkResult(L, CA::focus(PWINDOW));
    return 0;
}

// upval 1: tag string, upval 2: window or nil
static int dsp_tagWindow(lua_State* L) {
    checkResult(L, CA::tag(lua_tostring(L, lua_upvalueindex(1)), windowFromUpval(L, 2)));
    return 0;
}

static int dsp_toggleSwallow(lua_State* L) {
    checkResult(L, CA::toggleSwallow());
    return 0;
}

// upval 1: x (number), upval 2: y (number), upval 3: window or nil
static int dsp_resizeBy(lua_State* L) {
    Vector2D delta{lua_tonumber(L, lua_upvalueindex(1)), lua_tonumber(L, lua_upvalueindex(2))};
    checkResult(L, CA::resizeBy(delta, windowFromUpval(L, 3)));
    return 0;
}

static int dsp_moveBy(lua_State* L) {
    Vector2D delta{lua_tonumber(L, lua_upvalueindex(1)), lua_tonumber(L, lua_upvalueindex(2))};
    checkResult(L, CA::moveBy(delta, windowFromUpval(L, 3)));
    return 0;
}

// upval 1: toggle action (int), upval 2: window or nil
static int dsp_pinWindow(lua_State* L) {
    checkResult(L, CA::pinWindow(sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1))), windowFromUpval(L, 2)));
    return 0;
}

static int dsp_bringToTop(lua_State* L) {
    checkResult(L, CA::alterZOrder("top"));
    return 0;
}

// upval 1: mode string, upval 2: window or nil
static int dsp_alterZOrder(lua_State* L) {
    checkResult(L, CA::alterZOrder(lua_tostring(L, lua_upvalueindex(1)), windowFromUpval(L, 2)));
    return 0;
}

// upval 1: prop, upval 2: value, upval 3: window or nil
static int dsp_setProp(lua_State* L) {
    checkResult(L, CA::setProp(lua_tostring(L, lua_upvalueindex(1)), lua_tostring(L, lua_upvalueindex(2)), windowFromUpval(L, 3)));
    return 0;
}

// upval 1: direction (int), upval 2: window or nil
static int dsp_moveIntoGroup(lua_State* L) {
    checkResult(L, CA::moveIntoGroup(sc<Math::eDirection>((int)lua_tonumber(L, lua_upvalueindex(1))), windowFromUpval(L, 2)));
    return 0;
}

// upval 1: direction (int), upval 2: window or nil
static int dsp_moveOutOfGroup(lua_State* L) {
    checkResult(L, CA::moveOutOfGroup(sc<Math::eDirection>((int)lua_tonumber(L, lua_upvalueindex(1))), windowFromUpval(L, 2)));
    return 0;
}

// upval 1: direction (int), upval 2: window or nil
static int dsp_moveWindowOrGroup(lua_State* L) {
    checkResult(L, CA::moveWindowOrGroup(sc<Math::eDirection>((int)lua_tonumber(L, lua_upvalueindex(1))), windowFromUpval(L, 2)));
    return 0;
}

// upval 1: toggle action (int)
static int dsp_denyFromGroup(lua_State* L) {
    checkResult(L, CA::denyWindowFromGroup(sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1)))));
    return 0;
}

static int dsp_mouseDrag(lua_State* L) {
    checkResult(L, CA::mouse("1movewindow"));
    return 0;
}

static int dsp_mouseResize(lua_State* L) {
    checkResult(L, CA::mouse("1resizewindow"));
    return 0;
}

// -- Window factory functions ---------------------------------------------------

static int hlWindowClose(lua_State* L) {
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_closeWindow, 1);
    return 1;
}

static int hlWindowKill(lua_State* L) {
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_killWindow, 1);
    return 1;
}

static int hlWindowSignal(lua_State* L) {
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.window.signal: expected a table { signal, window? }");
    auto sig = tableOptNum(L, 1, "signal");
    if (!sig)
        return luaL_error(L, "hl.window.signal: 'signal' is required");
    lua_pushnumber(L, *sig);
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_signalWindow, 2);
    return 1;
}

static int hlWindowFloat(lua_State* L) {
    CA::eTogglableAction action = CA::TOGGLE_ACTION_TOGGLE;
    if (lua_istable(L, 1)) {
        auto a = tableOptStr(L, 1, "action");
        if (a)
            action = parseToggleStr(*a);
    }
    lua_pushnumber(L, (int)action);
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_floatWindow, 2);
    return 1;
}

static int hlWindowFullscreen(lua_State* L) {
    // fullscreen({ mode? = "fullscreen"|"maximized", window? })
    eFullscreenMode mode = FSMODE_FULLSCREEN;
    if (lua_istable(L, 1)) {
        auto m = tableOptStr(L, 1, "mode");
        if (m) {
            if (*m == "maximized" || *m == "1")
                mode = FSMODE_MAXIMIZED;
            else if (*m == "fullscreen" || *m == "0")
                mode = FSMODE_FULLSCREEN;
            else
                return luaL_error(L, "hl.window.fullscreen: invalid mode \"%s\" (expected fullscreen/maximized)", m->c_str());
        }
    }
    lua_pushnumber(L, (int)mode);
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_fullscreenWindow, 2);
    return 1;
}

static int hlWindowFullscreenState(lua_State* L) {
    // fullscreen_state({ internal, client, window? })
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.window.fullscreen_state: expected a table { internal, client, window? }");
    auto im = tableOptNum(L, 1, "internal");
    auto cm = tableOptNum(L, 1, "client");
    if (!im || !cm)
        return luaL_error(L, "hl.window.fullscreen_state: 'internal' and 'client' are required");
    lua_pushnumber(L, (int)*im);
    lua_pushnumber(L, (int)*cm);
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_fullscreenState, 3);
    return 1;
}

static int hlWindowPseudo(lua_State* L) {
    CA::eTogglableAction action = CA::TOGGLE_ACTION_TOGGLE;
    if (lua_istable(L, 1)) {
        auto a = tableOptStr(L, 1, "action");
        if (a)
            action = parseToggleStr(*a);
    }
    lua_pushnumber(L, (int)action);
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_pseudoWindow, 2);
    return 1;
}

// -- Unified window move factory: hl.window.move({ direction | x,y | workspace | into_group | out_of_group | group_aware }) --

static int hlWindowMove(lua_State* L) {
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.window.move: expected a table, e.g. { direction = \"left\" }");

    // hl.window.move({ direction = "left" })
    auto dirStr = tableOptStr(L, 1, "direction");
    if (dirStr) {
        auto dir = parseDirectionStr(*dirStr);
        if (dir == Math::DIRECTION_DEFAULT)
            return luaL_error(L, "hl.window.move: invalid direction \"%s\" (expected left/right/up/down)", dirStr->c_str());

        // hl.window.move({ direction = "left", group_aware = true })
        auto groupAware = tableOptBool(L, 1, "group_aware");
        if (groupAware && *groupAware) {
            lua_pushnumber(L, (int)dir);
            pushWindowUpval(L, 1);
            lua_pushcclosure(L, dsp_moveWindowOrGroup, 2);
            return 1;
        }

        lua_pushnumber(L, (int)dir);
        pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_moveInDirection, 2);
        return 1;
    }

    // hl.window.move({ x = 100, y = 0 })
    auto x = tableOptNum(L, 1, "x");
    auto y = tableOptNum(L, 1, "y");
    if (x && y) {
        lua_pushnumber(L, *x);
        lua_pushnumber(L, *y);
        pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_moveBy, 3);
        return 1;
    }

    // hl.window.move({ workspace = 3 }) or hl.window.move({ workspace = "special:magic" })
    auto ws = tableOptStr(L, 1, "workspace");
    if (ws) {
        auto follow = tableOptBool(L, 1, "follow");
        bool silent = follow.has_value() && !*follow; // follow=false means silent
        lua_pushstring(L, ws->c_str());
        lua_pushboolean(L, silent);
        pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_moveToWorkspace, 3);
        return 1;
    }

    // hl.window.move({ into_group = "left" })
    auto intoGroup = tableOptStr(L, 1, "into_group");
    if (intoGroup) {
        auto dir = parseDirectionStr(*intoGroup);
        if (dir == Math::DIRECTION_DEFAULT)
            return luaL_error(L, "hl.window.move: invalid into_group direction \"%s\"", intoGroup->c_str());
        lua_pushnumber(L, (int)dir);
        pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_moveIntoGroup, 2);
        return 1;
    }

    auto intoOrCreateGroup = tableOptStr(L, 1, "into_or_create_group");
    if (intoOrCreateGroup) {
        auto dir = parseDirectionStr(*intoOrCreateGroup);
        if (dir == Math::DIRECTION_DEFAULT)
            return luaL_error(L, "hl.window.move: invalid into_or_create_group direction \"%s\"", intoGroup->c_str());
        lua_pushnumber(L, (int)dir);
        pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_moveIntoGroup, 2);
        return 1;
    }

    // hl.window.move({ out_of_group = "left" })
    auto outOfGroup = tableOptStr(L, 1, "out_of_group");
    if (outOfGroup) {
        auto dir = parseDirectionStr(*outOfGroup);
        lua_pushnumber(L, (int)dir);
        pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_moveOutOfGroup, 2);
        return 1;
    }

    // hl.window.move({ out_of_group = true }) — no direction
    auto outOfGroupBool = tableOptBool(L, 1, "out_of_group");
    if (outOfGroupBool && *outOfGroupBool) {
        lua_pushnumber(L, (int)Math::DIRECTION_DEFAULT);
        pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_moveOutOfGroup, 2);
        return 1;
    }

    return luaL_error(L, "hl.window.move: unrecognized arguments. Expected one of: direction, x+y, workspace, into_group, out_of_group");
}

// -- Unified window swap factory: hl.window.swap({ direction | next | prev }) --

static int hlWindowSwap(lua_State* L) {
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.window.swap: expected a table, e.g. { direction = \"left\" }");

    // hl.window.swap({ direction = "left" })
    auto dirStr = tableOptStr(L, 1, "direction");
    if (dirStr) {
        auto dir = parseDirectionStr(*dirStr);
        if (dir == Math::DIRECTION_DEFAULT)
            return luaL_error(L, "hl.window.swap: invalid direction \"%s\" (expected left/right/up/down)", dirStr->c_str());
        lua_pushnumber(L, (int)dir);
        pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_swapInDirection, 2);
        return 1;
    }

    // hl.window.swap({ next = true })
    auto next = tableOptBool(L, 1, "next");
    if (next && *next) {
        lua_pushboolean(L, true);
        pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_swapNext, 2);
        return 1;
    }

    // hl.window.swap({ prev = true })
    auto prev = tableOptBool(L, 1, "prev");
    if (prev && *prev) {
        lua_pushboolean(L, false);
        pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_swapNext, 2);
        return 1;
    }

    return luaL_error(L, "hl.window.swap: unrecognized arguments. Expected one of: direction, next, prev");
}

static int hlWindowCenter(lua_State* L) {
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_center, 1);
    return 1;
}

static int hlWindowCycleNext(lua_State* L) {
    // cycle_next({ next?, tiled?, floating?, window? })
    bool next     = true;
    int  tiled    = -1; // -1 = nullopt
    int  floating = -1;
    if (lua_istable(L, 1)) {
        auto n = tableOptBool(L, 1, "next");
        if (n)
            next = *n;
        auto t = tableOptBool(L, 1, "tiled");
        if (t)
            tiled = *t ? 1 : 0;
        auto f = tableOptBool(L, 1, "floating");
        if (f)
            floating = *f ? 1 : 0;
    }
    lua_pushboolean(L, next);
    lua_pushnumber(L, tiled);
    lua_pushnumber(L, floating);
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_cycleNext, 4);
    return 1;
}

static int hlWindowTag(lua_State* L) {
    // tag({ tag, window? })
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.window.tag: expected a table { tag, window? }");
    auto t = tableOptStr(L, 1, "tag");
    if (!t)
        return luaL_error(L, "hl.window.tag: 'tag' is required");
    lua_pushstring(L, t->c_str());
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_tagWindow, 2);
    return 1;
}

static int hlWindowToggleSwallow(lua_State* L) {
    lua_pushcclosure(L, dsp_toggleSwallow, 0);
    return 1;
}

static int hlWindowResizeBy(lua_State* L) {
    // resize_by({ x, y, window? })
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.window.resize_by: expected a table { x, y, window? }");
    auto x = tableOptNum(L, 1, "x");
    auto y = tableOptNum(L, 1, "y");
    if (!x || !y)
        return luaL_error(L, "hl.window.resize_by: 'x' and 'y' are required");
    lua_pushnumber(L, *x);
    lua_pushnumber(L, *y);
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_resizeBy, 3);
    return 1;
}

static int hlWindowPin(lua_State* L) {
    CA::eTogglableAction action = CA::TOGGLE_ACTION_TOGGLE;
    if (lua_istable(L, 1)) {
        auto a = tableOptStr(L, 1, "action");
        if (a)
            action = parseToggleStr(*a);
    }
    lua_pushnumber(L, (int)action);
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_pinWindow, 2);
    return 1;
}

static int hlWindowBringToTop(lua_State* L) {
    lua_pushcclosure(L, dsp_bringToTop, 0);
    return 1;
}

static int hlWindowAlterZOrder(lua_State* L) {
    // alter_zorder({ mode, window? })
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.window.alter_zorder: expected a table { mode, window? }");
    auto m = tableOptStr(L, 1, "mode");
    if (!m)
        return luaL_error(L, "hl.window.alter_zorder: 'mode' is required");
    lua_pushstring(L, m->c_str());
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_alterZOrder, 2);
    return 1;
}

static int hlWindowSetProp(lua_State* L) {
    // set_prop({ prop, value, window? })
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.window.set_prop: expected a table { prop, value, window? }");
    auto p = tableOptStr(L, 1, "prop");
    auto v = tableOptStr(L, 1, "value");
    if (!p || !v)
        return luaL_error(L, "hl.window.set_prop: 'prop' and 'value' are required");
    lua_pushstring(L, p->c_str());
    lua_pushstring(L, v->c_str());
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_setProp, 3);
    return 1;
}

static int hlWindowDenyFromGroup(lua_State* L) {
    CA::eTogglableAction action = CA::TOGGLE_ACTION_TOGGLE;
    if (lua_istable(L, 1)) {
        auto a = tableOptStr(L, 1, "action");
        if (a)
            action = parseToggleStr(*a);
    }
    lua_pushnumber(L, (int)action);
    lua_pushcclosure(L, dsp_denyFromGroup, 1);
    return 1;
}

static int hlWindowDrag(lua_State* L) {
    lua_pushcclosure(L, dsp_mouseDrag, 0);
    return 1;
}

static int hlWindowResize(lua_State* L) {
    lua_pushcclosure(L, dsp_mouseResize, 0);
    return 1;
}

// -- Focus dispatch closures ----------------------------------------------------

// upval 1: direction (int)
static int dsp_moveFocus(lua_State* L) {
    checkResult(L, CA::moveFocus(sc<Math::eDirection>((int)lua_tonumber(L, lua_upvalueindex(1)))));
    return 0;
}

// upval 1: monitor selector string
static int dsp_focusMonitor(lua_State* L) {
    const auto PMONITOR = g_pCompositor->getMonitorFromString(lua_tostring(L, lua_upvalueindex(1)));
    if (!PMONITOR)
        return luaL_error(L, "hl.focus.monitor: monitor not found");
    checkResult(L, CA::focusMonitor(PMONITOR));
    return 0;
}

static int dsp_focusUrgentOrLast(lua_State* L) {
    checkResult(L, CA::focusUrgentOrLast());
    return 0;
}

static int dsp_focusCurrentOrLast(lua_State* L) {
    checkResult(L, CA::focusCurrentOrLast());
    return 0;
}

// -- Workspace dispatch closures ------------------------------------------------

// upval 1: workspace string
static int dsp_changeWorkspace(lua_State* L) {
    checkResult(L, CA::changeWorkspace(std::string(lua_tostring(L, lua_upvalueindex(1)))));
    return 0;
}

// upval 1: workspace string, upval 2: silent (bool), upval 3: window or nil
static int dsp_moveToWorkspace(lua_State* L) {
    auto ws = resolveWorkspaceStr(lua_tostring(L, lua_upvalueindex(1)));
    if (!ws)
        return luaL_error(L, "Invalid workspace");
    bool silent = lua_toboolean(L, lua_upvalueindex(2));
    checkResult(L, CA::moveToWorkspace(ws, silent, windowFromUpval(L, 3)));
    return 0;
}

// upval 1: special name string (may be empty)
static int dsp_toggleSpecial(lua_State* L) {
    std::string name                                   = lua_isnil(L, lua_upvalueindex(1)) ? "" : lua_tostring(L, lua_upvalueindex(1));
    const auto& [workspaceID, workspaceName, isAutoID] = getWorkspaceIDNameFromString("special:" + name);
    if (workspaceID == WORKSPACE_INVALID || !g_pCompositor->isWorkspaceSpecial(workspaceID))
        return luaL_error(L, "Invalid special workspace");

    auto ws = g_pCompositor->getWorkspaceByID(workspaceID);
    if (!ws) {
        const auto PMONITOR = Desktop::focusState()->monitor();
        if (PMONITOR)
            ws = g_pCompositor->createNewWorkspace(workspaceID, PMONITOR->m_id, workspaceName);
    }
    if (!ws)
        return luaL_error(L, "Could not resolve special workspace");

    checkResult(L, CA::toggleSpecial(ws));
    return 0;
}

// upval 1: workspace id (number), upval 2: name string
static int dsp_renameWorkspace(lua_State* L) {
    int        wsid = (int)lua_tonumber(L, lua_upvalueindex(1));
    const auto PWS  = g_pCompositor->getWorkspaceByID(wsid);
    if (!PWS)
        return luaL_error(L, "hl.workspace.rename: no such workspace");
    std::string name = lua_isnil(L, lua_upvalueindex(2)) ? "" : lua_tostring(L, lua_upvalueindex(2));
    checkResult(L, CA::renameWorkspace(PWS, name));
    return 0;
}

// upval 1: workspace string, upval 2: monitor string
static int dsp_moveWorkspaceToMonitor(lua_State* L) {
    const auto WORKSPACEID = getWorkspaceIDNameFromString(lua_tostring(L, lua_upvalueindex(1))).id;
    if (WORKSPACEID == WORKSPACE_INVALID)
        return luaL_error(L, "Invalid workspace");
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(WORKSPACEID);
    if (!PWORKSPACE)
        return luaL_error(L, "Workspace not found");
    const auto PMONITOR = g_pCompositor->getMonitorFromString(lua_tostring(L, lua_upvalueindex(2)));
    if (!PMONITOR)
        return luaL_error(L, "Monitor not found");
    checkResult(L, CA::moveToMonitor(PWORKSPACE, PMONITOR));
    return 0;
}

// upval 1: monitor string
static int dsp_moveCurrentWorkspaceToMonitor(lua_State* L) {
    const auto PMONITOR = g_pCompositor->getMonitorFromString(lua_tostring(L, lua_upvalueindex(1)));
    if (!PMONITOR)
        return luaL_error(L, "Monitor not found");
    const auto PCURRENTWORKSPACE = Desktop::focusState()->monitor()->m_activeWorkspace;
    if (!PCURRENTWORKSPACE)
        return luaL_error(L, "Invalid workspace");
    checkResult(L, CA::moveToMonitor(PCURRENTWORKSPACE, PMONITOR));
    return 0;
}

// upval 1: workspace string
static int dsp_focusWorkspaceOnCurrentMonitor(lua_State* L) {
    auto ws = resolveWorkspaceStr(lua_tostring(L, lua_upvalueindex(1)));
    if (!ws)
        return luaL_error(L, "Invalid workspace");
    checkResult(L, CA::changeWorkspaceOnCurrentMonitor(ws));
    return 0;
}

// upval 1: mon1 string, upval 2: mon2 string
static int dsp_swapActiveWorkspaces(lua_State* L) {
    const auto PMON1 = g_pCompositor->getMonitorFromString(lua_tostring(L, lua_upvalueindex(1)));
    const auto PMON2 = g_pCompositor->getMonitorFromString(lua_tostring(L, lua_upvalueindex(2)));
    if (!PMON1 || !PMON2)
        return luaL_error(L, "Monitor not found");
    checkResult(L, CA::swapActiveWorkspaces(PMON1, PMON2));
    return 0;
}

// -- Cursor dispatch closures ---------------------------------------------------

// upval 1: corner (int), upval 2: window or nil
static int dsp_moveCursorToCorner(lua_State* L) {
    checkResult(L, CA::moveCursorToCorner((int)lua_tonumber(L, lua_upvalueindex(1)), windowFromUpval(L, 2)));
    return 0;
}

// upval 1: x, upval 2: y
static int dsp_moveCursor(lua_State* L) {
    checkResult(L, CA::moveCursor(Vector2D{lua_tonumber(L, lua_upvalueindex(1)), lua_tonumber(L, lua_upvalueindex(2))}));
    return 0;
}

// -- Group dispatch closures ----------------------------------------------------

static int dsp_toggleGroup(lua_State* L) {
    checkResult(L, CA::toggleGroup(windowFromUpval(L, 1)));
    return 0;
}

// upval 1: forward (bool), upval 2: window or nil
static int dsp_changeGroupActive(lua_State* L) {
    checkResult(L, CA::changeGroupActive(lua_toboolean(L, lua_upvalueindex(1)), windowFromUpval(L, 2)));
    return 0;
}

// upval 1: forward (bool)
static int dsp_moveGroupWindow(lua_State* L) {
    checkResult(L, CA::moveGroupWindow(lua_toboolean(L, lua_upvalueindex(1))));
    return 0;
}

// upval 1: toggle action (int)
static int dsp_lockGroups(lua_State* L) {
    checkResult(L, CA::lockGroups(sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1)))));
    return 0;
}

static int dsp_lockActiveGroup(lua_State* L) {
    checkResult(L, CA::lockActiveGroup(sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1)))));
    return 0;
}

// -- Unified focus factory: hl.focus({ direction | monitor | window | urgent_or_last | last }) --

static int hlFocus(lua_State* L) {
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.focus: expected a table, e.g. { direction = \"left\" }");

    // hl.focus({ direction = "left" })
    auto dirStr = tableOptStr(L, 1, "direction");
    if (dirStr) {
        auto dir = parseDirectionStr(*dirStr);
        if (dir == Math::DIRECTION_DEFAULT)
            return luaL_error(L, "hl.focus: invalid direction \"%s\" (expected left/right/up/down)", dirStr->c_str());
        lua_pushnumber(L, (int)dir);
        lua_pushcclosure(L, dsp_moveFocus, 1);
        return 1;
    }

    // hl.focus({ monitor = "DP-1" })
    auto monStr = tableOptStr(L, 1, "monitor");
    if (monStr) {
        lua_pushstring(L, monStr->c_str());
        lua_pushcclosure(L, dsp_focusMonitor, 1);
        return 1;
    }

    // hl.focus({ window = "class:firefox" })
    auto winStr = tableOptStr(L, 1, "window");
    if (winStr) {
        lua_pushstring(L, winStr->c_str());
        lua_pushcclosure(L, dsp_focusWindow, 1);
        return 1;
    }

    // hl.focus({ urgent_or_last = true })
    auto urgent = tableOptBool(L, 1, "urgent_or_last");
    if (urgent && *urgent) {
        lua_pushcclosure(L, dsp_focusUrgentOrLast, 0);
        return 1;
    }

    // hl.focus({ last = true })
    auto last = tableOptBool(L, 1, "last");
    if (last && *last) {
        lua_pushcclosure(L, dsp_focusCurrentOrLast, 0);
        return 1;
    }

    return luaL_error(L, "hl.focus: unrecognized arguments. Expected one of: direction, monitor, window, urgent_or_last, last");
}

// -- Workspace factory functions ------------------------------------------------

// -- Unified workspace navigation: hl.workspace(3) / hl.workspace("e+1") / hl.workspace({ special = "magic" }) --

static int hlWorkspace(lua_State* L) {
    // hl.workspace(3) or hl.workspace("e+1") — shorthand
    if (lua_isstring(L, 1) || lua_isnumber(L, 1)) {
        std::string ws = argStr(L, 1);
        lua_pushstring(L, ws.c_str());
        lua_pushcclosure(L, dsp_changeWorkspace, 1);
        return 1;
    }

    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.workspace: expected a number, string, or table");

    // hl.workspace({ special = "magic" })
    auto special = tableOptStr(L, 1, "special");
    if (special) {
        lua_pushstring(L, special->c_str());
        lua_pushcclosure(L, dsp_toggleSpecial, 1);
        return 1;
    }

    // hl.workspace({ id = 3, on_current_monitor = true })
    auto onCurrentMonitor = tableOptBool(L, 1, "on_current_monitor");
    auto id               = tableOptStr(L, 1, "id");
    if (id) {
        if (onCurrentMonitor && *onCurrentMonitor) {
            lua_pushstring(L, id->c_str());
            lua_pushcclosure(L, dsp_focusWorkspaceOnCurrentMonitor, 1);
            return 1;
        }
        lua_pushstring(L, id->c_str());
        lua_pushcclosure(L, dsp_changeWorkspace, 1);
        return 1;
    }

    return luaL_error(L, "hl.workspace: unrecognized arguments. Expected one of: number/string shorthand, { special }, { id }");
}

static int hlWorkspaceRename(lua_State* L) {
    // workspace.rename({ id, name? })
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.workspace.rename: expected a table { id, name? }");
    auto id = tableOptNum(L, 1, "id");
    if (!id)
        return luaL_error(L, "hl.workspace.rename: 'id' is required");
    auto name = tableOptStr(L, 1, "name");
    lua_pushnumber(L, *id);
    if (name)
        lua_pushstring(L, name->c_str());
    else
        lua_pushnil(L);
    lua_pushcclosure(L, dsp_renameWorkspace, 2);
    return 1;
}

// -- Unified workspace move: hl.workspace.move({ monitor, id? }) --

static int hlWorkspaceMove(lua_State* L) {
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.workspace.move: expected a table, e.g. { monitor = \"DP-1\" }");

    auto mon = tableOptStr(L, 1, "monitor");
    if (!mon)
        return luaL_error(L, "hl.workspace.move: 'monitor' is required");

    // hl.workspace.move({ id = 3, monitor = "DP-1" }) — move specific workspace
    auto id = tableOptStr(L, 1, "id");
    if (id) {
        lua_pushstring(L, id->c_str());
        lua_pushstring(L, mon->c_str());
        lua_pushcclosure(L, dsp_moveWorkspaceToMonitor, 2);
        return 1;
    }

    // hl.workspace.move({ monitor = "DP-1" }) — move current workspace
    lua_pushstring(L, mon->c_str());
    lua_pushcclosure(L, dsp_moveCurrentWorkspaceToMonitor, 1);
    return 1;
}

static int hlWorkspaceSwapMonitors(lua_State* L) {
    // workspace.swap_monitors({ monitor1, monitor2 })
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.workspace.swap_monitors: expected a table { monitor1, monitor2 }");
    auto m1 = tableOptStr(L, 1, "monitor1");
    auto m2 = tableOptStr(L, 1, "monitor2");
    if (!m1 || !m2)
        return luaL_error(L, "hl.workspace.swap_monitors: 'monitor1' and 'monitor2' are required");
    lua_pushstring(L, m1->c_str());
    lua_pushstring(L, m2->c_str());
    lua_pushcclosure(L, dsp_swapActiveWorkspaces, 2);
    return 1;
}

// -- Cursor factory functions ---------------------------------------------------

static int hlCursorMoveToCorner(lua_State* L) {
    // cursor.move_to_corner({ corner, window? })
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.cursor.move_to_corner: expected a table { corner, window? }");
    auto c = tableOptNum(L, 1, "corner");
    if (!c)
        return luaL_error(L, "hl.cursor.move_to_corner: 'corner' is required");
    lua_pushnumber(L, *c);
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_moveCursorToCorner, 2);
    return 1;
}

static int hlCursorMove(lua_State* L) {
    // cursor.move({ x, y })
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.cursor.move: expected a table { x, y }");
    auto x = tableOptNum(L, 1, "x");
    auto y = tableOptNum(L, 1, "y");
    if (!x || !y)
        return luaL_error(L, "hl.cursor.move: 'x' and 'y' are required");
    lua_pushnumber(L, *x);
    lua_pushnumber(L, *y);
    lua_pushcclosure(L, dsp_moveCursor, 2);
    return 1;
}

// -- Group factory functions ----------------------------------------------------

static int hlGroupToggle(lua_State* L) {
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_toggleGroup, 1);
    return 1;
}

static int hlGroupNext(lua_State* L) {
    lua_pushboolean(L, true);
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_changeGroupActive, 2);
    return 1;
}

static int hlGroupPrev(lua_State* L) {
    lua_pushboolean(L, false);
    pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_changeGroupActive, 2);
    return 1;
}

static int hlGroupMoveWindow(lua_State* L) {
    // group.move_window({ forward? })
    bool forward = true;
    if (lua_istable(L, 1)) {
        auto f = tableOptBool(L, 1, "forward");
        if (f)
            forward = *f;
    }
    lua_pushboolean(L, forward);
    lua_pushcclosure(L, dsp_moveGroupWindow, 1);
    return 1;
}

static int hlGroupLock(lua_State* L) {
    CA::eTogglableAction action = CA::TOGGLE_ACTION_TOGGLE;
    if (lua_istable(L, 1)) {
        auto a = tableOptStr(L, 1, "action");
        if (a)
            action = parseToggleStr(*a);
    }
    lua_pushnumber(L, (int)action);
    lua_pushcclosure(L, dsp_lockGroups, 1);
    return 1;
}

static int hlGroupLockActive(lua_State* L) {
    CA::eTogglableAction action = CA::TOGGLE_ACTION_TOGGLE;
    if (lua_istable(L, 1)) {
        auto a = tableOptStr(L, 1, "action");
        if (a)
            action = parseToggleStr(*a);
    }
    lua_pushnumber(L, (int)action);
    lua_pushcclosure(L, dsp_lockActiveGroup, 1);
    return 1;
}

static int hlExecOnce(lua_State* L) {
    auto* mgr = static_cast<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

    if (mgr->isFirstLaunch())
        Config::Supplementary::executor()->addExecOnce({argStr(L, 1), true});

    return 0;
}

static int hlExecShutdown(lua_State* L) {
    if (g_pCompositor->m_finalRequests) {
        Config::Supplementary::executor()->spawn(argStr(L, 1));
        return 0;
    }

    Config::Supplementary::executor()->addExecShutdown({argStr(L, 1), true});
    return 0;
}

// push table field onto the stack, parse it with a typed parser, pop, and return the error (if any).
// on success the parsed value lives inside `parser`
template <typename T>
static SParseError parseTableField(lua_State* L, int tableIdx, const char* field, T& parser) {
    lua_getfield(L, tableIdx, field);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return {.errorCode = PARSE_ERROR_BAD_VALUE, .message = std::format("missing required field \"{}\"", field)};
    }
    auto err = parser.parse(L);
    lua_pop(L, 1);
    if (err.errorCode != PARSE_ERROR_OK)
        err.message = std::format("field \"{}\": {}", field, err.message);
    return err;
}

static int hlCurve(lua_State* L) {
    CLuaConfigString nameParser("");
    lua_pushvalue(L, 1);
    auto nameErr = nameParser.parse(L);
    lua_pop(L, 1);
    if (nameErr.errorCode != PARSE_ERROR_OK)
        return luaL_error(L, "%s", std::format("hl.curve: first argument (name) must be a string: {}", nameErr.message).c_str());

    const auto& name = nameParser.parsed();

    if (!lua_istable(L, 2))
        return luaL_error(L, "hl.curve: second argument must be a table, e.g. { type = \"bezier\", points = { {0, 0}, {1, 1} } }");

    // parse type field
    CLuaConfigString typeParser("");
    auto             typeErr = parseTableField(L, 2, "type", typeParser);
    if (typeErr.errorCode != PARSE_ERROR_OK)
        return luaL_error(L, "%s", std::format("hl.curve(\"{}\"): {}", name, typeErr.message).c_str());

    const auto& curveType = typeParser.parsed();

    if (curveType != "bezier")
        return luaL_error(L, "%s", std::format("hl.curve(\"{}\"): unknown curve type \"{}\", expected \"bezier\"", name, curveType).c_str());

    // parse points field - must be a table of two sub-tables: { {p1x, p1y}, {p2x, p2y} }
    lua_getfield(L, 2, "points");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return luaL_error(L, "%s", std::format("hl.curve(\"{}\"): missing or invalid \"points\" field, expected a table of two points", name).c_str());
    }
    int pointsIdx = lua_gettop(L);

    if (luaL_len(L, pointsIdx) != 2) {
        lua_pop(L, 1);
        return luaL_error(L, "%s", std::format("hl.curve(\"{}\"): \"points\" must contain exactly 2 points, e.g. {{ {{0, 0}}, {{1, 1}} }}", name).c_str());
    }

    float coords[4] = {};
    for (int pt = 1; pt <= 2; pt++) {
        lua_rawgeti(L, pointsIdx, pt);
        if (!lua_istable(L, -1) || luaL_len(L, -1) != 2) {
            lua_pop(L, 2); // point + points table
            return luaL_error(L, "%s", std::format("hl.curve(\"{}\"): point {} must be a table of 2 numbers, e.g. {{0.25, 0.1}}", name, pt).c_str());
        }
        int ptIdx = lua_gettop(L);

        for (int comp = 0; comp < 2; comp++) {
            lua_rawgeti(L, ptIdx, comp + 1);
            CLuaConfigFloat coordParser(0.F, -1.F, 2.F);
            auto            coordErr = coordParser.parse(L);
            lua_pop(L, 1);
            if (coordErr.errorCode != PARSE_ERROR_OK) {
                lua_pop(L, 2); // point + points table
                return luaL_error(L, "%s", std::format("hl.curve(\"{}\"): point {}[{}]: {}", name, pt, comp + 1, coordErr.message).c_str());
            }
            coords[((pt - 1) * 2) + comp] = coordParser.parsed();
        }

        lua_pop(L, 1); // pop point table
    }
    lua_pop(L, 1); // pop points table

    g_pAnimationManager->addBezierWithName(name, Vector2D(coords[0], coords[1]), Vector2D(coords[2], coords[3]));
    return 0;
}

static int hlAnimation(lua_State* L) {
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.animation: expected a table, e.g. { leaf = \"global\", enabled = true, speed = 5, bezier = \"default\" }");

    // parse leaf
    CLuaConfigString leafParser("");
    auto             leafErr = parseTableField(L, 1, "leaf", leafParser);
    if (leafErr.errorCode != PARSE_ERROR_OK)
        return luaL_error(L, "%s", std::format("hl.animation: {}", leafErr.message).c_str());

    const auto leaf = leafParser.parsed();

    if (!Config::animationTree()->nodeExists(leaf))
        return luaL_error(L, "%s", std::format("hl.animation: no such animation leaf \"{}\"", leaf).c_str());

    // parse enabled
    CLuaConfigBool enabledParser(true);
    auto           enabledErr = parseTableField(L, 1, "enabled", enabledParser);
    if (enabledErr.errorCode != PARSE_ERROR_OK)
        return luaL_error(L, "%s", std::format("hl.animation(\"{}\"): {}", leaf, enabledErr.message).c_str());

    bool enabled = enabledParser.parsed();

    if (!enabled) {
        Config::animationTree()->setConfigForNode(leaf, false, 1, "default");
        return 0;
    }

    // parse speed
    CLuaConfigFloat speedParser(0.F, 0.F, 100.F);
    auto            speedErr = parseTableField(L, 1, "speed", speedParser);
    if (speedErr.errorCode != PARSE_ERROR_OK)
        return luaL_error(L, "%s", std::format("hl.animation(\"{}\"): {}", leaf, speedErr.message).c_str());

    float speed = speedParser.parsed();

    if (speed <= 0)
        return luaL_error(L, "%s", std::format("hl.animation(\"{}\"): speed must be greater than 0", leaf).c_str());

    // parse bezier
    CLuaConfigString bezierParser("");
    auto             bezierErr = parseTableField(L, 1, "bezier", bezierParser);
    if (bezierErr.errorCode != PARSE_ERROR_OK)
        return luaL_error(L, "%s", std::format("hl.animation(\"{}\"): {}", leaf, bezierErr.message).c_str());

    const auto& bezierName = bezierParser.parsed();

    if (!g_pAnimationManager->bezierExists(bezierName))
        return luaL_error(L, "%s", std::format("hl.animation(\"{}\"): no such bezier \"{}\"", leaf, bezierName).c_str());

    // parse optional style
    std::string style;
    lua_getfield(L, 1, "style");
    if (!lua_isnil(L, -1)) {
        CLuaConfigString styleParser("");
        auto             styleErr = styleParser.parse(L);
        if (styleErr.errorCode != PARSE_ERROR_OK) {
            lua_pop(L, 1);
            return luaL_error(L, "%s", std::format("hl.animation(\"{}\"): field \"style\": {}", leaf, styleErr.message).c_str());
        }
        style = styleParser.parsed();
    }
    lua_pop(L, 1);

    if (!style.empty()) {
        auto err = g_pAnimationManager->styleValidInConfigVar(leaf, style);
        if (!err.empty()) {
            return luaL_error(L, "%s", std::format("hl.animation(\"{}\"): {}", leaf, err).c_str());
        }
    }

    Config::animationTree()->setConfigForNode(leaf, true, speed, bezierName, style);
    return 0;
}

static int hlUnbind(lua_State* L) {
    // hl.unbind("all") clears everything
    if (lua_isstring(L, 1) && std::string_view(lua_tostring(L, 1)) == "all" && lua_gettop(L) == 1) {
        g_pKeybindManager->clearKeybinds();
        return 0;
    }

    const char* mods   = luaL_checkstring(L, 1);
    const char* keyStr = luaL_checkstring(L, 2);

    uint32_t    mod = g_pKeybindManager->stringToModMask(mods);

    SParsedKey  key;
    std::string k = keyStr;
    if (Hyprutils::String::isNumber(k) && std::stoi(k) > 9)
        key = {.keycode = (uint32_t)std::stoi(k)};
    else if (k.starts_with("code:") && Hyprutils::String::isNumber(k.substr(5)))
        key = {.keycode = (uint32_t)std::stoi(k.substr(5))};
    else if (k == "catchall")
        key = {.catchAll = true};
    else
        key = {.key = k};

    g_pKeybindManager->removeKeybind(mod, key);
    return 0;
}

static int hlTimer(lua_State* L) {
    auto* mgr = static_cast<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

    luaL_checktype(L, 1, LUA_TFUNCTION);
    luaL_checktype(L, 2, LUA_TTABLE);

    // read opts.timeout
    lua_getfield(L, 2, "timeout");
    if (!lua_isnumber(L, -1))
        return luaL_error(L, "hl.timer: opts.timeout must be a number (ms)");
    int timeoutMs = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);

    if (timeoutMs <= 0)
        return luaL_error(L, "hl.timer: opts.timeout must be > 0");

    // read opts.type
    lua_getfield(L, 2, "type");
    if (!lua_isstring(L, -1))
        return luaL_error(L, "hl.timer: opts.type must be \"repeat\" or \"oneshot\"");
    std::string type = lua_tostring(L, -1);
    lua_pop(L, 1);

    bool repeat = false;
    if (type == "repeat")
        repeat = true;
    else if (type != "oneshot")
        return luaL_error(L, "hl.timer: opts.type must be \"repeat\" or \"oneshot\"");

    // store the lua callback
    lua_pushvalue(L, 1);
    int  ref = luaL_ref(L, LUA_REGISTRYINDEX);

    auto timer = makeShared<CEventLoopTimer>(
        std::chrono::milliseconds(timeoutMs),
        [L, ref, repeat, timeoutMs, mgr](SP<CEventLoopTimer> self, void* data) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                Log::logger->log(Log::ERR, "[Lua] error in timer callback: {}", lua_tostring(L, -1));
                lua_pop(L, 1);
            }

            if (repeat)
                self->updateTimeout(std::chrono::milliseconds(timeoutMs));
            else {
                luaL_unref(L, LUA_REGISTRYINDEX, ref);
                std::erase_if(mgr->m_luaTimers, [&self](const auto& lt) { return lt.timer == self; });
            }
        },
        nullptr);

    mgr->m_luaTimers.emplace_back(CConfigManager::SLuaTimer{timer, ref});
    g_pEventLoopManager->addTimer(timer);

    return 0;
}

static int hlEnv(lua_State* L) {
    auto*            mgr = static_cast<CConfigManager*>(lua_touserdata(L, lua_upvalueindex(1)));

    CLuaConfigString nameParser("");
    lua_pushvalue(L, 1);
    auto nameErr = nameParser.parse(L);
    lua_pop(L, 1);
    if (nameErr.errorCode != PARSE_ERROR_OK)
        return luaL_error(L, "%s", std::format("hl.env: first argument (name) must be a string: {}", nameErr.message).c_str());

    const auto& name = nameParser.parsed();

    if (name.empty())
        return luaL_error(L, "hl.env: name must not be empty");

    CLuaConfigString valueParser("");
    lua_pushvalue(L, 2);
    auto valueErr = valueParser.parse(L);
    lua_pop(L, 1);
    if (valueErr.errorCode != PARSE_ERROR_OK)
        return luaL_error(L, "%s", std::format("hl.env: second argument (value) must be a string: {}", valueErr.message).c_str());

    const auto& value = valueParser.parsed();

    if (!mgr->isFirstLaunch()) {
        const auto* ENV = getenv(name.c_str());
        if (ENV && ENV == value)
            return 0;
    }

    setenv(name.c_str(), value.c_str(), 1);

    // optional third argument: propagate to dbus/systemd
    bool dbus = false;
    if (!lua_isnoneornil(L, 3)) {
        CLuaConfigBool dbusParser(false);
        lua_pushvalue(L, 3);
        auto dbusErr = dbusParser.parse(L);
        lua_pop(L, 1);
        if (dbusErr.errorCode != PARSE_ERROR_OK)
            return luaL_error(L, "%s", std::format("hl.env: third argument (dbus) must be a boolean: {}", dbusErr.message).c_str());

        dbus = dbusParser.parsed();
    }

    if (dbus) {
        std::string CMD;
#ifdef USES_SYSTEMD
        CMD = "systemctl --user import-environment " + name + " && hash dbus-update-activation-environment 2>/dev/null && ";
#endif
        CMD += "dbus-update-activation-environment --systemd " + name;
        if (mgr->isFirstLaunch())
            Config::Supplementary::executor()->addExecOnce({CMD, false});
        else
            Config::Supplementary::executor()->spawnRaw(CMD);
    }

    return 0;
}

static int hlGesture(lua_State* L) {
    if (!lua_istable(L, 1))
        return luaL_error(L, "hl.gesture: expected a table, e.g. { fingers = 3, direction = \"horizontal\", action = \"workspace\" }");

    // parse fingers
    CLuaConfigInt fingersParser(0, 2, 9);
    auto          fingersErr = parseTableField(L, 1, "fingers", fingersParser);
    if (fingersErr.errorCode != PARSE_ERROR_OK)
        return luaL_error(L, "%s", std::format("hl.gesture: {}", fingersErr.message).c_str());

    size_t fingerCount = fingersParser.parsed();

    // parse direction
    CLuaConfigString dirParser("");
    auto             dirErr = parseTableField(L, 1, "direction", dirParser);
    if (dirErr.errorCode != PARSE_ERROR_OK)
        return luaL_error(L, "%s", std::format("hl.gesture: {}", dirErr.message).c_str());

    const auto direction = g_pTrackpadGestures->dirForString(dirParser.parsed());
    if (direction == TRACKPAD_GESTURE_DIR_NONE)
        return luaL_error(L, "%s", std::format("hl.gesture: invalid direction \"{}\"", dirParser.parsed()).c_str());

    // parse action
    CLuaConfigString actionParser("");
    auto             actionErr = parseTableField(L, 1, "action", actionParser);
    if (actionErr.errorCode != PARSE_ERROR_OK)
        return luaL_error(L, "%s", std::format("hl.gesture: {}", actionErr.message).c_str());

    const auto& action = actionParser.parsed();

    // parse optional mods
    uint32_t modMask = 0;
    lua_getfield(L, 1, "mods");
    if (!lua_isnil(L, -1)) {
        CLuaConfigString modsParser("");
        auto             modsErr = modsParser.parse(L);
        if (modsErr.errorCode != PARSE_ERROR_OK) {
            lua_pop(L, 1);
            return luaL_error(L, "%s", std::format("hl.gesture: field \"mods\": {}", modsErr.message).c_str());
        }
        modMask = g_pKeybindManager->stringToModMask(modsParser.parsed());
    }
    lua_pop(L, 1);

    // parse optional scale (clamped 0.1 - 10)
    float deltaScale = 1.F;
    lua_getfield(L, 1, "scale");
    if (!lua_isnil(L, -1)) {
        CLuaConfigFloat scaleParser(1.F, 0.1F, 10.F);
        auto            scaleErr = scaleParser.parse(L);
        if (scaleErr.errorCode != PARSE_ERROR_OK) {
            lua_pop(L, 1);
            return luaL_error(L, "%s", std::format("hl.gesture: field \"scale\": {}", scaleErr.message).c_str());
        }
        deltaScale = scaleParser.parsed();
    }
    lua_pop(L, 1);

    // parse optional arg (for dispatcher, special, float, fullscreen, cursorZoom)
    std::string actionArg;
    lua_getfield(L, 1, "arg");
    if (!lua_isnil(L, -1)) {
        CLuaConfigString argParser("");
        auto             argErr = argParser.parse(L);
        if (argErr.errorCode != PARSE_ERROR_OK) {
            lua_pop(L, 1);
            return luaL_error(L, "%s", std::format("hl.gesture: field \"arg\": {}", argErr.message).c_str());
        }
        actionArg = argParser.parsed();
    }
    lua_pop(L, 1);

    // parse optional arg2 (for cursorZoom second arg)
    std::string actionArg2;
    lua_getfield(L, 1, "arg2");
    if (!lua_isnil(L, -1)) {
        CLuaConfigString arg2Parser("");
        auto             arg2Err = arg2Parser.parse(L);
        if (arg2Err.errorCode != PARSE_ERROR_OK) {
            lua_pop(L, 1);
            return luaL_error(L, "%s", std::format("hl.gesture: field \"arg2\": {}", arg2Err.message).c_str());
        }
        actionArg2 = arg2Parser.parsed();
    }
    lua_pop(L, 1);

    constexpr bool                   disableInhibit = false;

    std::expected<void, std::string> result;

    if (action == "dispatcher")
        result = g_pTrackpadGestures->addGesture(makeUnique<CDispatcherTrackpadGesture>(actionArg, actionArg2), fingerCount, direction, modMask, deltaScale, disableInhibit);
    else if (action == "workspace")
        result = g_pTrackpadGestures->addGesture(makeUnique<CWorkspaceSwipeGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);
    else if (action == "resize")
        result = g_pTrackpadGestures->addGesture(makeUnique<CResizeTrackpadGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);
    else if (action == "move")
        result = g_pTrackpadGestures->addGesture(makeUnique<CMoveTrackpadGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);
    else if (action == "special")
        result = g_pTrackpadGestures->addGesture(makeUnique<CSpecialWorkspaceGesture>(actionArg), fingerCount, direction, modMask, deltaScale, disableInhibit);
    else if (action == "close")
        result = g_pTrackpadGestures->addGesture(makeUnique<CCloseTrackpadGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);
    else if (action == "float")
        result = g_pTrackpadGestures->addGesture(makeUnique<CFloatTrackpadGesture>(actionArg), fingerCount, direction, modMask, deltaScale, disableInhibit);
    else if (action == "fullscreen")
        result = g_pTrackpadGestures->addGesture(makeUnique<CFullscreenTrackpadGesture>(actionArg), fingerCount, direction, modMask, deltaScale, disableInhibit);
    else if (action == "cursorZoom")
        result = g_pTrackpadGestures->addGesture(makeUnique<CCursorZoomTrackpadGesture>(actionArg, actionArg2), fingerCount, direction, modMask, deltaScale, disableInhibit);
    else if (action == "unset")
        result = g_pTrackpadGestures->removeGesture(fingerCount, direction, modMask, deltaScale, disableInhibit);
    else
        return luaL_error(L, "%s", std::format("hl.gesture: unknown action \"{}\"", action).c_str());

    if (!result)
        return luaL_error(L, "%s", std::format("hl.gesture: {}", result.error()).c_str());

    return 0;
}

void Bindings::registerBindings(lua_State* L, CConfigManager* mgr) {
    // register a __lua dispatcher for lua lambda keybinds
    g_pKeybindManager->m_dispatchers["__lua"] = [L](std::string arg) -> SDispatchResult {
        int ref = std::stoi(arg);
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            Log::logger->log(Log::ERR, "[Lua] error in keybind lambda: {}", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        return {};
    };

    lua_getglobal(L, "hl");

    // hl.on
    lua_pushlightuserdata(L, mgr);
    lua_pushcclosure(L, hlOn, 1);
    lua_setfield(L, -2, "on");

    // hl.bind
    lua_pushlightuserdata(L, mgr);
    lua_pushcclosure(L, hlBind, 1);
    lua_setfield(L, -2, "bind");

    // hl.define_submap
    lua_pushlightuserdata(L, mgr);
    lua_pushcclosure(L, hlDefineSubmap, 1);
    lua_setfield(L, -2, "define_submap");

    // hl.timer
    lua_pushlightuserdata(L, mgr);
    lua_pushcclosure(L, hlTimer, 1);
    lua_setfield(L, -2, "timer");

    // hl.dispatch
    lua_pushcfunction(L, hlDispatch);
    lua_setfield(L, -2, "dispatch");

    // Top-level dispatcher factories
    lua_pushcfunction(L, hlExecCmd);
    lua_setfield(L, -2, "exec_cmd");
    lua_pushcfunction(L, hlExecRaw);
    lua_setfield(L, -2, "exec_raw");
    lua_pushcfunction(L, hlExit);
    lua_setfield(L, -2, "exit");
    lua_pushcfunction(L, hlSubmap);
    lua_setfield(L, -2, "submap");
    lua_pushcfunction(L, hlPass);
    lua_setfield(L, -2, "pass");
    lua_pushcfunction(L, hlSendShortcut);
    lua_setfield(L, -2, "send_shortcut");
    lua_pushcfunction(L, hlSendKeyState);
    lua_setfield(L, -2, "send_key_state");
    lua_pushcfunction(L, hlLayout);
    lua_setfield(L, -2, "layout");
    lua_pushcfunction(L, hlDpms);
    lua_setfield(L, -2, "dpms");
    lua_pushcfunction(L, hlEvent);
    lua_setfield(L, -2, "event");
    lua_pushcfunction(L, hlGlobal);
    lua_setfield(L, -2, "global");
    lua_pushcfunction(L, hlForceRendererReload);
    lua_setfield(L, -2, "force_renderer_reload");
    lua_pushcfunction(L, hlForceIdle);
    lua_setfield(L, -2, "force_idle");

    // hl.env
    lua_pushlightuserdata(L, mgr);
    lua_pushcclosure(L, hlEnv, 1);
    lua_setfield(L, -2, "env");

    // hl.gesture
    lua_pushcfunction(L, hlGesture);
    lua_setfield(L, -2, "gesture");

    // hl.exec_once
    lua_pushlightuserdata(L, mgr);
    lua_pushcclosure(L, hlExecOnce, 1);
    lua_setfield(L, -2, "exec_once");

    // hl.exec_shutdown
    lua_pushcfunction(L, hlExecShutdown);
    lua_setfield(L, -2, "exec_shutdown");

    // hl.curve
    lua_pushcfunction(L, hlCurve);
    lua_setfield(L, -2, "curve");

    // hl.animation
    lua_pushcfunction(L, hlAnimation);
    lua_setfield(L, -2, "animation");

    // hl.unbind
    lua_pushcfunction(L, hlUnbind);
    lua_setfield(L, -2, "unbind");

    lua_pushcfunction(L, hlGetWindows);
    lua_setfield(L, -2, "get_windows");
    lua_pushcfunction(L, hlGetWorkspaces);
    lua_setfield(L, -2, "get_workspaces");
    lua_pushcfunction(L, hlGetMonitors);
    lua_setfield(L, -2, "get_monitors");
    lua_pushcfunction(L, hlGetLayers);
    lua_setfield(L, -2, "get_layers");

    // hl.window
    lua_newtable(L);
    lua_pushcfunction(L, hlWindowClose);
    lua_setfield(L, -2, "close");
    lua_pushcfunction(L, hlWindowKill);
    lua_setfield(L, -2, "kill");
    lua_pushcfunction(L, hlWindowSignal);
    lua_setfield(L, -2, "signal");
    lua_pushcfunction(L, hlWindowFloat);
    lua_setfield(L, -2, "float");
    lua_pushcfunction(L, hlWindowFullscreen);
    lua_setfield(L, -2, "fullscreen");
    lua_pushcfunction(L, hlWindowFullscreenState);
    lua_setfield(L, -2, "fullscreen_state");
    lua_pushcfunction(L, hlWindowPseudo);
    lua_setfield(L, -2, "pseudo");
    lua_pushcfunction(L, hlWindowMove);
    lua_setfield(L, -2, "move");
    lua_pushcfunction(L, hlWindowSwap);
    lua_setfield(L, -2, "swap");
    lua_pushcfunction(L, hlWindowCenter);
    lua_setfield(L, -2, "center");
    lua_pushcfunction(L, hlWindowCycleNext);
    lua_setfield(L, -2, "cycle_next");
    lua_pushcfunction(L, hlWindowTag);
    lua_setfield(L, -2, "tag");
    lua_pushcfunction(L, hlWindowToggleSwallow);
    lua_setfield(L, -2, "toggle_swallow");
    lua_pushcfunction(L, hlWindowResizeBy);
    lua_setfield(L, -2, "resize_by");
    lua_pushcfunction(L, hlWindowPin);
    lua_setfield(L, -2, "pin");
    lua_pushcfunction(L, hlWindowBringToTop);
    lua_setfield(L, -2, "bring_to_top");
    lua_pushcfunction(L, hlWindowAlterZOrder);
    lua_setfield(L, -2, "alter_zorder");
    lua_pushcfunction(L, hlWindowSetProp);
    lua_setfield(L, -2, "set_prop");
    lua_pushcfunction(L, hlWindowDenyFromGroup);
    lua_setfield(L, -2, "deny_from_group");
    lua_pushcfunction(L, hlWindowDrag);
    lua_setfield(L, -2, "drag");
    lua_pushcfunction(L, hlWindowResize);
    lua_setfield(L, -2, "resize");
    lua_setfield(L, -2, "window");

    // hl.focus
    lua_pushcfunction(L, hlFocus);
    lua_setfield(L, -2, "focus");

    // hl.workspace — callable as hl.workspace(3) via __call, with subtable methods
    lua_newtable(L); // workspace table
    lua_pushcfunction(L, hlWorkspaceRename);
    lua_setfield(L, -2, "rename");
    lua_pushcfunction(L, hlWorkspaceMove);
    lua_setfield(L, -2, "move");
    lua_pushcfunction(L, hlWorkspaceSwapMonitors);
    lua_setfield(L, -2, "swap_monitors");
    // set metatable with __call so hl.workspace(3) works
    // __call receives (table, ...) so we shift args by removing the table
    lua_newtable(L); // metatable
    lua_pushcfunction(L, [](lua_State* L) -> int {
        lua_remove(L, 1); // remove the table (self) that __call passes
        return hlWorkspace(L);
    });
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);
    lua_setfield(L, -2, "workspace");

    // hl.cursor
    lua_newtable(L);
    lua_pushcfunction(L, hlCursorMoveToCorner);
    lua_setfield(L, -2, "move_to_corner");
    lua_pushcfunction(L, hlCursorMove);
    lua_setfield(L, -2, "move");
    lua_setfield(L, -2, "cursor");

    // hl.group
    lua_newtable(L);
    lua_pushcfunction(L, hlGroupToggle);
    lua_setfield(L, -2, "toggle");
    lua_pushcfunction(L, hlGroupNext);
    lua_setfield(L, -2, "next");
    lua_pushcfunction(L, hlGroupPrev);
    lua_setfield(L, -2, "prev");
    lua_pushcfunction(L, hlGroupMoveWindow);
    lua_setfield(L, -2, "move_window");
    lua_pushcfunction(L, hlGroupLock);
    lua_setfield(L, -2, "lock");
    lua_pushcfunction(L, hlGroupLockActive);
    lua_setfield(L, -2, "lock_active");
    lua_setfield(L, -2, "group");

    lua_pop(L, 1); // pop hl

    // override the global print() to route through the Hyprland logger.
    lua_pushcfunction(L, hlPrint);
    lua_setglobal(L, "print");
}
