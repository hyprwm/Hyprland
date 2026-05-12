#include "LuaBindingsInternal.hpp"

#include <hyprutils/string/String.hpp>

#include "../../supplementary/executor/Executor.hpp"

#include "../../../managers/SeatManager.hpp"
#include "../../../devices/IKeyboard.hpp"
#include "../../../desktop/rule/windowRule/WindowRule.hpp"

using namespace Config;
using namespace Config::Lua;
using namespace Config::Lua::Bindings;
using namespace Hyprutils::String;

namespace CA = Config::Actions;

static constexpr auto ERR        = CA::eActionErrorLevel::ERROR;
static constexpr auto WARN       = CA::eActionErrorLevel::WARNING;
static constexpr auto INFO       = CA::eActionErrorLevel::INFO;
static constexpr auto C_UNKNOWN  = CA::eActionErrorCode::UNKNOWN;
static constexpr auto C_INVARG   = CA::eActionErrorCode::INVALID_ARGUMENT;
static constexpr auto C_NOTFOUND = CA::eActionErrorCode::NOT_FOUND;
static constexpr auto C_NOTARGET = CA::eActionErrorCode::NO_TARGET;
static constexpr auto C_UNAVAIL  = CA::eActionErrorCode::UNAVAILABLE;
static constexpr auto C_EXECFAIL = CA::eActionErrorCode::EXECUTION_FAILED;

static int            dsp_moveCursorToCorner(lua_State* L) {
    return Internal::checkResult(L, CA::moveCursorToCorner((int)lua_tonumber(L, lua_upvalueindex(1)), Internal::windowFromUpval(L, 2)));
}

static int dsp_moveCursor(lua_State* L) {
    return Internal::checkResult(L, CA::moveCursor(Vector2D{lua_tonumber(L, lua_upvalueindex(1)), lua_tonumber(L, lua_upvalueindex(2))}));
}

static int dsp_toggleGroup(lua_State* L) {
    return Internal::checkResult(L, CA::toggleGroup(Internal::windowFromUpval(L, 1)));
}

static int dsp_changeGroupActive(lua_State* L) {
    return Internal::checkResult(L, CA::changeGroupActive(lua_toboolean(L, lua_upvalueindex(1)), Internal::windowFromUpval(L, 2)));
}

static int dsp_setGroupActive(lua_State* L) {
    return Internal::checkResult(L, CA::setGroupActive((int)lua_tonumber(L, lua_upvalueindex(1)), Internal::windowFromUpval(L, 2)));
}

static int dsp_moveGroupWindow(lua_State* L) {
    return Internal::checkResult(L, CA::moveGroupWindow(lua_toboolean(L, lua_upvalueindex(1))));
}

static int dsp_lockGroups(lua_State* L) {
    return Internal::checkResult(L, CA::lockGroups(sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1)))));
}

static int dsp_lockActiveGroup(lua_State* L) {
    return Internal::checkResult(L, CA::lockActiveGroup(sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1)))));
}

static int hlCursorMoveToCorner(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.cursor.move_to_corner: expected a table { corner, window? }");

    lua_pushnumber(L, Internal::requireTableFieldNum(L, 1, "corner", "hl.cursor.move_to_corner"));
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_moveCursorToCorner, 2);
    return 1;
}

static int hlCursorMove(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.cursor.move: expected a table { x, y }");

    lua_pushnumber(L, Internal::requireTableFieldNum(L, 1, "x", "hl.cursor.move"));
    lua_pushnumber(L, Internal::requireTableFieldNum(L, 1, "y", "hl.cursor.move"));
    lua_pushcclosure(L, dsp_moveCursor, 2);
    return 1;
}

static int hlGroupToggle(lua_State* L) {
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_toggleGroup, 1);
    return 1;
}

static int hlGroupNext(lua_State* L) {
    lua_pushboolean(L, true);
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_changeGroupActive, 2);
    return 1;
}

static int hlGroupPrev(lua_State* L) {
    lua_pushboolean(L, false);
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_changeGroupActive, 2);
    return 1;
}

static int hlGroupMoveWindow(lua_State* L) {
    bool forward = true;
    if (lua_istable(L, 1)) {
        auto f = Internal::tableOptBool(L, 1, "forward");
        if (f)
            forward = *f;
    }
    lua_pushboolean(L, forward);
    lua_pushcclosure(L, dsp_moveGroupWindow, 1);
    return 1;
}

static int hlGroupActive(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.group.active: expected a table { index, window? }");

    lua_pushnumber(L, Internal::requireTableFieldNum(L, 1, "index", "hl.group.active"));
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_setGroupActive, 2);
    return 1;
}

static int hlGroupLock(lua_State* L) {
    const auto action = Internal::tableToggleAction(L, 1);

    lua_pushnumber(L, (int)action);
    lua_pushcclosure(L, dsp_lockGroups, 1);
    return 1;
}

static int hlGroupLockActive(lua_State* L) {
    const auto action = Internal::tableToggleAction(L, 1);

    lua_pushnumber(L, (int)action);
    lua_pushcclosure(L, dsp_lockActiveGroup, 1);
    return 1;
}

static int dsp_execCmd(lua_State* L) {
    auto proc = lua_tostring(L, lua_upvalueindex(1));

    if (std::string_view{proc}.empty())
        return Internal::dispatcherError(L, "Invalid process string", ERR, C_INVARG);

    std::optional<uint64_t> pid;
    auto                    ruleRet = Internal::buildRuleFromTable(L, lua_upvalueindex(2));

    if (!ruleRet)
        return ruleRet.error();

    if (*ruleRet)
        pid = Config::Supplementary::executor()->spawn(Config::Supplementary::SExecRequest{.exec = proc, .rule = std::move(*ruleRet)});
    else
        pid = Config::Supplementary::executor()->spawn(proc);

    if (!pid.has_value())
        return Internal::dispatcherError(L, "Failed to start process", ERR, C_EXECFAIL);
    return Internal::pushSuccessResult(L);
}

static int dsp_execRaw(lua_State* L) {
    auto proc = Config::Supplementary::executor()->spawnRaw(lua_tostring(L, lua_upvalueindex(1)));
    if (!proc || !*proc)
        return Internal::dispatcherError(L, "Failed to start process", ERR, C_EXECFAIL);
    return Internal::pushSuccessResult(L);
}

static int dsp_exit(lua_State* L) {
    return Internal::checkResult(L, CA::exit());
}

static int dsp_submap(lua_State* L) {
    return Internal::checkResult(L, CA::setSubmap(lua_tostring(L, lua_upvalueindex(1))));
}

static int dsp_pass(lua_State* L) {
    const auto PWINDOW = g_pCompositor->getWindowByRegex(lua_tostring(L, lua_upvalueindex(1)));
    if (!PWINDOW)
        return Internal::dispatcherError(L, "hl.pass: window not found", WARN, C_NOTFOUND);
    return Internal::checkResult(L, CA::pass(PWINDOW));
}

static int dsp_layoutMsg(lua_State* L) {
    return Internal::checkResult(L, CA::layoutMessage(lua_tostring(L, lua_upvalueindex(1))));
}

static int dsp_dpms(lua_State* L) {
    auto                      action = sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1)));
    std::optional<PHLMONITOR> mon    = std::nullopt;

    if (!lua_isnil(L, lua_upvalueindex(2))) {
        auto m = g_pCompositor->getMonitorFromString(lua_tostring(L, lua_upvalueindex(2)));
        if (m)
            mon = m;
    }

    return Internal::checkResult(L, CA::dpms(action, mon));
}

static int dsp_event(lua_State* L) {
    return Internal::checkResult(L, CA::event(lua_tostring(L, lua_upvalueindex(1))));
}

static int dsp_global(lua_State* L) {
    return Internal::checkResult(L, CA::global(lua_tostring(L, lua_upvalueindex(1))));
}

static int dsp_forceRendererReload(lua_State* L) {
    return Internal::checkResult(L, CA::forceRendererReload());
}

static int dsp_forceIdle(lua_State* L) {
    return Internal::checkResult(L, CA::forceIdle((float)lua_tonumber(L, lua_upvalueindex(1))));
}

static int hlExecCmd(lua_State* L) {
    const auto proc       = luaL_checkstring(L, 1);
    const bool hasRuleArg = !lua_isnoneornil(L, 2);

    lua_pushstring(L, proc);

    if (hasRuleArg)
        lua_pushvalue(L, 2);
    else
        lua_pushnil(L);

    lua_pushcclosure(L, dsp_execCmd, 2);
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
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.pass: expected a table { window }");

    const auto w = Internal::requireTableFieldWindowSelector(L, 1, "window", "hl.pass");
    lua_pushstring(L, w.c_str());
    lua_pushcclosure(L, dsp_pass, 1);
    return 1;
}

static int hlLayout(lua_State* L) {
    lua_pushstring(L, luaL_checkstring(L, 1));
    lua_pushcclosure(L, dsp_layoutMsg, 1);
    return 1;
}

static int hlDpms(lua_State* L) {
    CA::eTogglableAction       action = Internal::tableToggleAction(L, 1);
    std::optional<std::string> monStr;

    if (lua_istable(L, 1))
        monStr = Internal::tableOptMonitorSelector(L, 1, "monitor", "hl.dpms");

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

static int dsp_sendShortcut(lua_State* L) {
    const uint32_t    modMask = g_pKeybindManager->stringToModMask(lua_tostring(L, lua_upvalueindex(1)));
    const std::string key     = lua_tostring(L, lua_upvalueindex(2));

    auto              keycodeResult = resolveKeycode(key);
    if (!keycodeResult)
        return Internal::dispatcherError(L, std::format("send_shortcut: {}", keycodeResult.error()), ERR, C_INVARG);

    PHLWINDOW window = nullptr;
    if (!lua_isnil(L, lua_upvalueindex(3))) {
        window = g_pCompositor->getWindowByRegex(lua_tostring(L, lua_upvalueindex(3)));
        if (!window)
            return Internal::dispatcherError(L, "send_shortcut: window not found", WARN, C_NOTFOUND);
    }

    return Internal::checkResult(L, CA::pass(modMask, *keycodeResult, window));
}

static int dsp_sendKeyState(lua_State* L) {
    const uint32_t    modMask  = g_pKeybindManager->stringToModMask(lua_tostring(L, lua_upvalueindex(1)));
    const std::string key      = lua_tostring(L, lua_upvalueindex(2));
    const uint32_t    keyState = (uint32_t)lua_tonumber(L, lua_upvalueindex(3));

    auto              keycodeResult = resolveKeycode(key);
    if (!keycodeResult)
        return Internal::dispatcherError(L, std::format("send_key_state: {}", keycodeResult.error()), ERR, C_INVARG);

    PHLWINDOW window = nullptr;
    if (!lua_isnil(L, lua_upvalueindex(4))) {
        window = g_pCompositor->getWindowByRegex(lua_tostring(L, lua_upvalueindex(4)));
        if (!window)
            return Internal::dispatcherError(L, "send_key_state: window not found", WARN, C_NOTFOUND);
    }

    return Internal::checkResult(L, CA::sendKeyState(modMask, *keycodeResult, keyState, window));
}

static int hlSendShortcut(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "send_shortcut: expected a table { mods, key, window? }");

    const auto mods = Internal::requireTableFieldStr(L, 1, "mods", "hl.send_shortcut");
    const auto key  = Internal::requireTableFieldStr(L, 1, "key", "hl.send_shortcut");

    lua_pushstring(L, mods.c_str());
    lua_pushstring(L, key.c_str());
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_sendShortcut, 3);
    return 1;
}

static int hlSendKeyState(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "send_key_state: expected a table { mods, key, state, window? }");

    const auto mods     = Internal::requireTableFieldStr(L, 1, "mods", "hl.send_key_state");
    const auto key      = Internal::requireTableFieldStr(L, 1, "key", "hl.send_key_state");
    const auto stateStr = Internal::requireTableFieldStr(L, 1, "state", "hl.send_key_state");

    uint32_t   keyState = 0;
    if (stateStr == "down")
        keyState = 1;
    else if (stateStr == "repeat")
        keyState = 2;
    else if (stateStr != "up")
        return Internal::configError(L, "send_key_state: 'state' must be \"down\", \"up\", or \"repeat\"");

    lua_pushstring(L, mods.c_str());
    lua_pushstring(L, key.c_str());
    lua_pushnumber(L, keyState);
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_sendKeyState, 4);
    return 1;
}

static int dsp_moveToWorkspace(lua_State* L) {
    auto ws = Internal::resolveWorkspaceStr(lua_tostring(L, lua_upvalueindex(1)));
    if (!ws)
        return Internal::dispatcherError(L, "Invalid workspace", ERR, C_INVARG);

    bool silent = lua_toboolean(L, lua_upvalueindex(2));
    return Internal::checkResult(L, CA::moveToWorkspace(ws, silent, Internal::windowFromUpval(L, 3)));
}

static int dsp_moveToMonitor(lua_State* L) {
    auto mon = Internal::resolveMonitorStr(lua_tostring(L, lua_upvalueindex(1)));
    if (!mon)
        return Internal::dispatcherError(L, "Invalid monitor / monitor doesn't exist", ERR, C_INVARG);

    bool silent = lua_toboolean(L, lua_upvalueindex(2));
    return Internal::checkResult(L, CA::moveToWorkspace(mon->m_activeWorkspace, silent, Internal::windowFromUpval(L, 3)));
}

static int dsp_closeWindow(lua_State* L) {
    return Internal::checkResult(L, CA::closeWindow(Internal::windowFromUpval(L, 1)));
}

static int dsp_killWindow(lua_State* L) {
    return Internal::checkResult(L, CA::killWindow(Internal::windowFromUpval(L, 1)));
}

static int dsp_signalWindow(lua_State* L) {
    return Internal::checkResult(L, CA::signalWindow((int)lua_tonumber(L, lua_upvalueindex(1)), Internal::windowFromUpval(L, 2)));
}

static int dsp_floatWindow(lua_State* L) {
    return Internal::checkResult(L, CA::floatWindow(sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1))), Internal::windowFromUpval(L, 2)));
}

static int dsp_fullscreenWindow(lua_State* L) {
    return Internal::checkResult(L, CA::fullscreenWindow(sc<eFullscreenMode>((int)lua_tonumber(L, lua_upvalueindex(1))), Internal::windowFromUpval(L, 2)));
}

static int dsp_fullscreenWindowWithAction(lua_State* L) {
    const auto mode      = sc<eFullscreenMode>((int)lua_tonumber(L, lua_upvalueindex(1)));
    const int  actionRaw = (int)lua_tonumber(L, lua_upvalueindex(2));
    auto       maybeW    = Internal::windowFromUpval(L, 3);

    if (actionRaw == 0) {
        return Internal::checkResult(L, CA::fullscreenWindow(mode, maybeW));
    }

    const auto target = maybeW.value_or(Desktop::focusState()->window());
    if (!target)
        return Internal::dispatcherError(L, "hl.window.fullscreen: no target", WARN, C_NOTARGET);

    const bool currentlyMode = target->isEffectiveInternalFSMode(mode);

    if (actionRaw == 1) {
        if (!currentlyMode)
            return Internal::checkResult(L, CA::fullscreenWindow(mode, maybeW));
        return Internal::pushSuccessResult(L);
    }

    if (actionRaw == 2) {
        if (currentlyMode)
            return Internal::checkResult(L, CA::fullscreenWindow(mode, maybeW));
        return Internal::pushSuccessResult(L);
    }

    return Internal::dispatcherError(L, "hl.window.fullscreen: invalid action", ERR, C_INVARG);
}

static int dsp_fullscreenState(lua_State* L) {
    const auto desiredInternal = sc<eFullscreenMode>((int)lua_tonumber(L, lua_upvalueindex(1)));
    const auto desiredClient   = sc<eFullscreenMode>((int)lua_tonumber(L, lua_upvalueindex(2)));
    const int  actionRaw       = (int)lua_tonumber(L, lua_upvalueindex(3)); // 0: toggle, 1: set, 2: unset
    auto       maybeW          = Internal::windowFromUpval(L, 4);

    const auto target = maybeW.value_or(Desktop::focusState()->window());
    if (!target)
        return Internal::pushSuccessResult(L);

    const auto CURRENT        = target->m_fullscreenState;
    const bool atDesiredState = CURRENT.internal == desiredInternal && CURRENT.client == desiredClient;

    if (actionRaw == 0) {
        return Internal::checkResult(L, CA::fullscreenWindow(desiredInternal, desiredClient, maybeW));
    }

    if (actionRaw == 1) {
        if (!atDesiredState)
            return Internal::checkResult(L, CA::fullscreenWindow(desiredInternal, desiredClient, maybeW));
        return Internal::pushSuccessResult(L);
    }

    if (actionRaw == 2) {
        if (atDesiredState)
            return Internal::checkResult(L, CA::fullscreenWindow(desiredInternal, desiredClient, maybeW));
        return Internal::pushSuccessResult(L);
    }

    return Internal::dispatcherError(L, "hl.window.fullscreen_state: invalid action", ERR, C_INVARG);
}

static int dsp_pseudoWindow(lua_State* L) {
    return Internal::checkResult(L, CA::pseudoWindow(sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1))), Internal::windowFromUpval(L, 2)));
}

static int dsp_moveInDirection(lua_State* L) {
    return Internal::checkResult(L, CA::moveInDirection(sc<Math::eDirection>((int)lua_tonumber(L, lua_upvalueindex(1))), Internal::windowFromUpval(L, 2)));
}

static int dsp_swapInDirection(lua_State* L) {
    return Internal::checkResult(L, CA::swapInDirection(sc<Math::eDirection>((int)lua_tonumber(L, lua_upvalueindex(1))), Internal::windowFromUpval(L, 2)));
}

static int dsp_center(lua_State* L) {
    return Internal::checkResult(L, CA::center(Internal::windowFromUpval(L, 1)));
}

static int dsp_cycleNext(lua_State* L) {
    bool                next        = lua_toboolean(L, lua_upvalueindex(1));
    int                 tiledRaw    = (int)lua_tonumber(L, lua_upvalueindex(2));
    int                 floatingRaw = (int)lua_tonumber(L, lua_upvalueindex(3));
    std::optional<bool> tiled       = tiledRaw < 0 ? std::nullopt : std::optional(tiledRaw > 0);
    std::optional<bool> floating    = floatingRaw < 0 ? std::nullopt : std::optional(floatingRaw > 0);
    return Internal::checkResult(L, CA::cycleNext(next, tiled, floating, Internal::windowFromUpval(L, 4)));
}

static int dsp_swapNext(lua_State* L) {
    return Internal::checkResult(L, CA::swapNext(lua_toboolean(L, lua_upvalueindex(1)), Internal::windowFromUpval(L, 2)));
}

static int dsp_swapWithWindow(lua_State* L) {
    auto       source = Internal::windowFromUpval(L, 1);

    const auto targetSelector = lua_tostring(L, lua_upvalueindex(2));
    const auto target         = g_pCompositor->getWindowByRegex(targetSelector);
    if (!target)
        return Internal::dispatcherError(L, "hl.window.swap: target window not found", WARN, C_NOTFOUND);

    return Internal::checkResult(L, CA::swapWith(target, source));
}

static int dsp_tagWindow(lua_State* L) {
    return Internal::checkResult(L, CA::tag(lua_tostring(L, lua_upvalueindex(1)), Internal::windowFromUpval(L, 2)));
}

static int dsp_clearTags(lua_State* L) {
    return Internal::checkResult(L, CA::clearTags(Internal::windowFromUpval(L, 1)));
}

static int dsp_toggleSwallow(lua_State* L) {
    return Internal::checkResult(L, CA::toggleSwallow());
}

static int dsp_resize(lua_State* L) {
    Vector2D value{lua_tonumber(L, lua_upvalueindex(1)), lua_tonumber(L, lua_upvalueindex(2))};
    return Internal::checkResult(L, CA::resize(value, lua_toboolean(L, lua_upvalueindex(3)), Internal::windowFromUpval(L, 4)));
}

static int dsp_move(lua_State* L) {
    Vector2D value{lua_tonumber(L, lua_upvalueindex(1)), lua_tonumber(L, lua_upvalueindex(2))};
    return Internal::checkResult(L, CA::move(value, lua_toboolean(L, lua_upvalueindex(3)), Internal::windowFromUpval(L, 4)));
}

static int dsp_pinWindow(lua_State* L) {
    return Internal::checkResult(L, CA::pinWindow(sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1))), Internal::windowFromUpval(L, 2)));
}

static int dsp_bringToTop(lua_State* L) {
    return Internal::checkResult(L, CA::alterZOrder("top"));
}

static int dsp_alterZOrder(lua_State* L) {
    return Internal::checkResult(L, CA::alterZOrder(lua_tostring(L, lua_upvalueindex(1)), Internal::windowFromUpval(L, 2)));
}

static int dsp_setProp(lua_State* L) {
    return Internal::checkResult(L, CA::setProp(lua_tostring(L, lua_upvalueindex(1)), lua_tostring(L, lua_upvalueindex(2)), Internal::windowFromUpval(L, 3)));
}

static int dsp_moveIntoGroup(lua_State* L) {
    return Internal::checkResult(L, CA::moveIntoGroup(sc<Math::eDirection>((int)lua_tonumber(L, lua_upvalueindex(1))), Internal::windowFromUpval(L, 2)));
}

static int dsp_moveOutOfGroup(lua_State* L) {
    return Internal::checkResult(L, CA::moveOutOfGroup(sc<Math::eDirection>((int)lua_tonumber(L, lua_upvalueindex(1))), Internal::windowFromUpval(L, 2)));
}

static int dsp_moveWindowOrGroup(lua_State* L) {
    return Internal::checkResult(L, CA::moveWindowOrGroup(sc<Math::eDirection>((int)lua_tonumber(L, lua_upvalueindex(1))), Internal::windowFromUpval(L, 2)));
}

static int dsp_moveIntoOrCreateGroup(lua_State* L) {
    return Internal::checkResult(L, CA::moveIntoOrCreateGroup(sc<Math::eDirection>((int)lua_tonumber(L, lua_upvalueindex(1))), Internal::windowFromUpval(L, 2)));
}

static int dsp_denyFromGroup(lua_State* L) {
    return Internal::checkResult(L, CA::denyWindowFromGroup(sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1)))));
}

static int dsp_mouseDrag(lua_State* L) {
    return Internal::checkResult(L, CA::mouse("movewindow"));
}

static int dsp_mouseResize(lua_State* L) {
    return Internal::checkResult(L, CA::mouse("resizewindow"));
}

static int hlWindowClose(lua_State* L) {
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_closeWindow, 1);
    return 1;
}

static int hlWindowKill(lua_State* L) {
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_killWindow, 1);
    return 1;
}

static int hlWindowSignal(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.window.signal: expected a table { signal, window? }");

    lua_pushnumber(L, Internal::requireTableFieldNum(L, 1, "signal", "hl.window.signal"));
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_signalWindow, 2);
    return 1;
}

static int hlWindowFloat(lua_State* L) {
    const auto action = Internal::tableToggleAction(L, 1);

    lua_pushnumber(L, (int)action);
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_floatWindow, 2);
    return 1;
}

static int hlWindowFullscreen(lua_State* L) {
    eFullscreenMode mode   = FSMODE_FULLSCREEN;
    int             action = 0; // 0: toggle, 1: set, 2: unset
    if (lua_istable(L, 1)) {
        auto m = Internal::tableOptStr(L, 1, "mode");
        if (m) {
            if (*m == "maximized" || *m == "1")
                mode = FSMODE_MAXIMIZED;
            else if (*m == "fullscreen" || *m == "0")
                mode = FSMODE_FULLSCREEN;
            else
                return Internal::configError(L, "hl.window.fullscreen: invalid mode \"{}\" (expected fullscreen/maximized)", *m);
        }

        auto a = Internal::tableOptStr(L, 1, "action");
        if (a) {
            if (*a == "toggle")
                action = 0;
            else if (*a == "set")
                action = 1;
            else if (*a == "unset")
                action = 2;
            else
                return Internal::configError(L, "hl.window.fullscreen: invalid action \"{}\" (expected toggle/set/unset)", *a);
        }
    }
    lua_pushnumber(L, (int)mode);
    if (action == 0) {
        Internal::pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_fullscreenWindow, 2);
    } else {
        lua_pushnumber(L, action);
        Internal::pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_fullscreenWindowWithAction, 3);
    }
    return 1;
}

static int hlWindowFullscreenState(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.window.fullscreen_state: expected a table { internal, client, action?, window? }");

    int action = 1; // default to set semantics
    if (auto a = Internal::tableOptStr(L, 1, "action"); a) {
        if (*a == "toggle")
            action = 0;
        else if (*a == "set")
            action = 1;
        else if (*a == "unset")
            action = 2;
        else
            return Internal::configError(L, "hl.window.fullscreen_state: invalid action \"{}\" (expected toggle/set/unset)", *a);
    }

    auto im = Internal::tableOptNum(L, 1, "internal");
    auto cm = Internal::tableOptNum(L, 1, "client");
    if (!im || !cm)
        return Internal::configError(L, "hl.window.fullscreen_state: 'internal' and 'client' are required");

    lua_pushnumber(L, (int)*im);
    lua_pushnumber(L, (int)*cm);
    lua_pushnumber(L, action);
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_fullscreenState, 4);
    return 1;
}

static int hlWindowPseudo(lua_State* L) {
    const auto action = Internal::tableToggleAction(L, 1);

    lua_pushnumber(L, (int)action);
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_pseudoWindow, 2);
    return 1;
}

static int hlWindowMove(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.window.move: expected a table, e.g. { direction = \"left\" }");

    auto dirStr = Internal::tableOptStr(L, 1, "direction");
    if (dirStr) {
        auto dir = Internal::parseDirectionStr(*dirStr);
        if (dir == Math::DIRECTION_DEFAULT)
            return Internal::configError(L, "hl.window.move: invalid direction \"{}\" (expected left/right/up/down)", *dirStr);

        auto groupAware = Internal::tableOptBool(L, 1, "group_aware");
        if (groupAware && *groupAware) {
            lua_pushnumber(L, (int)dir);
            Internal::pushWindowUpval(L, 1);
            lua_pushcclosure(L, dsp_moveWindowOrGroup, 2);
            return 1;
        }

        lua_pushnumber(L, (int)dir);
        Internal::pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_moveInDirection, 2);
        return 1;
    }

    auto x = Internal::tableOptNum(L, 1, "x");
    auto y = Internal::tableOptNum(L, 1, "y");
    if (x && y) {
        bool relative = Internal::tableOptBool(L, 1, "relative").value_or(false);
        lua_pushnumber(L, *x);
        lua_pushnumber(L, *y);
        lua_pushboolean(L, relative);
        Internal::pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_move, 4);
        return 1;
    }

    auto ws = Internal::tableOptWorkspaceSelector(L, 1, "workspace", "hl.window.move");
    if (ws) {
        auto follow = Internal::tableOptBool(L, 1, "follow");
        bool silent = follow.has_value() && !*follow;
        lua_pushstring(L, ws->c_str());
        lua_pushboolean(L, silent);
        Internal::pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_moveToWorkspace, 3);
        return 1;
    }

    auto mon = Internal::tableOptMonitorSelector(L, 1, "monitor", "hl.window.move");
    if (mon) {
        auto follow = Internal::tableOptBool(L, 1, "follow");
        bool silent = follow.has_value() && !*follow;
        lua_pushstring(L, mon->c_str());
        lua_pushboolean(L, silent);
        Internal::pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_moveToMonitor, 3);
        return 1;
    }

    auto intoGroup = Internal::tableOptStr(L, 1, "into_group");
    if (intoGroup) {
        auto dir = Internal::parseDirectionStr(*intoGroup);
        if (dir == Math::DIRECTION_DEFAULT)
            return Internal::configError(L, "hl.window.move: invalid into_group direction \"{}\"", *intoGroup);
        lua_pushnumber(L, (int)dir);
        Internal::pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_moveIntoGroup, 2);
        return 1;
    }

    auto intoOrCreateGroup = Internal::tableOptStr(L, 1, "into_or_create_group");
    if (intoOrCreateGroup) {
        auto dir = Internal::parseDirectionStr(*intoOrCreateGroup);
        if (dir == Math::DIRECTION_DEFAULT)
            return Internal::configError(L, "hl.window.move: invalid into_or_create_group direction \"{}\"", *intoOrCreateGroup);
        lua_pushnumber(L, (int)dir);
        Internal::pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_moveIntoOrCreateGroup, 2);
        return 1;
    }

    auto outOfGroup = Internal::tableOptStr(L, 1, "out_of_group");
    if (outOfGroup) {
        auto dir = Internal::parseDirectionStr(*outOfGroup);
        lua_pushnumber(L, (int)dir);
        Internal::pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_moveOutOfGroup, 2);
        return 1;
    }

    auto outOfGroupBool = Internal::tableOptBool(L, 1, "out_of_group");
    if (outOfGroupBool && *outOfGroupBool) {
        lua_pushnumber(L, (int)Math::DIRECTION_DEFAULT);
        Internal::pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_moveOutOfGroup, 2);
        return 1;
    }

    return Internal::configError(L, "hl.window.move: unrecognized arguments. Expected one of: direction, x+y(+relative), workspace, into_group, out_of_group");
}

static int hlWindowSwap(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.window.swap: expected a table, e.g. { direction = \"left\" }");

    auto dirStr = Internal::tableOptStr(L, 1, "direction");
    if (dirStr) {
        auto dir = Internal::parseDirectionStr(*dirStr);
        if (dir == Math::DIRECTION_DEFAULT)
            return Internal::configError(L, "hl.window.swap: invalid direction \"{}\" (expected left/right/up/down)", *dirStr);
        lua_pushnumber(L, (int)dir);
        Internal::pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_swapInDirection, 2);
        return 1;
    }

    auto target = Internal::tableOptWindowSelector(L, 1, "target", "hl.window.swap");
    if (!target)
        target = Internal::tableOptWindowSelector(L, 1, "with", "hl.window.swap");
    if (!target)
        target = Internal::tableOptWindowSelector(L, 1, "other", "hl.window.swap");

    if (target) {
        Internal::pushWindowUpval(L, 1);
        lua_pushstring(L, target->c_str());
        lua_pushcclosure(L, dsp_swapWithWindow, 2);
        return 1;
    }

    auto next = Internal::tableOptBool(L, 1, "next");
    if (next && *next) {
        lua_pushboolean(L, true);
        Internal::pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_swapNext, 2);
        return 1;
    }

    auto prev = Internal::tableOptBool(L, 1, "prev");
    if (prev && *prev) {
        lua_pushboolean(L, false);
        Internal::pushWindowUpval(L, 1);
        lua_pushcclosure(L, dsp_swapNext, 2);
        return 1;
    }

    return Internal::configError(L, "hl.window.swap: unrecognized arguments. Expected one of: direction, target/with/other, next, prev");
}

static int hlWindowCenter(lua_State* L) {
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_center, 1);
    return 1;
}

static int hlWindowCycleNext(lua_State* L) {
    bool next     = true;
    int  tiled    = -1;
    int  floating = -1;
    if (lua_istable(L, 1)) {
        auto n = Internal::tableOptBool(L, 1, "next");
        if (n)
            next = *n;
        auto t = Internal::tableOptBool(L, 1, "tiled");
        if (t)
            tiled = *t ? 1 : 0;
        auto f = Internal::tableOptBool(L, 1, "floating");
        if (f)
            floating = *f ? 1 : 0;
    }
    lua_pushboolean(L, next);
    lua_pushnumber(L, tiled);
    lua_pushnumber(L, floating);
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_cycleNext, 4);
    return 1;
}

static int hlWindowTag(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.window.tag: expected a table { tag, window? }");

    const auto tag = Internal::requireTableFieldStr(L, 1, "tag", "hl.window.tag");
    lua_pushstring(L, tag.c_str());
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_tagWindow, 2);
    return 1;
}

static int hlWindowClearTags(lua_State* L) {
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_clearTags, 1);
    return 1;
}

static int hlWindowToggleSwallow(lua_State* L) {
    lua_pushcclosure(L, dsp_toggleSwallow, 0);
    return 1;
}

static int hlWindowResizeExact(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.window.resize: expected a table { x, y, relative?, window? }");
    auto x        = Internal::tableOptNum(L, 1, "x");
    auto y        = Internal::tableOptNum(L, 1, "y");
    bool relative = Internal::tableOptBool(L, 1, "relative").value_or(false);
    if (!x || !y)
        return Internal::configError(L, "hl.window.resize: 'x' and 'y' are required");
    lua_pushnumber(L, *x);
    lua_pushnumber(L, *y);
    lua_pushboolean(L, relative);
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_resize, 4);
    return 1;
}

static int hlWindowPin(lua_State* L) {
    const auto action = Internal::tableToggleAction(L, 1);

    lua_pushnumber(L, (int)action);
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_pinWindow, 2);
    return 1;
}

static int hlWindowBringToTop(lua_State* L) {
    lua_pushcclosure(L, dsp_bringToTop, 0);
    return 1;
}

static int hlWindowAlterZOrder(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.window.alter_zorder: expected a table { mode, window? }");

    const auto mode = Internal::requireTableFieldStr(L, 1, "mode", "hl.window.alter_zorder");
    lua_pushstring(L, mode.c_str());
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_alterZOrder, 2);
    return 1;
}

static int hlWindowSetProp(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.window.set_prop: expected a table { prop, value, window? }");

    const auto prop  = Internal::requireTableFieldStr(L, 1, "prop", "hl.window.set_prop");
    const auto value = Internal::requireTableFieldStr(L, 1, "value", "hl.window.set_prop");
    lua_pushstring(L, prop.c_str());
    lua_pushstring(L, value.c_str());
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_setProp, 3);
    return 1;
}

static int hlWindowDenyFromGroup(lua_State* L) {
    const auto action = Internal::tableToggleAction(L, 1);

    lua_pushnumber(L, (int)action);
    lua_pushcclosure(L, dsp_denyFromGroup, 1);
    return 1;
}

static int hlWindowDrag(lua_State* L) {
    lua_pushcclosure(L, dsp_mouseDrag, 0);
    return 1;
}

static int hlWindowResize(lua_State* L) {
    if (lua_gettop(L) == 0 || lua_isnil(L, 1)) {
        lua_pushcclosure(L, dsp_mouseResize, 0);
        return 1;
    }

    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.window.resize: expected no args, or a table { x, y, relative?, window? }");

    return hlWindowResizeExact(L);
}

static int dsp_moveFocus(lua_State* L) {
    return Internal::checkResult(L, CA::moveFocus(sc<Math::eDirection>((int)lua_tonumber(L, lua_upvalueindex(1)))));
}

static int dsp_focusMonitor(lua_State* L) {
    const auto PMONITOR = g_pCompositor->getMonitorFromString(lua_tostring(L, lua_upvalueindex(1)));
    if (!PMONITOR)
        return Internal::dispatcherError(L, "hl.focus.monitor: monitor not found", WARN, C_NOTFOUND);
    return Internal::checkResult(L, CA::focusMonitor(PMONITOR));
}

static int dsp_focusWindowBySelector(lua_State* L) {
    const auto PWINDOW = g_pCompositor->getWindowByRegex(lua_tostring(L, lua_upvalueindex(1)));
    if (!PWINDOW)
        return Internal::dispatcherError(L, "hl.focus: window not found", WARN, C_NOTFOUND);
    return Internal::checkResult(L, CA::focus(PWINDOW));
}

static int dsp_focusUrgentOrLast(lua_State* L) {
    return Internal::checkResult(L, CA::focusUrgentOrLast());
}

static int dsp_focusCurrentOrLast(lua_State* L) {
    return Internal::checkResult(L, CA::focusCurrentOrLast());
}

static int dsp_changeWorkspace(lua_State* L) {
    return Internal::checkResult(L, CA::changeWorkspace(std::string(lua_tostring(L, lua_upvalueindex(1)))));
}

static int dsp_focusWorkspaceOnCurrentMonitor(lua_State* L) {
    auto ws = Internal::resolveWorkspaceStr(lua_tostring(L, lua_upvalueindex(1)));
    if (!ws)
        return Internal::dispatcherError(L, "Invalid workspace", ERR, C_INVARG);
    return Internal::checkResult(L, CA::changeWorkspaceOnCurrentMonitor(ws));
}

static int hlFocus(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.focus: expected a table, e.g. { direction = \"left\" }");

    auto dirStr = Internal::tableOptStr(L, 1, "direction");
    if (dirStr) {
        auto dir = Internal::parseDirectionStr(*dirStr);
        if (dir == Math::DIRECTION_DEFAULT)
            return Internal::configError(L, "hl.focus: invalid direction \"{}\" (expected left/right/up/down)", *dirStr);
        lua_pushnumber(L, (int)dir);
        lua_pushcclosure(L, dsp_moveFocus, 1);
        return 1;
    }

    auto monStr = Internal::tableOptMonitorSelector(L, 1, "monitor", "hl.focus");
    if (monStr) {
        lua_pushstring(L, monStr->c_str());
        lua_pushcclosure(L, dsp_focusMonitor, 1);
        return 1;
    }

    auto wsStr = Internal::tableOptWorkspaceSelector(L, 1, "workspace", "hl.focus");
    if (wsStr) {
        auto onCurrentMon = Internal::tableOptBool(L, 1, "on_current_monitor");

        lua_pushstring(L, wsStr->c_str());

        if (onCurrentMon.value_or(false))
            lua_pushcclosure(L, dsp_focusWorkspaceOnCurrentMonitor, 1);
        else
            lua_pushcclosure(L, dsp_changeWorkspace, 1);

        return 1;
    }

    auto winStr = Internal::tableOptWindowSelector(L, 1, "window", "hl.focus");
    if (winStr) {
        lua_pushstring(L, winStr->c_str());
        lua_pushcclosure(L, dsp_focusWindowBySelector, 1);
        return 1;
    }

    auto urgent = Internal::tableOptBool(L, 1, "urgent_or_last");
    if (urgent && *urgent) {
        lua_pushcclosure(L, dsp_focusUrgentOrLast, 0);
        return 1;
    }

    auto last = Internal::tableOptBool(L, 1, "last");
    if (last && *last) {
        lua_pushcclosure(L, dsp_focusCurrentOrLast, 0);
        return 1;
    }

    return Internal::configError(L, "hl.focus: unrecognized arguments. Expected one of: direction, monitor, window, urgent_or_last, last");
}

static int dsp_noop(lua_State* L) {
    return 0;
}

static int hlNoop(lua_State* L) {
    lua_pushcclosure(L, dsp_noop, 0);
    return 1;
}

static int dsp_toggleSpecial(lua_State* L) {
    std::string name                                   = lua_isnil(L, lua_upvalueindex(1)) ? "" : lua_tostring(L, lua_upvalueindex(1));
    const auto& [workspaceID, workspaceName, isAutoID] = getWorkspaceIDNameFromString("special:" + name);
    if (workspaceID == WORKSPACE_INVALID || !g_pCompositor->isWorkspaceSpecial(workspaceID))
        return Internal::dispatcherError(L, "Invalid special workspace", ERR, C_INVARG);

    auto ws = g_pCompositor->getWorkspaceByID(workspaceID);
    if (!ws) {
        const auto PMONITOR = Desktop::focusState()->monitor();
        if (PMONITOR)
            ws = g_pCompositor->createNewWorkspace(workspaceID, PMONITOR->m_id, workspaceName);
    }
    if (!ws)
        return Internal::dispatcherError(L, "Could not resolve special workspace", ERR, C_UNAVAIL);

    return Internal::checkResult(L, CA::toggleSpecial(ws));
}

static int dsp_renameWorkspace(lua_State* L) {
    const auto PWS = g_pCompositor->getWorkspaceByString(lua_tostring(L, lua_upvalueindex(1)));
    if (!PWS)
        return Internal::dispatcherError(L, "hl.workspace.rename: no such workspace", WARN, C_NOTFOUND);
    std::string name = lua_isnil(L, lua_upvalueindex(2)) ? "" : lua_tostring(L, lua_upvalueindex(2));
    return Internal::checkResult(L, CA::renameWorkspace(PWS, name));
}

static int dsp_moveWorkspaceToMonitor(lua_State* L) {
    const auto WORKSPACEID = getWorkspaceIDNameFromString(lua_tostring(L, lua_upvalueindex(1))).id;
    if (WORKSPACEID == WORKSPACE_INVALID)
        return Internal::dispatcherError(L, "Invalid workspace", ERR, C_INVARG);
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(WORKSPACEID);
    if (!PWORKSPACE)
        return Internal::dispatcherError(L, "Workspace not found", WARN, C_NOTFOUND);
    const auto PMONITOR = g_pCompositor->getMonitorFromString(lua_tostring(L, lua_upvalueindex(2)));
    if (!PMONITOR)
        return Internal::dispatcherError(L, "Monitor not found", WARN, C_NOTFOUND);
    return Internal::checkResult(L, CA::moveToMonitor(PWORKSPACE, PMONITOR));
}

static int dsp_moveCurrentWorkspaceToMonitor(lua_State* L) {
    const auto PMONITOR = g_pCompositor->getMonitorFromString(lua_tostring(L, lua_upvalueindex(1)));
    if (!PMONITOR)
        return Internal::dispatcherError(L, "Monitor not found", WARN, C_NOTFOUND);
    const auto PCURRENTWORKSPACE = Desktop::focusState()->monitor()->m_activeWorkspace;
    if (!PCURRENTWORKSPACE)
        return Internal::dispatcherError(L, "Invalid workspace", ERR, C_INVARG);
    return Internal::checkResult(L, CA::moveToMonitor(PCURRENTWORKSPACE, PMONITOR));
}

static int dsp_swapActiveWorkspaces(lua_State* L) {
    const auto PMON1 = g_pCompositor->getMonitorFromString(lua_tostring(L, lua_upvalueindex(1)));
    const auto PMON2 = g_pCompositor->getMonitorFromString(lua_tostring(L, lua_upvalueindex(2)));
    if (!PMON1 || !PMON2)
        return Internal::dispatcherError(L, "Monitor not found", WARN, C_NOTFOUND);
    return Internal::checkResult(L, CA::swapActiveWorkspaces(PMON1, PMON2));
}

static int hlWorkspaceToggleSpecial(lua_State* L) {
    lua_pushstring(L, lua_tostring(L, 1));
    lua_pushcclosure(L, dsp_toggleSpecial, 1);
    return 1;
}

static int hlWorkspaceRename(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.workspace.rename: expected a table { workspace, name? }");

    const auto id   = Internal::requireTableFieldWorkspaceSelector(L, 1, "workspace", "hl.workspace.rename");
    auto       name = Internal::tableOptStr(L, 1, "name");

    lua_pushstring(L, id.c_str());
    if (name)
        lua_pushstring(L, name->c_str());
    else
        lua_pushnil(L);
    lua_pushcclosure(L, dsp_renameWorkspace, 2);
    return 1;
}

static int hlWorkspaceMove(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.workspace.move: expected a table, e.g. { monitor = \"DP-1\" }");

    const auto mon = Internal::requireTableFieldMonitorSelector(L, 1, "monitor", "hl.workspace.move");

    auto       id = Internal::tableOptWorkspaceSelector(L, 1, "workspace", "hl.workspace.move");
    if (id) {
        lua_pushstring(L, id->c_str());
        lua_pushstring(L, mon.c_str());
        lua_pushcclosure(L, dsp_moveWorkspaceToMonitor, 2);
        return 1;
    }

    lua_pushstring(L, mon.c_str());
    lua_pushcclosure(L, dsp_moveCurrentWorkspaceToMonitor, 1);
    return 1;
}

static int hlWorkspaceSwapMonitors(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.workspace.swap_monitors: expected a table { monitor1, monitor2 }");

    const auto m1 = Internal::requireTableFieldMonitorSelector(L, 1, "monitor1", "hl.workspace.swap_monitors");
    const auto m2 = Internal::requireTableFieldMonitorSelector(L, 1, "monitor2", "hl.workspace.swap_monitors");
    lua_pushstring(L, m1.c_str());
    lua_pushstring(L, m2.c_str());
    lua_pushcclosure(L, dsp_swapActiveWorkspaces, 2);
    return 1;
}

void Internal::registerDispatcherBindings(lua_State* L) {
    lua_newtable(L);
    Internal::markDispatcherTable(L);

    {
        lua_newtable(L);
        Internal::markDispatcherTable(L);
        Internal::setFn(L, "move_to_corner", hlCursorMoveToCorner);
        Internal::setFn(L, "move", hlCursorMove);
        lua_setfield(L, -2, "cursor");

        lua_newtable(L);
        Internal::markDispatcherTable(L);
        Internal::setFn(L, "toggle", hlGroupToggle);
        Internal::setFn(L, "next", hlGroupNext);
        Internal::setFn(L, "prev", hlGroupPrev);
        Internal::setFn(L, "active", hlGroupActive);
        Internal::setFn(L, "move_window", hlGroupMoveWindow);
        Internal::setFn(L, "lock", hlGroupLock);
        Internal::setFn(L, "lock_active", hlGroupLockActive);
        lua_setfield(L, -2, "group");

        lua_newtable(L);
        Internal::markDispatcherTable(L);
        Internal::setFn(L, "close", hlWindowClose);
        Internal::setFn(L, "kill", hlWindowKill);
        Internal::setFn(L, "signal", hlWindowSignal);
        Internal::setFn(L, "float", hlWindowFloat);
        Internal::setFn(L, "fullscreen", hlWindowFullscreen);
        Internal::setFn(L, "fullscreen_state", hlWindowFullscreenState);
        Internal::setFn(L, "pseudo", hlWindowPseudo);
        Internal::setFn(L, "move", hlWindowMove);
        Internal::setFn(L, "swap", hlWindowSwap);
        Internal::setFn(L, "center", hlWindowCenter);
        Internal::setFn(L, "cycle_next", hlWindowCycleNext);
        Internal::setFn(L, "tag", hlWindowTag);
        Internal::setFn(L, "clear_tags", hlWindowClearTags);
        Internal::setFn(L, "toggle_swallow", hlWindowToggleSwallow);
        Internal::setFn(L, "pin", hlWindowPin);
        Internal::setFn(L, "bring_to_top", hlWindowBringToTop);
        Internal::setFn(L, "alter_zorder", hlWindowAlterZOrder);
        Internal::setFn(L, "set_prop", hlWindowSetProp);
        Internal::setFn(L, "deny_from_group", hlWindowDenyFromGroup);
        Internal::setFn(L, "drag", hlWindowDrag);
        Internal::setFn(L, "resize", hlWindowResize);
        lua_setfield(L, -2, "window");

        lua_newtable(L);
        Internal::markDispatcherTable(L);
        Internal::setFn(L, "rename", hlWorkspaceRename);
        Internal::setFn(L, "move", hlWorkspaceMove);
        Internal::setFn(L, "swap_monitors", hlWorkspaceSwapMonitors);
        Internal::setFn(L, "toggle_special", hlWorkspaceToggleSpecial);
        lua_setfield(L, -2, "workspace");

        Internal::setFn(L, "exec_cmd", hlExecCmd);
        Internal::setFn(L, "exec_raw", hlExecRaw);
        Internal::setFn(L, "exit", hlExit);
        Internal::setFn(L, "submap", hlSubmap);
        Internal::setFn(L, "pass", hlPass);
        Internal::setFn(L, "send_shortcut", hlSendShortcut);
        Internal::setFn(L, "send_key_state", hlSendKeyState);
        Internal::setFn(L, "layout", hlLayout);
        Internal::setFn(L, "dpms", hlDpms);
        Internal::setFn(L, "event", hlEvent);
        Internal::setFn(L, "global", hlGlobal);
        Internal::setFn(L, "force_renderer_reload", hlForceRendererReload);
        Internal::setFn(L, "force_idle", hlForceIdle);
        Internal::setFn(L, "focus", hlFocus);
        Internal::setFn(L, "no_op", hlNoop);
    }

    lua_setfield(L, -2, "dsp");
}
