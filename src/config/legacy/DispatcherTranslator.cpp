#include "DispatcherTranslator.hpp"

#include "../shared/actions/ConfigActions.hpp"
#include "../supplementary/executor/Executor.hpp"
#include "../../Compositor.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../helpers/Monitor.hpp"
#include "../../desktop/view/Group.hpp"
#include "../../managers/KeybindManager.hpp"
#include "../../managers/SeatManager.hpp"
#include "../../managers/input/InputManager.hpp"
#include "../../layout/LayoutManager.hpp"

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList2.hpp>
using namespace Hyprutils::String;

using namespace Config;
using namespace Config::Legacy;
using namespace Config::Actions;

UP<CDispatcherTranslator>& Legacy::translator() {
    static UP<CDispatcherTranslator> p = makeUnique<CDispatcherTranslator>();
    return p;
}

SDispatchResult CDispatcherTranslator::run(const std::string& d, const std::string& w) {
    if (!m_dispMap.contains(d))
        return {.success = false, .error = "Bad dispatcher"};

    return m_dispMap.at(d)(w);
}

// helper: convert ActionResult to SDispatchResult
static SDispatchResult wrap(ActionResult res) {
    if (!res)
        return {.success = false, .error = res.error()};
    return {.passEvent = res->passEvent};
}

// helper: resolve window from regex string, or focused if empty/active
static PHLWINDOW windowFromArg(const std::string& arg) {
    if (arg.empty() || arg == "active")
        return nullptr; // will use xtract(nullopt) -> focused window
    return g_pCompositor->getWindowByRegex(arg);
}

// helper: resolve workspace from string and optionally create it
static PHLWORKSPACE resolveWorkspace(const std::string& args) {
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

static SDispatchResult exec(const std::string& args) {
    const auto PROC = Config::Supplementary::executor()->spawn(args);
    if (!PROC.has_value())
        return {.success = false, .error = std::format("Failed to start process. No closing bracket in exec rule. {}", args)};
    return {.success = PROC.value() > 0, .error = std::format("Failed to start process {}", args)};
}

static SDispatchResult execr(const std::string& args) {
    const auto PROC = Config::Supplementary::executor()->spawnRaw(args);
    return {.success = PROC && *PROC > 0, .error = std::format("Failed to start process {}", args)};
}

static SDispatchResult killactive(const std::string&) {
    return wrap(Actions::closeWindow());
}

static SDispatchResult forcekillactive(const std::string&) {
    return wrap(Actions::killWindow());
}

static SDispatchResult closewindow(const std::string& data) {
    return wrap(Actions::closeWindow(g_pCompositor->getWindowByRegex(data)));
}

static SDispatchResult killwindow(const std::string& data) {
    return wrap(Actions::killWindow(g_pCompositor->getWindowByRegex(data)));
}

static SDispatchResult signalactive(const std::string& args) {
    if (!isNumber(args))
        return {.success = false, .error = "signalActive: signal has to be int"};
    try {
        return wrap(Actions::signalWindow(std::stoi(args)));
    } catch (...) { return {.success = false, .error = "signalActive: invalid signal format"}; }
}

static SDispatchResult signalwindow(const std::string& args) {
    const auto WINDOWREGEX = args.substr(0, args.find_first_of(','));
    const auto SIGNAL      = args.substr(args.find_first_of(',') + 1);

    const auto PWINDOW = g_pCompositor->getWindowByRegex(WINDOWREGEX);
    if (!PWINDOW)
        return {.success = false, .error = "signalWindow: no window"};

    if (!isNumber(SIGNAL))
        return {.success = false, .error = "signalWindow: signal has to be int"};

    try {
        return wrap(Actions::signalWindow(std::stoi(SIGNAL), PWINDOW));
    } catch (...) { return {.success = false, .error = "signalWindow: invalid signal format"}; }
}

static SDispatchResult togglefloating(const std::string& args) {
    auto w = windowFromArg(args);
    return wrap(Actions::floatWindow(TOGGLE_ACTION_TOGGLE, w));
}

static SDispatchResult setfloating(const std::string& args) {
    auto w = windowFromArg(args);
    return wrap(Actions::floatWindow(TOGGLE_ACTION_ENABLE, w));
}

static SDispatchResult settiled(const std::string& args) {
    auto w = windowFromArg(args);
    return wrap(Actions::floatWindow(TOGGLE_ACTION_DISABLE, w));
}

static SDispatchResult pseudo(const std::string& args) {
    auto w = windowFromArg(args);
    return wrap(Actions::pseudoWindow(TOGGLE_ACTION_TOGGLE, w));
}

static SDispatchResult workspace(const std::string& args) {
    return wrap(Actions::changeWorkspace(args));
}

static SDispatchResult renameworkspace(const std::string& args) {
    try {
        const auto FIRSTSPACEPOS = args.find_first_of(' ');
        if (FIRSTSPACEPOS != std::string::npos) {
            int         wsid = std::stoi(args.substr(0, FIRSTSPACEPOS));
            std::string name = args.substr(FIRSTSPACEPOS + 1);
            const auto  PWS  = g_pCompositor->getWorkspaceByID(wsid);
            if (!PWS)
                return {.success = false, .error = "No such workspace"};
            return wrap(Actions::renameWorkspace(PWS, name));
        } else {
            const auto PWS = g_pCompositor->getWorkspaceByID(std::stoi(args));
            if (!PWS)
                return {.success = false, .error = "No such workspace"};
            return wrap(Actions::renameWorkspace(PWS, ""));
        }
    } catch (std::exception& e) { return {.success = false, .error = std::format("Invalid arg in renameWorkspace: {}", e.what())}; }
}

static SDispatchResult fullscreen(const std::string& args) {
    CVarList2             ARGS(args, 2, ' ');

    const eFullscreenMode MODE = ARGS.size() > 0 && ARGS[0] == "1" ? FSMODE_MAXIMIZED : FSMODE_FULLSCREEN;

    if (ARGS.size() <= 1 || ARGS[1] == "toggle")
        return wrap(Actions::fullscreenWindow(MODE));

    // "set" means enable, "unset" means disable - but the Action toggles.
    // We need to check current state ourselves.
    const auto PWINDOW = Desktop::focusState()->window();
    if (!PWINDOW)
        return {.success = false, .error = "Window not found"};

    if (ARGS[1] == "set") {
        if (!PWINDOW->isEffectiveInternalFSMode(MODE))
            return wrap(Actions::fullscreenWindow(MODE));
        return {};
    } else if (ARGS[1] == "unset") {
        if (PWINDOW->isEffectiveInternalFSMode(MODE))
            return wrap(Actions::fullscreenWindow(MODE));
        return {};
    }

    return {};
}

static SDispatchResult fullscreenstate(const std::string& args) {
    CVarList2  ARGS(args, 3, ' ');

    const auto PWINDOW = Desktop::focusState()->window();
    if (!PWINDOW)
        return {.success = false, .error = "Window not found"};

    int internalMode, clientMode;
    try {
        internalMode = std::stoi(std::string(ARGS[0]));
    } catch (...) { internalMode = -1; }
    try {
        clientMode = std::stoi(std::string(ARGS[1]));
    } catch (...) { clientMode = -1; }

    eFullscreenMode im = internalMode != -1 ? sc<eFullscreenMode>(internalMode) : PWINDOW->m_fullscreenState.internal;
    eFullscreenMode cm = clientMode != -1 ? sc<eFullscreenMode>(clientMode) : PWINDOW->m_fullscreenState.client;

    return wrap(Actions::fullscreenWindow(im, cm));
}

static SDispatchResult movetoworkspace(const std::string& args) {
    PHLWINDOW   PWINDOW = nullptr;
    std::string wsArgs  = args;

    if (args.contains(',')) {
        PWINDOW = g_pCompositor->getWindowByRegex(args.substr(args.find_last_of(',') + 1));
        wsArgs  = args.substr(0, args.find_last_of(','));
    }

    auto ws = resolveWorkspace(wsArgs);
    if (!ws)
        return {.success = false, .error = "Invalid workspace"};

    return wrap(Actions::moveToWorkspace(ws, false, PWINDOW));
}

static SDispatchResult movetoworkspacesilent(const std::string& args) {
    PHLWINDOW   PWINDOW = nullptr;
    std::string wsArgs  = args;

    if (args.contains(',')) {
        PWINDOW = g_pCompositor->getWindowByRegex(args.substr(args.find_last_of(',') + 1));
        wsArgs  = args.substr(0, args.find_last_of(','));
    }

    auto ws = resolveWorkspace(wsArgs);
    if (!ws)
        return {.success = false, .error = "Invalid workspace"};

    return wrap(Actions::moveToWorkspace(ws, true, PWINDOW));
}

static SDispatchResult movefocus(const std::string& args) {
    Math::eDirection dir = Math::fromChar(args[0]);
    if (dir == Math::DIRECTION_DEFAULT)
        return {.success = false, .error = std::format("Unsupported direction: {}", args[0])};
    return wrap(Actions::moveFocus(dir));
}

static SDispatchResult movewindow(const std::string& args) {
    // "movewindow" dispatcher handles both "mon:<monitor>" and directional moves.
    // For mon: prefix, it delegates to movetoworkspace.
    bool silent    = args.ends_with(" silent");
    auto cleanArgs = silent ? args.substr(0, args.length() - 7) : args;

    if (cleanArgs.starts_with("mon:")) {
        const auto PNEWMONITOR = g_pCompositor->getMonitorFromString(cleanArgs.substr(4));
        if (!PNEWMONITOR)
            return {.success = false, .error = std::format("Monitor {} not found", cleanArgs.substr(4))};

        auto ws = PNEWMONITOR->m_activeWorkspace;
        return wrap(Actions::moveToWorkspace(ws, silent));
    }

    Math::eDirection dir = Math::fromChar(cleanArgs[0]);
    if (dir == Math::DIRECTION_DEFAULT)
        return {.success = false, .error = std::format("Unsupported direction: {}", cleanArgs[0])};

    return wrap(Actions::moveInDirection(dir));
}

static SDispatchResult swapwindow(const std::string& args) {
    if (isDirection(args))
        return wrap(Actions::swapInDirection(Math::fromChar(args[0])));

    // regex-based swap: resolve window and use swapInDirection? No - the old code used getWindowByRegex + switchTargets.
    // The new Actions don't have a "swap with specific window" variant.
    // Fall through to the old swapActive logic via getWindowByRegex + layout switchTargets.
    const auto PLASTWINDOW = Desktop::focusState()->window();
    if (!PLASTWINDOW)
        return {.success = false, .error = "Window to swap with not found"};
    if (PLASTWINDOW->isFullscreen())
        return {.success = false, .error = "Can't swap fullscreen window"};

    const auto PWINDOWTOCHANGETO = g_pCompositor->getWindowByRegex(args);
    if (!PWINDOWTOCHANGETO || PWINDOWTOCHANGETO == PLASTWINDOW)
        return {.success = false, .error = std::format("Can't swap with {}, invalid window", args)};

    g_layoutManager->switchTargets(PLASTWINDOW->layoutTarget(), PWINDOWTOCHANGETO->layoutTarget(), true);
    PLASTWINDOW->warpCursor();
    return {};
}

static SDispatchResult centerwindow(const std::string&) {
    return wrap(Actions::center());
}

static SDispatchResult togglegroup(const std::string&) {
    return wrap(Actions::toggleGroup());
}

static SDispatchResult changegroupactive(const std::string& args) {
    bool forward = !(args == "b" || args == "prev");

    // index-based change
    if (isNumber(args, false)) {
        const auto PWINDOW = Desktop::focusState()->window();
        if (!PWINDOW)
            return {.success = false, .error = "No window found"};
        if (!PWINDOW->m_group)
            return {.success = false, .error = "No group"};
        if (PWINDOW->m_group->size() == 1)
            return {.success = false, .error = "Only one window in group"};
        try {
            const int INDEX = std::stoi(args);
            if (INDEX <= 0)
                PWINDOW->m_group->setCurrent(PWINDOW->m_group->size() - 1);
            else
                PWINDOW->m_group->setCurrent(INDEX - 1);
        } catch (...) { return {.success = false, .error = "invalid idx"}; }
        return {};
    }

    return wrap(Actions::changeGroupActive(forward));
}

static SDispatchResult movegroupwindow(const std::string& args) {
    return wrap(Actions::moveGroupWindow(!(args == "b" || args == "prev")));
}

static SDispatchResult focusmonitor(const std::string& args) {
    const auto PMONITOR = g_pCompositor->getMonitorFromString(args);
    if (!PMONITOR)
        return {.success = false, .error = "Monitor not found"};
    return wrap(Actions::focusMonitor(PMONITOR));
}

static SDispatchResult movecursortocorner(const std::string& args) {
    if (!isNumber(args))
        return {.success = false, .error = "moveCursorToCorner, arg has to be a number"};
    return wrap(Actions::moveCursorToCorner(std::stoi(args)));
}

static SDispatchResult movecursor(const std::string& args) {
    size_t i = args.find_first_of(' ');
    if (i == std::string::npos)
        return {.success = false, .error = "moveCursor takes 2 arguments"};

    auto x_str = args.substr(0, i);
    auto y_str = args.substr(i + 1);

    if (!isNumber(x_str) || !isNumber(y_str))
        return {.success = false, .error = "moveCursor arguments must be numbers"};

    return wrap(Actions::moveCursor({std::stoi(x_str), std::stoi(y_str)}));
}

static SDispatchResult workspaceopt(const std::string&) {
    return {.success = false, .error = "workspaceopt is deprecated"};
}

static SDispatchResult exitHyprland(const std::string&) {
    return wrap(Actions::exit());
}

static SDispatchResult movecurrentworkspacetomonitor(const std::string& args) {
    const auto PMONITOR = g_pCompositor->getMonitorFromString(args);
    if (!PMONITOR)
        return {.success = false, .error = "Monitor not found"};

    const auto PCURRENTWORKSPACE = Desktop::focusState()->monitor()->m_activeWorkspace;
    if (!PCURRENTWORKSPACE)
        return {.success = false, .error = "Invalid workspace"};

    return wrap(Actions::moveToMonitor(PCURRENTWORKSPACE, PMONITOR));
}

static SDispatchResult moveworkspacetomonitor(const std::string& args) {
    if (!args.contains(' '))
        return {.success = false, .error = "Expected: workspace monitor"};

    std::string wsStr  = args.substr(0, args.find_first_of(' '));
    std::string monStr = args.substr(args.find_first_of(' ') + 1);

    const auto  PMONITOR = g_pCompositor->getMonitorFromString(monStr);
    if (!PMONITOR)
        return {.success = false, .error = "Monitor not found"};

    const auto WORKSPACEID = getWorkspaceIDNameFromString(wsStr).id;
    if (WORKSPACEID == WORKSPACE_INVALID)
        return {.success = false, .error = "Invalid workspace"};

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(WORKSPACEID);
    if (!PWORKSPACE)
        return {.success = false, .error = "Workspace not found"};

    return wrap(Actions::moveToMonitor(PWORKSPACE, PMONITOR));
}

static SDispatchResult focusworkspaceoncurrentmonitor(const std::string& args) {
    auto ws = resolveWorkspace(args);
    if (!ws)
        return {.success = false, .error = "Invalid workspace"};
    return wrap(Actions::changeWorkspaceOnCurrentMonitor(ws));
}

static SDispatchResult togglespecialworkspace(const std::string& args) {
    const auto& [workspaceID, workspaceName, isAutoID] = getWorkspaceIDNameFromString("special:" + args);
    if (workspaceID == WORKSPACE_INVALID || !g_pCompositor->isWorkspaceSpecial(workspaceID))
        return {.success = false, .error = "Invalid special workspace"};

    auto ws = g_pCompositor->getWorkspaceByID(workspaceID);
    if (!ws) {
        const auto PMONITOR = Desktop::focusState()->monitor();
        if (PMONITOR)
            ws = g_pCompositor->createNewWorkspace(workspaceID, PMONITOR->m_id, workspaceName);
    }

    if (!ws)
        return {.success = false, .error = "Could not resolve special workspace"};

    return wrap(Actions::toggleSpecial(ws));
}

static SDispatchResult forcerendererreload(const std::string&) {
    return wrap(Actions::forceRendererReload());
}

static SDispatchResult resizeactive(const std::string& args) {
    const auto PWINDOW = Desktop::focusState()->window();
    if (!PWINDOW)
        return {.success = false, .error = "No window found"};

    const auto SIZ = g_pCompositor->parseWindowVectorArgsRelative(args, PWINDOW->m_realSize->goal());
    if (SIZ.x < 1 || SIZ.y < 1)
        return {.success = false, .error = "Invalid size"};

    return wrap(Actions::resizeBy(SIZ - PWINDOW->m_realSize->goal()));
}

static SDispatchResult moveactive(const std::string& args) {
    const auto PWINDOW = Desktop::focusState()->window();
    if (!PWINDOW)
        return {.success = false, .error = "No window found"};

    const auto POS = g_pCompositor->parseWindowVectorArgsRelative(args, PWINDOW->m_realPosition->goal());
    return wrap(Actions::moveBy(POS - PWINDOW->m_realPosition->goal()));
}

static SDispatchResult movewindowpixel(const std::string& args) {
    const auto WINDOWREGEX = args.substr(args.find_first_of(',') + 1);
    const auto MOVECMD     = args.substr(0, args.find_first_of(','));

    const auto PWINDOW = g_pCompositor->getWindowByRegex(WINDOWREGEX);
    if (!PWINDOW)
        return {.success = false, .error = "moveWindow: no window"};

    const auto POS = g_pCompositor->parseWindowVectorArgsRelative(MOVECMD, PWINDOW->m_realPosition->goal());
    return wrap(Actions::moveBy(POS - PWINDOW->m_realPosition->goal(), PWINDOW));
}

static SDispatchResult resizewindowpixel(const std::string& args) {
    const auto WINDOWREGEX = args.substr(args.find_first_of(',') + 1);
    const auto MOVECMD     = args.substr(0, args.find_first_of(','));

    const auto PWINDOW = g_pCompositor->getWindowByRegex(WINDOWREGEX);
    if (!PWINDOW)
        return {.success = false, .error = "resizeWindow: no window"};

    const auto SIZ = g_pCompositor->parseWindowVectorArgsRelative(MOVECMD, PWINDOW->m_realSize->goal());
    if (SIZ.x < 1 || SIZ.y < 1)
        return {.success = false, .error = "Invalid size"};

    return wrap(Actions::resizeBy(SIZ - PWINDOW->m_realSize->goal(), PWINDOW));
}

static SDispatchResult cyclenext(const std::string& arg) {
    CVarList2           args(arg, 0, 's', true);

    const bool          PREV = args.contains("prev") || args.contains("p") || args.contains("last") || args.contains("l");
    const bool          NEXT = args.contains("next") || args.contains("n");

    std::optional<bool> onlyTiled    = {};
    std::optional<bool> onlyFloating = {};

    if (args.contains("tile") || args.contains("tiled"))
        onlyTiled = true;
    if (args.contains("float") || args.contains("floating"))
        onlyFloating = true;

    // "hist" and "visible" modes are not mapped to the new API - they remain niche.
    // The new cycleNext uses a simple next/prev boolean.
    // PREV is default in classic alt+tab, NEXT overrides it.
    return wrap(Actions::cycleNext(NEXT || !PREV, onlyTiled, onlyFloating));
}

static SDispatchResult focuswindow(const std::string& regexp) {
    const auto PWINDOW = g_pCompositor->getWindowByRegex(regexp);
    if (!PWINDOW)
        return {.success = false, .error = "No such window found"};
    return wrap(Actions::focus(PWINDOW));
}

static SDispatchResult tagwindow(const std::string& args) {
    CVarList2 vars(args, 0, 's', true);

    PHLWINDOW PWINDOW = nullptr;
    if (vars.size() == 1)
        ; // use focused (nullptr)
    else if (vars.size() == 2)
        PWINDOW = g_pCompositor->getWindowByRegex(std::string(vars[1]));
    else
        return {.success = false, .error = "Invalid number of arguments, expected 1 or 2"};

    return wrap(Actions::tag(std::string(vars[0]), PWINDOW));
}

static SDispatchResult toggleswallow(const std::string&) {
    return wrap(Actions::toggleSwallow());
}

static SDispatchResult setsubmap(const std::string& submap) {
    return wrap(Actions::setSubmap(submap));
}

static SDispatchResult passDispatcher(const std::string& regexp) {
    const auto PWINDOW = g_pCompositor->getWindowByRegex(regexp);
    if (!PWINDOW)
        return {.success = false, .error = "pass: window not found"};
    return wrap(Actions::pass(PWINDOW));
}

static SDispatchResult sendshortcut(const std::string& args) {
    CVarList2 ARGS(args, 3);
    if (ARGS.size() != 3)
        return {.success = false, .error = "sendshortcut: invalid args"};

    const auto MOD     = g_pKeybindManager->stringToModMask(std::string(ARGS[0]));
    const auto KEY     = std::string(ARGS[1]);
    uint32_t   keycode = 0;

    if (isNumber(KEY) && std::stoi(KEY) > 9)
        keycode = std::stoi(KEY);
    else if (KEY.starts_with("code:") && isNumber(KEY.substr(5)))
        keycode = std::stoi(KEY.substr(5));
    else if (KEY.starts_with("mouse:") && isNumber(KEY.substr(6))) {
        keycode = std::stoi(KEY.substr(6));
        if (keycode < 272)
            return {.success = false, .error = "sendshortcut: invalid mouse button"};
    } else {
        // resolve keycode from key name via xkb
        const auto KEYSYM = xkb_keysym_from_name(KEY.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
        keycode           = 0;

        const auto KB = g_pSeatManager->m_keyboard;
        if (!KB)
            return {.success = false, .error = "sendshortcut: no kb"};

        const auto KEYPAIRSTRING = std::format("{}{}", rc<uintptr_t>(KB.get()), KEY);

        if (!g_pKeybindManager->m_keyToCodeCache.contains(KEYPAIRSTRING)) {
            xkb_keymap*   km          = KB->m_xkbKeymap;
            xkb_state*    ks          = KB->m_xkbState;
            xkb_keycode_t keycode_min = xkb_keymap_min_keycode(km);
            xkb_keycode_t keycode_max = xkb_keymap_max_keycode(km);

            for (xkb_keycode_t kc = keycode_min; kc <= keycode_max; ++kc) {
                xkb_keysym_t sym = xkb_state_key_get_one_sym(ks, kc);
                if (sym == KEYSYM) {
                    keycode                                            = kc;
                    g_pKeybindManager->m_keyToCodeCache[KEYPAIRSTRING] = keycode;
                }
            }

            if (!keycode)
                return {.success = false, .error = "sendshortcut: key not found"};
        } else
            keycode = g_pKeybindManager->m_keyToCodeCache[KEYPAIRSTRING];
    }

    if (!keycode)
        return {.success = false, .error = "sendshortcut: invalid key"};

    const std::string regexp  = std::string(ARGS[2]);
    PHLWINDOW         PWINDOW = regexp.empty() ? nullptr : g_pCompositor->getWindowByRegex(regexp);

    if (!regexp.empty() && !PWINDOW)
        return {.success = false, .error = "sendshortcut: window not found"};

    return wrap(Actions::pass(MOD, keycode, PWINDOW));
}

static SDispatchResult sendkeystate(const std::string& args) {
    CVarList2 ARGS(args, 4);
    if (ARGS.size() != 4)
        return {.success = false, .error = "sendkeystate: invalid args"};

    const auto STATE = ARGS[2];
    if (STATE != "down" && STATE != "repeat" && STATE != "up")
        return {.success = false, .error = "sendkeystate: invalid state, must be 'down', 'repeat', or 'up'"};

    uint32_t keyState = 0;
    if (STATE == "down")
        keyState = 1;
    else if (STATE == "repeat")
        keyState = 2;

    // Reuse sendshortcut for keycode resolution, but wrap with state
    std::string modifiedArgs = std::string(ARGS[0]) + "," + std::string(ARGS[1]) + "," + std::string(ARGS[3]);

    // We need to resolve the keycode first, so delegate to sendshortcut parsing.
    // But sendkeystate overrides m_passPressed. Let's just call through sendshortcut
    // with the proper state set.
    const int oldPassPressed = Config::Actions::state()->m_passPressed;

    if (keyState == 1 || keyState == 2)
        Config::Actions::state()->m_passPressed = 1;
    else
        Config::Actions::state()->m_passPressed = 0;

    auto result = sendshortcut(modifiedArgs);

    if (keyState == 2 && result.success)
        result = sendshortcut(modifiedArgs);

    Config::Actions::state()->m_passPressed = oldPassPressed;

    return result;
}

static SDispatchResult layoutmsg(const std::string& msg) {
    return wrap(Actions::layoutMessage(msg));
}

static SDispatchResult dpmsDispatcher(const std::string& arg) {
    eTogglableAction action;
    if (arg.starts_with("on"))
        action = TOGGLE_ACTION_ENABLE;
    else if (arg.starts_with("toggle"))
        action = TOGGLE_ACTION_TOGGLE;
    else
        action = TOGGLE_ACTION_DISABLE;

    std::optional<PHLMONITOR> mon = std::nullopt;
    if (arg.find_first_of(' ') != std::string::npos) {
        auto port = arg.substr(arg.find_first_of(' ') + 1);
        auto pMon = g_pCompositor->getMonitorFromString(port);
        if (pMon)
            mon = pMon;
    }

    return wrap(Actions::dpms(action, mon));
}

static SDispatchResult swapnext(const std::string& arg) {
    return wrap(Actions::swapNext(arg != "l" && arg != "last" && arg != "prev" && arg != "b" && arg != "back"));
}

static SDispatchResult swapactiveworkspaces(const std::string& args) {
    const auto MON1 = args.substr(0, args.find_first_of(' '));
    const auto MON2 = args.substr(args.find_first_of(' ') + 1);

    const auto PMON1 = g_pCompositor->getMonitorFromString(MON1);
    const auto PMON2 = g_pCompositor->getMonitorFromString(MON2);

    if (!PMON1 || !PMON2)
        return {.success = false, .error = "Monitor not found"};

    return wrap(Actions::swapActiveWorkspaces(PMON1, PMON2));
}

static SDispatchResult pin(const std::string& args) {
    auto w = windowFromArg(args);
    return wrap(Actions::pinWindow(TOGGLE_ACTION_TOGGLE, w));
}

static SDispatchResult mouseDispatcher(const std::string& args) {
    return wrap(Actions::mouse(args.substr(1)));
}

static SDispatchResult bringactivetotop(const std::string&) {
    return wrap(Actions::alterZOrder("top"));
}

static SDispatchResult alterzorder(const std::string& args) {
    const auto WINDOWREGEX = args.substr(args.find_first_of(',') + 1);
    const auto POSITION    = args.substr(0, args.find_first_of(','));

    auto       PWINDOW = g_pCompositor->getWindowByRegex(WINDOWREGEX);
    if (!PWINDOW && Desktop::focusState()->window() && Desktop::focusState()->window()->m_isFloating)
        PWINDOW = Desktop::focusState()->window();

    return wrap(Actions::alterZOrder(POSITION, PWINDOW));
}

static SDispatchResult focusurgentorlast(const std::string&) {
    return wrap(Actions::focusUrgentOrLast());
}

static SDispatchResult focuscurrentorlast(const std::string&) {
    return wrap(Actions::focusCurrentOrLast());
}

static SDispatchResult lockgroups(const std::string& args) {
    eTogglableAction action;
    if (args == "toggle")
        action = TOGGLE_ACTION_TOGGLE;
    else if (args == "lock" || args.empty() || args == "lockgroups")
        action = TOGGLE_ACTION_ENABLE;
    else
        action = TOGGLE_ACTION_DISABLE;
    return wrap(Actions::lockGroups(action));
}

static SDispatchResult lockactivegroup(const std::string& args) {
    eTogglableAction action;
    if (args == "toggle")
        action = TOGGLE_ACTION_TOGGLE;
    else if (args == "lock")
        action = TOGGLE_ACTION_ENABLE;
    else
        action = TOGGLE_ACTION_DISABLE;
    return wrap(Actions::lockActiveGroup(action));
}

static SDispatchResult moveintogroup(const std::string& args) {
    Math::eDirection dir = Math::fromChar(args[0]);
    if (dir == Math::DIRECTION_DEFAULT)
        return {.success = false, .error = std::format("Unsupported direction: {}", args[0])};
    return wrap(Actions::moveIntoGroup(dir));
}

static SDispatchResult moveintoorcreategroup(const std::string& args) {
    Math::eDirection dir = Math::fromChar(args[0]);
    if (dir == Math::DIRECTION_DEFAULT)
        return {.success = false, .error = std::format("Unsupported direction: {}", args[0])};
    return wrap(Actions::moveIntoOrCreateGroup(dir));
}

static SDispatchResult moveoutofgroup(const std::string& args) {
    if (args != "active" && args.length() > 1) {
        auto PWINDOW = g_pCompositor->getWindowByRegex(args);
        return wrap(Actions::moveOutOfGroup(Math::DIRECTION_DEFAULT, PWINDOW));
    }
    return wrap(Actions::moveOutOfGroup(Math::DIRECTION_DEFAULT));
}

static SDispatchResult movewindoworgroup(const std::string& args) {
    Math::eDirection dir = Math::fromChar(args[0]);
    if (dir == Math::DIRECTION_DEFAULT)
        return {.success = false, .error = std::format("Unsupported direction: {}", args[0])};
    return wrap(Actions::moveWindowOrGroup(dir));
}

static SDispatchResult denywindowfromgroup(const std::string& args) {
    eTogglableAction action;
    if (args == "toggle")
        action = TOGGLE_ACTION_TOGGLE;
    else if (args == "on")
        action = TOGGLE_ACTION_ENABLE;
    else
        action = TOGGLE_ACTION_DISABLE;
    return wrap(Actions::denyWindowFromGroup(action));
}

static SDispatchResult eventDispatcher(const std::string& args) {
    return wrap(Actions::event(args));
}

static SDispatchResult globalDispatcher(const std::string& args) {
    return wrap(Actions::global(args));
}

static SDispatchResult setprop(const std::string& args) {
    CVarList2 vars(args, 3, ' ');
    if (vars.size() < 3)
        return {.success = false, .error = "Not enough args"};

    const auto PWINDOW = g_pCompositor->getWindowByRegex(std::string(vars[0]));
    if (!PWINDOW)
        return {.success = false, .error = "Window not found"};

    // Reconstruct val from remaining args (for multi-arg values like colors)
    return wrap(Actions::setProp(std::string(vars[1]), vars.join(" ", 2), PWINDOW));
}

static SDispatchResult forceidle(const std::string& args) {
    std::optional<float> duration = getPlusMinusKeywordResult(args, 0);
    if (!duration.has_value())
        return {.success = false, .error = "Duration invalid in forceIdle"};
    return wrap(Actions::forceIdle(duration.value()));
}

CDispatcherTranslator::CDispatcherTranslator() {
    m_dispMap["exec"]                           = ::exec;
    m_dispMap["execr"]                          = ::execr;
    m_dispMap["killactive"]                     = ::killactive;
    m_dispMap["forcekillactive"]                = ::forcekillactive;
    m_dispMap["closewindow"]                    = ::closewindow;
    m_dispMap["killwindow"]                     = ::killwindow;
    m_dispMap["signal"]                         = ::signalactive;
    m_dispMap["signalwindow"]                   = ::signalwindow;
    m_dispMap["togglefloating"]                 = ::togglefloating;
    m_dispMap["setfloating"]                    = ::setfloating;
    m_dispMap["settiled"]                       = ::settiled;
    m_dispMap["workspace"]                      = ::workspace;
    m_dispMap["renameworkspace"]                = ::renameworkspace;
    m_dispMap["fullscreen"]                     = ::fullscreen;
    m_dispMap["fullscreenstate"]                = ::fullscreenstate;
    m_dispMap["movetoworkspace"]                = ::movetoworkspace;
    m_dispMap["movetoworkspacesilent"]          = ::movetoworkspacesilent;
    m_dispMap["pseudo"]                         = ::pseudo;
    m_dispMap["movefocus"]                      = ::movefocus;
    m_dispMap["movewindow"]                     = ::movewindow;
    m_dispMap["swapwindow"]                     = ::swapwindow;
    m_dispMap["centerwindow"]                   = ::centerwindow;
    m_dispMap["togglegroup"]                    = ::togglegroup;
    m_dispMap["changegroupactive"]              = ::changegroupactive;
    m_dispMap["movegroupwindow"]                = ::movegroupwindow;
    m_dispMap["focusmonitor"]                   = ::focusmonitor;
    m_dispMap["movecursortocorner"]             = ::movecursortocorner;
    m_dispMap["movecursor"]                     = ::movecursor;
    m_dispMap["workspaceopt"]                   = ::workspaceopt;
    m_dispMap["exit"]                           = ::exitHyprland;
    m_dispMap["movecurrentworkspacetomonitor"]  = ::movecurrentworkspacetomonitor;
    m_dispMap["focusworkspaceoncurrentmonitor"] = ::focusworkspaceoncurrentmonitor;
    m_dispMap["moveworkspacetomonitor"]         = ::moveworkspacetomonitor;
    m_dispMap["togglespecialworkspace"]         = ::togglespecialworkspace;
    m_dispMap["forcerendererreload"]            = ::forcerendererreload;
    m_dispMap["resizeactive"]                   = ::resizeactive;
    m_dispMap["moveactive"]                     = ::moveactive;
    m_dispMap["cyclenext"]                      = ::cyclenext;
    m_dispMap["focuswindowbyclass"]             = ::focuswindow;
    m_dispMap["focuswindow"]                    = ::focuswindow;
    m_dispMap["tagwindow"]                      = ::tagwindow;
    m_dispMap["toggleswallow"]                  = ::toggleswallow;
    m_dispMap["submap"]                         = ::setsubmap;
    m_dispMap["pass"]                           = ::passDispatcher;
    m_dispMap["sendshortcut"]                   = ::sendshortcut;
    m_dispMap["sendkeystate"]                   = ::sendkeystate;
    m_dispMap["layoutmsg"]                      = ::layoutmsg;
    m_dispMap["dpms"]                           = ::dpmsDispatcher;
    m_dispMap["movewindowpixel"]                = ::movewindowpixel;
    m_dispMap["resizewindowpixel"]              = ::resizewindowpixel;
    m_dispMap["swapnext"]                       = ::swapnext;
    m_dispMap["swapactiveworkspaces"]           = ::swapactiveworkspaces;
    m_dispMap["pin"]                            = ::pin;
    m_dispMap["mouse"]                          = ::mouseDispatcher;
    m_dispMap["bringactivetotop"]               = ::bringactivetotop;
    m_dispMap["alterzorder"]                    = ::alterzorder;
    m_dispMap["focusurgentorlast"]              = ::focusurgentorlast;
    m_dispMap["focuscurrentorlast"]             = ::focuscurrentorlast;
    m_dispMap["lockgroups"]                     = ::lockgroups;
    m_dispMap["lockactivegroup"]                = ::lockactivegroup;
    m_dispMap["moveintogroup"]                  = ::moveintogroup;
    m_dispMap["moveintoorcreategroup"]          = ::moveintoorcreategroup;
    m_dispMap["moveoutofgroup"]                 = ::moveoutofgroup;
    m_dispMap["movewindoworgroup"]              = ::movewindoworgroup;
    m_dispMap["setignoregrouplock"]             = [](const std::string&) -> SDispatchResult { return {}; }; // deprecated
    m_dispMap["denywindowfromgroup"]            = ::denywindowfromgroup;
    m_dispMap["event"]                          = ::eventDispatcher;
    m_dispMap["global"]                         = ::globalDispatcher;
    m_dispMap["setprop"]                        = ::setprop;
    m_dispMap["forceidle"]                      = ::forceidle;
}
