#include "ConfigActions.hpp"
#include "../../../desktop/state/FocusState.hpp"
#include "../../../desktop/view/Window.hpp"
#include "../../../desktop/view/Group.hpp"
#include "../../../desktop/history/WindowHistoryTracker.hpp"
#include "../../../desktop/history/WorkspaceHistoryTracker.hpp"
#include "../../../Compositor.hpp"
#include "../../../managers/SeatManager.hpp"
#include "../../../managers/PointerManager.hpp"
#include "../../../managers/EventManager.hpp"
#include "../../../managers/KeybindManager.hpp"
#include "../../../managers/input/InputManager.hpp"
#include "../../../layout/LayoutManager.hpp"
#include "../../../layout/space/Space.hpp"
#include "../../../render/Renderer.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../config/shared/monitor/MonitorRuleManager.hpp"
#include "../../../protocols/IdleNotify.hpp"
#include "../../../protocols/GlobalShortcuts.hpp"
#include "../../../event/EventBus.hpp"
#include "../../../managers/XWaylandManager.hpp"
#include "../../../layout/algorithm/Algorithm.hpp"
#include "../../../layout/algorithm/tiled/master/MasterAlgorithm.hpp"
#include "../../../layout/algorithm/tiled/monocle/MonocleAlgorithm.hpp"

#include <utility>
#include <type_traits>

using namespace Config;
using namespace Config::Actions;

UP<CActionState>& Actions::state() {
    static UP<CActionState> p = makeUnique<CActionState>();
    return p;
}

static PHLWINDOW xtract(std::optional<PHLWINDOW> window) {
    return window.value_or(Desktop::focusState()->window());
}

static void updateRelativeCursorCoords() {
    static auto PNOWARPS = CConfigValue<Config::INTEGER>("cursor:no_warps");

    if (*PNOWARPS)
        return;

    if (Desktop::focusState()->window())
        Desktop::focusState()->window()->m_relativeCursorCoordsOnLastWarp = g_pInputManager->getMouseCoordsInternal() - Desktop::focusState()->window()->m_position;
}

static void switchToWindow(PHLWINDOW PWINDOWTOCHANGETO, bool forceFSCycle = false) {
    static auto PFOLLOWMOUSE = CConfigValue<Config::INTEGER>("input:follow_mouse");
    static auto PNOWARPS     = CConfigValue<Config::INTEGER>("cursor:no_warps");

    const auto  PLASTWINDOW = Desktop::focusState()->window();

    if (PWINDOWTOCHANGETO == PLASTWINDOW || !PWINDOWTOCHANGETO)
        return;

    g_pInputManager->unconstrainMouse();

    if (PLASTWINDOW && PLASTWINDOW->m_workspace == PWINDOWTOCHANGETO->m_workspace && PLASTWINDOW->isFullscreen())
        Desktop::focusState()->fullWindowFocus(PWINDOWTOCHANGETO, Desktop::FOCUS_REASON_KEYBIND, nullptr, forceFSCycle);
    else {
        updateRelativeCursorCoords();
        Desktop::focusState()->fullWindowFocus(PWINDOWTOCHANGETO, Desktop::FOCUS_REASON_KEYBIND, nullptr, forceFSCycle);
        PWINDOWTOCHANGETO->warpCursor();

        if (*PNOWARPS == 0 || *PFOLLOWMOUSE < 2) {
            g_pInputManager->m_forcedFocus = PWINDOWTOCHANGETO;
            g_pInputManager->simulateMouseMovement();
            g_pInputManager->m_forcedFocus.reset();
        }

        if (PLASTWINDOW && PLASTWINDOW->m_monitor != PWINDOWTOCHANGETO->m_monitor) {
            const auto PNEWMON = PWINDOWTOCHANGETO->m_monitor.lock();
            Desktop::focusState()->rawMonitorFocus(PNEWMON);
        }
    }
}

static bool tryMoveFocusToMonitor(PHLMONITOR monitor) {
    if (!monitor)
        return false;

    const auto LASTMONITOR = Desktop::focusState()->monitor();
    if (!LASTMONITOR || LASTMONITOR == monitor)
        return false;

    static auto PFOLLOWMOUSE = CConfigValue<Config::INTEGER>("input:follow_mouse");
    static auto PNOWARPS     = CConfigValue<Config::INTEGER>("cursor:no_warps");

    g_pInputManager->unconstrainMouse();

    const auto PNEWMAINWORKSPACE = monitor->m_activeWorkspace;
    const auto PNEWWORKSPACE     = monitor->m_activeSpecialWorkspace ? monitor->m_activeSpecialWorkspace : PNEWMAINWORKSPACE;
    const auto PNEWWINDOW        = PNEWWORKSPACE->getLastFocusedWindow();

    if (PNEWWINDOW) {
        updateRelativeCursorCoords();
        Desktop::focusState()->fullWindowFocus(PNEWWINDOW, Desktop::FOCUS_REASON_KEYBIND);
        PNEWWINDOW->warpCursor();

        if (*PNOWARPS == 0 || *PFOLLOWMOUSE < 2) {
            g_pInputManager->m_forcedFocus = PNEWWINDOW;
            g_pInputManager->simulateMouseMovement();
            g_pInputManager->m_forcedFocus.reset();
        }
    } else {
        Desktop::focusState()->rawWindowFocus(nullptr, Desktop::FOCUS_REASON_KEYBIND);
        g_pCompositor->warpCursorTo(monitor->middle());
    }
    Desktop::focusState()->rawMonitorFocus(monitor);

    return true;
}

static std::string dirToString(Math::eDirection dir) {
    switch (dir) {
        case Math::DIRECTION_LEFT: return "l";
        case Math::DIRECTION_RIGHT: return "r";
        case Math::DIRECTION_UP: return "u";
        case Math::DIRECTION_DOWN: return "d";
        default: return "";
    }
}

ActionResult Actions::closeWindow(std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    if (window->m_closeableSince > Time::steadyNow())
        return std::unexpected("can't close window, it's not closeable yet (noclosefor)");

    window->sendClose();

    return {};
}

ActionResult Actions::killWindow(std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    kill(window->getPID(), SIGKILL);

    return {};
}

ActionResult Actions::signalWindow(int sig, std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    if (sig < 1 || sig > 31)
        return std::unexpected(std::format("Invalid signal number {}", sig));

    kill(window->getPID(), sig);

    return {};
}

ActionResult Actions::floatWindow(eTogglableAction action, std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    bool wantFloat = false;
    switch (action) {
        case TOGGLE_ACTION_TOGGLE: wantFloat = !window->m_isFloating; break;
        case TOGGLE_ACTION_ENABLE: wantFloat = true; break;
        case TOGGLE_ACTION_DISABLE: wantFloat = false; break;
    }

    if (wantFloat == window->m_isFloating)
        return {};

    if (g_layoutManager->dragController()->target())
        CKeybindManager::changeMouseBindMode(MBIND_INVALID);

    g_layoutManager->changeFloatingMode(window->layoutTarget());

    if (window->m_workspace) {
        window->m_workspace->updateWindows();
        window->m_workspace->updateWindowData();
    }

    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    return {};
}

ActionResult Actions::pseudoWindow(eTogglableAction action, std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    bool wantPseudo = false;
    switch (action) {
        case TOGGLE_ACTION_TOGGLE: wantPseudo = !window->layoutTarget()->isPseudo(); break;
        case TOGGLE_ACTION_ENABLE: wantPseudo = true; break;
        case TOGGLE_ACTION_DISABLE: wantPseudo = false; break;
    }

    window->layoutTarget()->setPseudo(wantPseudo);

    return {};
}

ActionResult Actions::pinWindow(eTogglableAction action, std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    if (!window->m_isFloating || window->isFullscreen())
        return std::unexpected("Window does not qualify to be pinned");

    bool wantPin = false;
    switch (action) {
        case TOGGLE_ACTION_TOGGLE: wantPin = !window->m_pinned; break;
        case TOGGLE_ACTION_ENABLE: wantPin = true; break;
        case TOGGLE_ACTION_DISABLE: wantPin = false; break;
    }

    if (wantPin == window->m_pinned)
        return {};

    window->m_pinned = wantPin;

    const auto PMONITOR = window->m_monitor.lock();
    if (!PMONITOR)
        return std::unexpected("Window has no monitor");

    window->moveToWorkspace(PMONITOR->m_activeWorkspace);
    window->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_PINNED);

    const auto PWORKSPACE = window->m_workspace;
    PWORKSPACE->m_lastFocusedWindow =
        g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(), Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS);

    g_pEventManager->postEvent(SHyprIPCEvent{.event = "pin", .data = std::format("{:x},{}", rc<uintptr_t>(window.get()), sc<int>(window->m_pinned))});
    Event::bus()->m_events.window.pin.emit(window);

    g_pHyprRenderer->damageWindow(window, true);

    return {};
}

ActionResult Actions::fullscreenWindow(eFullscreenMode mode, std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    if (window->isEffectiveInternalFSMode(mode))
        g_pCompositor->setWindowFullscreenInternal(window, FSMODE_NONE);
    else
        g_pCompositor->setWindowFullscreenInternal(window, mode);

    return {};
}

ActionResult Actions::fullscreenWindow(eFullscreenMode internalMode, eFullscreenMode clientMode, std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    window->m_ruleApplicator->syncFullscreenOverride(Desktop::Types::COverridableVar(false, Desktop::Types::PRIORITY_SET_PROP));

    const Desktop::View::SFullscreenState STATE = {.internal = internalMode, .client = clientMode};

    if (window->m_fullscreenState.internal == STATE.internal && window->m_fullscreenState.client == STATE.client)
        g_pCompositor->setWindowFullscreenState(window, Desktop::View::SFullscreenState{.internal = FSMODE_NONE, .client = FSMODE_NONE});
    else
        g_pCompositor->setWindowFullscreenState(window, STATE);

    window->m_ruleApplicator->syncFullscreenOverride(
        Desktop::Types::COverridableVar(window->m_fullscreenState.internal == window->m_fullscreenState.client, Desktop::Types::PRIORITY_SET_PROP));

    return {};
}

ActionResult Actions::moveToWorkspace(PHLWORKSPACE ws, bool silent, std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    if (!ws)
        return std::unexpected("Invalid workspace");

    if (ws->m_id == window->workspaceID())
        return {};

    const auto POLDWS = window->m_workspace;

    updateRelativeCursorCoords();
    g_pHyprRenderer->damageWindow(window);

    if (silent) {
        const auto OLDMIDDLE = window->middle();
        g_pCompositor->moveWindowToWorkspaceSafe(window, ws);

        if (window == Desktop::focusState()->window()) {
            if (const auto PATCOORDS =
                    g_pCompositor->vectorToWindowUnified(OLDMIDDLE, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING, window);
                PATCOORDS)
                Desktop::focusState()->fullWindowFocus(PATCOORDS, Desktop::FOCUS_REASON_KEYBIND);
            else
                g_pInputManager->refocus();
        }
    } else {
        PHLMONITOR pMonitor = nullptr;

        const auto FULLSCREENMODE = window->m_fullscreenState.internal;
        g_pCompositor->moveWindowToWorkspaceSafe(window, ws);
        pMonitor = ws->m_monitor.lock();
        Desktop::focusState()->rawMonitorFocus(pMonitor);
        g_pCompositor->setWindowFullscreenInternal(window, FULLSCREENMODE);

        POLDWS->m_lastFocusedWindow = POLDWS->getFirstWindow();

        if (ws->m_isSpecialWorkspace)
            pMonitor->setSpecialWorkspace(ws);
        else if (POLDWS->m_isSpecialWorkspace)
            POLDWS->m_monitor.lock()->setSpecialWorkspace(nullptr);

        pMonitor->changeWorkspace(ws);

        Desktop::focusState()->fullWindowFocus(window, Desktop::FOCUS_REASON_KEYBIND);
        window->warpCursor();
    }

    return {};
}

ActionResult Actions::moveFocus(Math::eDirection dir) {
    static auto PFULLCYCLE       = CConfigValue<Config::INTEGER>("binds:movefocus_cycles_fullscreen");
    static auto PGROUPCYCLE      = CConfigValue<Config::INTEGER>("binds:movefocus_cycles_groupfirst");
    static auto PMONITORFALLBACK = CConfigValue<Config::INTEGER>("binds:window_direction_monitor_fallback");

    const auto  PLASTWINDOW = Desktop::focusState()->window();
    if (!PLASTWINDOW || !PLASTWINDOW->aliveAndVisible()) {
        if (*PMONITORFALLBACK)
            tryMoveFocusToMonitor(g_pCompositor->getMonitorInDirection(dir));
        return {};
    }

    const auto PWINDOWTOCHANGETO = *PFULLCYCLE && PLASTWINDOW->isFullscreen() ?
        g_pCompositor->getWindowCycle(PLASTWINDOW, true, {}, false, dir != Math::DIRECTION_DOWN && dir != Math::DIRECTION_RIGHT) :
        g_pCompositor->getWindowInDirection(PLASTWINDOW, dir);

    if (*PGROUPCYCLE && PLASTWINDOW->m_group) {
        auto isTheOnlyGroupOnWs = !PWINDOWTOCHANGETO && g_pCompositor->m_monitors.size() == 1;
        if (dir == Math::DIRECTION_LEFT && (PLASTWINDOW != PLASTWINDOW->m_group->head() || isTheOnlyGroupOnWs)) {
            PLASTWINDOW->m_group->moveCurrent(false);
            return {};
        } else if (dir == Math::DIRECTION_RIGHT && (PLASTWINDOW != PLASTWINDOW->m_group->tail() || isTheOnlyGroupOnWs)) {
            PLASTWINDOW->m_group->moveCurrent(true);
            return {};
        }
    }

    if (PWINDOWTOCHANGETO) {
        switchToWindow(PWINDOWTOCHANGETO, *PFULLCYCLE && PLASTWINDOW->isFullscreen());
        return {};
    }

    if (*PMONITORFALLBACK && tryMoveFocusToMonitor(g_pCompositor->getMonitorInDirection(dir)))
        return {};

    static auto PNOFALLBACK = CConfigValue<Config::INTEGER>("general:no_focus_fallback");
    if (*PNOFALLBACK)
        return std::unexpected(std::format("Nothing to focus to in direction {}", Math::toString(dir)));

    const auto PMONITOR = PLASTWINDOW->m_monitor.lock();
    if (!PMONITOR)
        return std::unexpected("Window has no monitor");

    if (dir == Math::DIRECTION_LEFT || dir == Math::DIRECTION_RIGHT) {
        if (STICKS(PLASTWINDOW->m_position.x, PMONITOR->m_position.x) && STICKS(PLASTWINDOW->m_size.x, PMONITOR->m_size.x))
            return {};
    } else if (STICKS(PLASTWINDOW->m_position.y, PMONITOR->m_position.y) && STICKS(PLASTWINDOW->m_size.y, PMONITOR->m_size.y))
        return {};

    CBox box = PMONITOR->logicalBox();
    switch (dir) {
        case Math::DIRECTION_LEFT:
            box.x += box.w;
            box.w = 1;
            break;
        case Math::DIRECTION_RIGHT:
            box.x -= 1;
            box.w = 1;
            break;
        case Math::DIRECTION_UP:
            box.y += box.h;
            box.h = 1;
            break;
        case Math::DIRECTION_DOWN:
            box.y -= 1;
            box.h = 1;
            break;
        default: break;
    }

    const auto PWINDOWCANDIDATE = g_pCompositor->getWindowInDirection(box, PMONITOR->m_activeSpecialWorkspace ? PMONITOR->m_activeSpecialWorkspace : PMONITOR->m_activeWorkspace,
                                                                      dir, PLASTWINDOW, PLASTWINDOW->m_isFloating);
    if (PWINDOWCANDIDATE)
        switchToWindow(PWINDOWCANDIDATE);

    return {};
}

ActionResult Actions::focus(PHLWINDOW window) {
    if (!window)
        return std::unexpected("No target found.");

    const auto PWORKSPACE = window->m_workspace;
    if (!PWORKSPACE)
        return std::unexpected("Window has no workspace");

    updateRelativeCursorCoords();

    if (Desktop::focusState()->monitor() && Desktop::focusState()->monitor()->m_activeWorkspace != window->m_workspace &&
        Desktop::focusState()->monitor()->m_activeSpecialWorkspace != window->m_workspace) // NOLINTNEXTLINE
        Actions::changeWorkspace(PWORKSPACE);

    Desktop::focusState()->fullWindowFocus(window, Desktop::FOCUS_REASON_KEYBIND, nullptr, false);
    window->warpCursor();

    return {};
}

ActionResult Actions::moveInDirection(Math::eDirection dir, std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    if (window->isFullscreen())
        return std::unexpected("Can't move fullscreen window");

    updateRelativeCursorCoords();

    g_layoutManager->moveInDirection(window->layoutTarget(), dirToString(dir));
    window->warpCursor();

    return {};
}

ActionResult Actions::swapInDirection(Math::eDirection dir, std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    if (window->isFullscreen())
        return std::unexpected("Can't swap fullscreen window");

    const auto PWINDOWTOCHANGETO = g_pCompositor->getWindowInDirection(window, dir);

    if (!PWINDOWTOCHANGETO || PWINDOWTOCHANGETO == window)
        return std::unexpected("No window to swap with in that direction");

    updateRelativeCursorCoords();
    g_layoutManager->switchTargets(window->layoutTarget(), PWINDOWTOCHANGETO->layoutTarget(), true);
    window->warpCursor();

    return {};
}

ActionResult Actions::focusCurrentOrLast() {
    const auto& HISTORY = Desktop::History::windowTracker()->fullHistory();

    if (HISTORY.size() <= 1)
        return std::unexpected("History too short");

    const auto PWINDOWPREV = HISTORY[HISTORY.size() - 2].lock();

    if (!PWINDOWPREV)
        return std::unexpected("Window not found");

    switchToWindow(PWINDOWPREV);

    return {};
}

ActionResult Actions::focusUrgentOrLast() {
    const auto& HISTORY       = Desktop::History::windowTracker()->fullHistory();
    const auto  PWINDOWURGENT = g_pCompositor->getUrgentWindow();
    const auto  PWINDOWPREV   = Desktop::focusState()->window() ? (HISTORY.size() < 2 ? nullptr : HISTORY[1].lock()) : (HISTORY.empty() ? nullptr : HISTORY[0].lock());

    if (!PWINDOWURGENT && !PWINDOWPREV)
        return std::unexpected("Window not found");

    switchToWindow(PWINDOWURGENT ? PWINDOWURGENT : PWINDOWPREV);

    return {};
}

ActionResult Actions::center(std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window || !window->m_isFloating || window->isFullscreen())
        return std::unexpected("No floating window found");

    const auto PMONITOR = window->m_monitor.lock();

    window->layoutTarget()->setPositionGlobal(CBox{PMONITOR->logicalBoxMinusReserved().middle() - window->m_realSize->goal() / 2.F, window->layoutTarget()->position().size()});

    return {};
}

ActionResult Actions::moveCursorToCorner(int corner, std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    if (corner < 0 || corner > 3)
        return std::unexpected("Corner must be 0-3");

    switch (corner) {
        case 0: g_pCompositor->warpCursorTo({window->m_realPosition->value().x, window->m_realPosition->value().y + window->m_realSize->value().y}, true); break;
        case 1:
            g_pCompositor->warpCursorTo({window->m_realPosition->value().x + window->m_realSize->value().x, window->m_realPosition->value().y + window->m_realSize->value().y},
                                        true);
            break;
        case 2: g_pCompositor->warpCursorTo({window->m_realPosition->value().x + window->m_realSize->value().x, window->m_realPosition->value().y}, true); break;
        case 3: g_pCompositor->warpCursorTo({window->m_realPosition->value().x, window->m_realPosition->value().y}, true); break;
        default: break;
    }

    return {};
}

ActionResult Actions::resizeBy(const Vector2D& delta, std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    if (window->isFullscreen())
        return std::unexpected("Window is fullscreen");

    g_layoutManager->resizeTarget(delta, window->layoutTarget());

    if (window->m_realSize->goal().x > 1 && window->m_realSize->goal().y > 1)
        window->setHidden(false);

    return {};
}

ActionResult Actions::moveBy(const Vector2D& delta, std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    if (window->isFullscreen())
        return std::unexpected("Window is fullscreen");

    g_layoutManager->moveTarget(delta, window->layoutTarget());

    return {};
}

ActionResult Actions::tag(const std::string& tagStr, std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    if (window->m_ruleApplicator->m_tagKeeper.applyTag(tagStr)) {
        window->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_TAG);
        window->updateDecorationValues();
    }

    return {};
}

ActionResult Actions::swapNext(const bool next, std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    const auto PLASTCYCLED =
        validMapped(window->m_lastCycledWindow) && window->m_lastCycledWindow->m_workspace == window->m_workspace ? window->m_lastCycledWindow.lock() : nullptr;

    auto toSwap = g_pCompositor->getWindowCycle(PLASTCYCLED ? PLASTCYCLED : window, true, std::nullopt, false, !next);

    if (toSwap == window)
        toSwap = g_pCompositor->getWindowCycle(window, true, std::nullopt, false, !next);

    if (!toSwap)
        return std::unexpected("No window to swap with");

    g_layoutManager->switchTargets(window->layoutTarget(), toSwap->layoutTarget(), false);
    window->m_lastCycledWindow = toSwap;
    Desktop::focusState()->fullWindowFocus(window, Desktop::FOCUS_REASON_KEYBIND);

    return {};
}

ActionResult Actions::alterZOrder(const std::string& mode, std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    if (mode == "top")
        g_pCompositor->changeWindowZOrder(window, true);
    else if (mode == "bottom")
        g_pCompositor->changeWindowZOrder(window, false);
    else
        return std::unexpected(std::format("Bad z-order position: {}", mode));

    g_pInputManager->simulateMouseMovement();

    return {};
}

template <typename T>
static void parsePropTrivial(Desktop::Types::COverridableVar<T>& prop, const std::string& s) {
    static_assert(std::is_same_v<T, bool> || std::is_same_v<T, Hyprlang::INT> || std::is_same_v<T, int> || std::is_same_v<T, Hyprlang::FLOAT> || std::is_same_v<T, std::string>,
                  "Invalid type passed to parsePropTrivial");

    if (s == "unset") {
        prop.unset(Desktop::Types::PRIORITY_SET_PROP);
        return;
    }

    try {
        if constexpr (std::is_same_v<T, bool>) {
            if (s == "toggle")
                prop.increment(true, Desktop::Types::PRIORITY_SET_PROP);
            else
                prop = Desktop::Types::COverridableVar<T>(truthy(s), Desktop::Types::PRIORITY_SET_PROP);
        } else if constexpr (std::is_same_v<T, Hyprlang::INT> || std::is_same_v<T, int>) {
            if (s.starts_with("relative")) {
                const auto VAL = std::stoi(s.substr(s.find(' ') + 1));
                prop.increment(VAL, Desktop::Types::PRIORITY_SET_PROP);
            } else
                prop = Desktop::Types::COverridableVar<T>(std::stoull(s), Desktop::Types::PRIORITY_SET_PROP);
        } else if constexpr (std::is_same_v<T, Hyprlang::FLOAT>) {
            if (s.starts_with("relative")) {
                const auto VAL = std::stof(s.substr(s.find(' ') + 1));
                prop.increment(VAL, Desktop::Types::PRIORITY_SET_PROP);
            } else
                prop = Desktop::Types::COverridableVar<T>(std::stof(s), Desktop::Types::PRIORITY_SET_PROP);
        } else if constexpr (std::is_same_v<T, std::string>)
            prop = Desktop::Types::COverridableVar<T>(s, Desktop::Types::PRIORITY_SET_PROP);
    } catch (...) { Log::logger->log(Log::ERR, "Hyprctl: parsePropTrivial: failed to parse setprop for {}", s); }
}

ActionResult Actions::setProp(const std::string& PROP, const std::string& VAL, std::optional<PHLWINDOW> w) {
    auto PWINDOW = xtract(w);
    if (!PWINDOW)
        return std::unexpected("No target found.");

    try {
        if (PROP == "max_size") {
            const auto SIZE = PWINDOW->calculateExpression(VAL);
            if (!SIZE) {
                Log::logger->log(Log::ERR, "failed to parse {} as an expression", VAL);
                throw "failed to parse expression";
            }
            PWINDOW->m_ruleApplicator->maxSizeOverride(Desktop::Types::COverridableVar(*SIZE, Desktop::Types::PRIORITY_SET_PROP));
            PWINDOW->clampWindowSize(std::nullopt, PWINDOW->m_ruleApplicator->maxSize().value());
            PWINDOW->setHidden(false);
        } else if (PROP == "min_size") {
            const auto SIZE = PWINDOW->calculateExpression(VAL);
            if (!SIZE) {
                Log::logger->log(Log::ERR, "failed to parse {} as an expression", VAL);
                throw "failed to parse expression";
            }
            PWINDOW->m_ruleApplicator->minSizeOverride(Desktop::Types::COverridableVar(*SIZE, Desktop::Types::PRIORITY_SET_PROP));
            PWINDOW->clampWindowSize(PWINDOW->m_ruleApplicator->minSize().value(), std::nullopt);
            PWINDOW->setHidden(false);
        } else if (PROP == "active_border_color" || PROP == "inactive_border_color") {
            Config::CGradientValueData   colorData = {};

            Hyprutils::String::CVarList2 vars(VAL, 0, 's', true);

            if (vars.size() > 1) {
                for (int i = 1; i < sc<int>(vars.size()); ++i) {
                    const auto TOKEN = vars[i];
                    if (TOKEN.ends_with("deg"))
                        colorData.m_angle = std::stoi(std::string(TOKEN.substr(0, TOKEN.size() - 3))) * (PI / 180.0);
                    else
                        configStringToInt(std::string(TOKEN)).and_then([&colorData](const auto& e) {
                            colorData.m_colors.push_back(e);
                            return std::invoke_result_t<decltype(::configStringToInt), const std::string&>(1);
                        });
                }
            } else if (VAL != "-1")
                configStringToInt(VAL).and_then([&colorData](const auto& e) {
                    colorData.m_colors.push_back(e);
                    return std::invoke_result_t<decltype(::configStringToInt), const std::string&>(1);
                });

            colorData.updateColorsOk();

            if (PROP == "active_border_color")
                PWINDOW->m_ruleApplicator->activeBorderColorOverride(Desktop::Types::COverridableVar(colorData, Desktop::Types::PRIORITY_SET_PROP));
            else
                PWINDOW->m_ruleApplicator->inactiveBorderColorOverride(Desktop::Types::COverridableVar(colorData, Desktop::Types::PRIORITY_SET_PROP));
        } else if (PROP == "opacity") {
            PWINDOW->m_ruleApplicator->alphaOverride(Desktop::Types::COverridableVar(
                Desktop::Types::SAlphaValue{std::stof(VAL), PWINDOW->m_ruleApplicator->alpha().valueOrDefault().overridden}, Desktop::Types::PRIORITY_SET_PROP));
        } else if (PROP == "opacity_inactive") {
            PWINDOW->m_ruleApplicator->alphaInactiveOverride(Desktop::Types::COverridableVar(
                Desktop::Types::SAlphaValue{std::stof(VAL), PWINDOW->m_ruleApplicator->alphaInactive().valueOrDefault().overridden}, Desktop::Types::PRIORITY_SET_PROP));
        } else if (PROP == "opacity_fullscreen") {
            PWINDOW->m_ruleApplicator->alphaFullscreenOverride(Desktop::Types::COverridableVar(
                Desktop::Types::SAlphaValue{std::stof(VAL), PWINDOW->m_ruleApplicator->alphaFullscreen().valueOrDefault().overridden}, Desktop::Types::PRIORITY_SET_PROP));
        } else if (PROP == "opacity_override") {
            PWINDOW->m_ruleApplicator->alphaOverride(Desktop::Types::COverridableVar(
                Desktop::Types::SAlphaValue{PWINDOW->m_ruleApplicator->alpha().valueOrDefault().alpha, sc<bool>(configStringToInt(VAL).value_or(0))},
                Desktop::Types::PRIORITY_SET_PROP));
        } else if (PROP == "opacity_inactive_override") {
            PWINDOW->m_ruleApplicator->alphaInactiveOverride(Desktop::Types::COverridableVar(
                Desktop::Types::SAlphaValue{PWINDOW->m_ruleApplicator->alphaInactive().valueOrDefault().alpha, sc<bool>(configStringToInt(VAL).value_or(0))},
                Desktop::Types::PRIORITY_SET_PROP));
        } else if (PROP == "opacity_fullscreen_override") {
            PWINDOW->m_ruleApplicator->alphaFullscreenOverride(Desktop::Types::COverridableVar(
                Desktop::Types::SAlphaValue{PWINDOW->m_ruleApplicator->alphaFullscreen().valueOrDefault().alpha, sc<bool>(configStringToInt(VAL).value_or(0))},
                Desktop::Types::PRIORITY_SET_PROP));
        } else if (PROP == "allows_input")
            parsePropTrivial(PWINDOW->m_ruleApplicator->allowsInput(), VAL);
        else if (PROP == "decorate")
            parsePropTrivial(PWINDOW->m_ruleApplicator->decorate(), VAL);
        else if (PROP == "focus_on_activate")
            parsePropTrivial(PWINDOW->m_ruleApplicator->focusOnActivate(), VAL);
        else if (PROP == "keep_aspect_ratio")
            parsePropTrivial(PWINDOW->m_ruleApplicator->keepAspectRatio(), VAL);
        else if (PROP == "nearest_neighbor")
            parsePropTrivial(PWINDOW->m_ruleApplicator->nearestNeighbor(), VAL);
        else if (PROP == "no_anim")
            parsePropTrivial(PWINDOW->m_ruleApplicator->noAnim(), VAL);
        else if (PROP == "no_blur")
            parsePropTrivial(PWINDOW->m_ruleApplicator->noBlur(), VAL);
        else if (PROP == "no_dim")
            parsePropTrivial(PWINDOW->m_ruleApplicator->noDim(), VAL);
        else if (PROP == "no_focus")
            parsePropTrivial(PWINDOW->m_ruleApplicator->noFocus(), VAL);
        else if (PROP == "no_max_size")
            parsePropTrivial(PWINDOW->m_ruleApplicator->noMaxSize(), VAL);
        else if (PROP == "no_shadow")
            parsePropTrivial(PWINDOW->m_ruleApplicator->noShadow(), VAL);
        else if (PROP == "no_shortcuts_inhibit")
            parsePropTrivial(PWINDOW->m_ruleApplicator->noShortcutsInhibit(), VAL);
        else if (PROP == "dim_around")
            parsePropTrivial(PWINDOW->m_ruleApplicator->dimAround(), VAL);
        else if (PROP == "opaque")
            parsePropTrivial(PWINDOW->m_ruleApplicator->opaque(), VAL);
        else if (PROP == "force_rgbx")
            parsePropTrivial(PWINDOW->m_ruleApplicator->RGBX(), VAL);
        else if (PROP == "sync_fullscreen")
            parsePropTrivial(PWINDOW->m_ruleApplicator->syncFullscreen(), VAL);
        else if (PROP == "immediate")
            parsePropTrivial(PWINDOW->m_ruleApplicator->tearing(), VAL);
        else if (PROP == "xray")
            parsePropTrivial(PWINDOW->m_ruleApplicator->xray(), VAL);
        else if (PROP == "render_unfocused")
            parsePropTrivial(PWINDOW->m_ruleApplicator->renderUnfocused(), VAL);
        else if (PROP == "no_follow_mouse")
            parsePropTrivial(PWINDOW->m_ruleApplicator->noFollowMouse(), VAL);
        else if (PROP == "no_screen_share")
            parsePropTrivial(PWINDOW->m_ruleApplicator->noScreenShare(), VAL);
        else if (PROP == "no_vrr")
            parsePropTrivial(PWINDOW->m_ruleApplicator->noVRR(), VAL);
        else if (PROP == "persistent_size")
            parsePropTrivial(PWINDOW->m_ruleApplicator->persistentSize(), VAL);
        else if (PROP == "stay_focused")
            parsePropTrivial(PWINDOW->m_ruleApplicator->stayFocused(), VAL);
        else if (PROP == "idle_inhibit")
            parsePropTrivial(PWINDOW->m_ruleApplicator->idleInhibitMode(), VAL);
        else if (PROP == "border_size")
            parsePropTrivial(PWINDOW->m_ruleApplicator->borderSize(), VAL);
        else if (PROP == "rounding")
            parsePropTrivial(PWINDOW->m_ruleApplicator->rounding(), VAL);
        else if (PROP == "rounding_power")
            parsePropTrivial(PWINDOW->m_ruleApplicator->roundingPower(), VAL);
        else if (PROP == "scroll_mouse")
            parsePropTrivial(PWINDOW->m_ruleApplicator->scrollMouse(), VAL);
        else if (PROP == "scroll_touchpad")
            parsePropTrivial(PWINDOW->m_ruleApplicator->scrollTouchpad(), VAL);
        else if (PROP == "animation")
            parsePropTrivial(PWINDOW->m_ruleApplicator->animationStyle(), VAL);
        else
            return std::unexpected("prop not found");

    } catch (std::exception& e) { return std::unexpected(std::format("Error parsing prop value: {}", std::string(e.what()))); }

    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    if (PROP == "no_vrr")
        Config::monitorRuleMgr()->ensureVRR();

    for (auto const& m : g_pCompositor->m_monitors) {
        if (m->m_activeWorkspace)
            m->m_activeWorkspace->m_space->recalculate();
    }

    return {};
}

ActionResult Actions::toggleGroup(std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    if (window->isFullscreen())
        g_pCompositor->setWindowFullscreenInternal(window, FSMODE_NONE);

    if (!window->m_group)
        window->m_group = Desktop::View::CGroup::create({window});
    else
        window->m_group->destroy();

    return {};
}

ActionResult Actions::changeGroupActive(bool forward, std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    if (!window->m_group)
        return std::unexpected("Window is not in a group");

    if (window->m_group->size() == 1)
        return std::unexpected("Only one window in group");

    window->m_group->moveCurrent(forward);

    return {};
}

ActionResult Actions::changeWorkspace(PHLWORKSPACE ws) {
    if (!ws)
        return std::unexpected("Invalid workspace");

    static auto PHIDESPECIALONWORKSPACECHANGE = CConfigValue<Config::INTEGER>("binds:hide_special_on_workspace_change");
    static auto PWORKSPACECENTERON            = CConfigValue<Config::INTEGER>("binds:workspace_center_on");

    const auto  PMONITOR = Desktop::focusState()->monitor();
    if (!PMONITOR)
        return std::unexpected("No monitor");

    if (ws->m_isSpecialWorkspace) {
        PMONITOR->setSpecialWorkspace(ws);
        g_pInputManager->simulateMouseMovement();
        return {};
    }

    g_pInputManager->unconstrainMouse();
    g_pInputManager->m_emptyFocusCursorSet = false;
    g_pInputManager->releaseAllMouseButtons();

    const auto PMONITORWORKSPACEOWNER = PMONITOR == ws->m_monitor ? PMONITOR : ws->m_monitor.lock();
    if (!PMONITORWORKSPACEOWNER)
        return std::unexpected("Workspace has no monitor");

    updateRelativeCursorCoords();

    Desktop::focusState()->rawMonitorFocus(PMONITORWORKSPACEOWNER);

    if (*PHIDESPECIALONWORKSPACECHANGE)
        PMONITORWORKSPACEOWNER->setSpecialWorkspace(nullptr);
    PMONITORWORKSPACEOWNER->changeWorkspace(ws, false, true);

    if (PMONITOR != PMONITORWORKSPACEOWNER) {
        Vector2D middle = PMONITORWORKSPACEOWNER->middle();
        if (const auto PLAST = ws->getLastFocusedWindow(); PLAST) {
            Desktop::focusState()->fullWindowFocus(PLAST, Desktop::FOCUS_REASON_KEYBIND);
            if (*PWORKSPACECENTERON == 1)
                middle = PLAST->middle();
        }
        g_pCompositor->warpCursorTo(middle);
    }

    if (!g_pInputManager->m_lastFocusOnLS) {
        if (Desktop::focusState()->surface())
            g_pInputManager->sendMotionEventsToFocused();
        else
            g_pInputManager->simulateMouseMovement();
    }

    const static auto PWARPONWORKSPACECHANGE = CConfigValue<Config::INTEGER>("cursor:warp_on_change_workspace");

    if (*PWARPONWORKSPACECHANGE > 0) {
        auto PLAST     = ws->getLastFocusedWindow();
        auto HLSurface = Desktop::View::CWLSurface::fromResource(g_pSeatManager->m_state.pointerFocus.lock());

        if (PLAST && (!HLSurface || HLSurface->view()->type() == Desktop::View::VIEW_TYPE_WINDOW))
            PLAST->warpCursor(*PWARPONWORKSPACECHANGE == 2);
    }

    return {};
}

static PHLWORKSPACE resolveWorkspaceForChange(const std::string& args) {
    static auto PBACKANDFORTH = CConfigValue<Config::INTEGER>("binds:workspace_back_and_forth");

    const auto  PMONITOR = Desktop::focusState()->monitor();
    if (!PMONITOR)
        return nullptr;

    const auto PCURRENTWORKSPACE = PMONITOR->m_activeWorkspace;
    const bool EXPLICITPREVIOUS  = args.contains("previous");

    // handle "previous" workspace
    if (args.starts_with("previous")) {
        const bool             PER_MON = args.contains("_per_monitor");
        const SWorkspaceIDName PPREVWS = PER_MON ? Desktop::History::workspaceTracker()->previousWorkspaceIDName(PCURRENTWORKSPACE, PMONITOR) :
                                                   Desktop::History::workspaceTracker()->previousWorkspaceIDName(PCURRENTWORKSPACE);
        if (PPREVWS.id == -1 || PPREVWS.id == PCURRENTWORKSPACE->m_id)
            return nullptr;

        auto ws = g_pCompositor->getWorkspaceByID(PPREVWS.id);
        if (!ws)
            ws = g_pCompositor->createNewWorkspace(PPREVWS.id, PMONITOR->m_id, PPREVWS.name.empty() ? std::to_string(PPREVWS.id) : PPREVWS.name);
        return ws;
    }

    const auto& [workspaceToChangeTo, workspaceName, isAutoID] = getWorkspaceIDNameFromString(args);
    if (workspaceToChangeTo == WORKSPACE_INVALID || workspaceToChangeTo == WORKSPACE_NOT_CHANGED)
        return nullptr;

    // back_and_forth: if switching to current workspace, go to previous
    if (workspaceToChangeTo == PCURRENTWORKSPACE->m_id && (*PBACKANDFORTH || EXPLICITPREVIOUS)) {
        const SWorkspaceIDName PPREVWS = args.contains("_per_monitor") ? Desktop::History::workspaceTracker()->previousWorkspaceIDName(PCURRENTWORKSPACE, PMONITOR) :
                                                                         Desktop::History::workspaceTracker()->previousWorkspaceIDName(PCURRENTWORKSPACE);
        if (PPREVWS.id == -1)
            return nullptr;

        auto ws = g_pCompositor->getWorkspaceByID(PPREVWS.id);
        if (!ws)
            ws = g_pCompositor->createNewWorkspace(PPREVWS.id, PMONITOR->m_id, PPREVWS.name.empty() ? std::to_string(PPREVWS.id) : PPREVWS.name);
        return ws;
    }

    auto ws = g_pCompositor->getWorkspaceByID(workspaceToChangeTo);
    if (!ws)
        ws = g_pCompositor->createNewWorkspace(workspaceToChangeTo, PMONITOR->m_id, workspaceName);
    return ws;
}

ActionResult Actions::changeWorkspace(const std::string& ws) {
    auto p = resolveWorkspaceForChange(ws);
    if (!p)
        return std::unexpected("invalid workspace");
    return Actions::changeWorkspace(p);
}

ActionResult Actions::renameWorkspace(PHLWORKSPACE ws, const std::string& s) {
    if (!ws)
        return std::unexpected("Invalid workspace");

    ws->rename(s);

    return {};
}

ActionResult Actions::moveToMonitor(PHLWORKSPACE ws, PHLMONITOR mon) {
    if (!ws)
        return std::unexpected("Invalid workspace");
    if (!mon)
        return std::unexpected("Invalid monitor");

    g_pCompositor->moveWorkspaceToMonitor(ws, mon);

    return {};
}

ActionResult Actions::changeWorkspaceOnCurrentMonitor(PHLWORKSPACE ws) {
    if (!ws)
        return std::unexpected("Invalid workspace");

    const auto PCURRMONITOR = Desktop::focusState()->monitor();
    if (!PCURRMONITOR)
        return std::unexpected("No current monitor");

    if (ws->m_monitor != PCURRMONITOR) {
        const auto POLDMONITOR = ws->m_monitor.lock();
        if (!POLDMONITOR)
            return std::unexpected("Workspace has no monitor");

        if (POLDMONITOR->activeWorkspaceID() == ws->m_id) {
            g_pCompositor->swapActiveWorkspaces(POLDMONITOR, PCURRMONITOR);
            return {};
        } else {
            g_pCompositor->moveWorkspaceToMonitor(ws, PCURRMONITOR, true);
        }
    }

    return changeWorkspace(ws);
}

ActionResult Actions::toggleSpecial(PHLWORKSPACE special) {
    if (!special || !special->m_isSpecialWorkspace)
        return std::unexpected("Invalid special workspace");

    const auto PMONITOR = Desktop::focusState()->monitor();
    if (!PMONITOR)
        return std::unexpected("No monitor");

    bool requestedWorkspaceIsAlreadyOpen = false;
    auto specialOpenOnMonitor            = PMONITOR->activeSpecialWorkspaceID();

    for (auto const& m : g_pCompositor->m_monitors) {
        if (m->activeSpecialWorkspaceID() == special->m_id) {
            requestedWorkspaceIsAlreadyOpen = true;
            break;
        }
    }

    updateRelativeCursorCoords();

    PHLWORKSPACEREF focusedWorkspace;

    if (requestedWorkspaceIsAlreadyOpen && specialOpenOnMonitor == special->m_id) {
        PMONITOR->setSpecialWorkspace(nullptr);
        focusedWorkspace = PMONITOR->m_activeWorkspace;
    } else {
        PMONITOR->setSpecialWorkspace(special);
        focusedWorkspace = special;
    }

    const static auto PWARPONTOGGLESPECIAL = CConfigValue<Config::INTEGER>("cursor:warp_on_toggle_special");

    if (*PWARPONTOGGLESPECIAL > 0) {
        auto PLAST     = focusedWorkspace->getLastFocusedWindow();
        auto HLSurface = Desktop::View::CWLSurface::fromResource(g_pSeatManager->m_state.pointerFocus.lock());

        if (PLAST && (!HLSurface || HLSurface->view()->type() == Desktop::View::VIEW_TYPE_WINDOW))
            PLAST->warpCursor(*PWARPONTOGGLESPECIAL == 2);
    }

    return {};
}

ActionResult Actions::focusMonitor(PHLMONITOR mon) {
    if (!mon)
        return std::unexpected("Invalid monitor");

    tryMoveFocusToMonitor(mon);

    return {};
}

ActionResult Actions::swapActiveWorkspaces(PHLMONITOR mon1, PHLMONITOR mon2) {
    if (!mon1 || !mon2)
        return std::unexpected("Invalid monitor");

    if (mon1 == mon2)
        return {};

    g_pCompositor->swapActiveWorkspaces(mon1, mon2);

    return {};
}

ActionResult Actions::layoutMessage(const std::string& msg) {
    auto ret = g_layoutManager->layoutMsg(msg);
    if (!ret)
        return std::unexpected(ret.error());
    return {};
}

ActionResult Actions::moveCursor(const Vector2D& pos) {
    g_pCompositor->warpCursorTo(pos, true);
    g_pInputManager->simulateMouseMovement();
    return {};
}

ActionResult Actions::exit() {
    Event::bus()->m_events.exit.emit();

    if (g_pCompositor->m_finalRequests)
        return {};

    g_pCompositor->stopCompositor();
    return {};
}

ActionResult Actions::forceRendererReload() {
    bool overAgain = false;

    for (auto const& m : g_pCompositor->m_monitors) {
        if (!m->m_output)
            continue;

        auto rule = Config::monitorRuleMgr()->get(m);
        if (!m->applyMonitorRule(std::move(rule), true)) {
            overAgain = true;
            break;
        }
    }

    if (overAgain) // NOLINTNEXTLINE
        forceRendererReload();

    return {};
}

ActionResult Actions::toggleSwallow() {
    PHLWINDOWREF pWindow = Desktop::focusState()->window();

    if (!valid(pWindow) || !valid(pWindow->m_swallowed))
        return {};

    if (pWindow->m_swallowed->m_currentlySwallowed) {
        pWindow->m_swallowed->m_currentlySwallowed = false;
        pWindow->m_swallowed->setHidden(false);
        g_layoutManager->newTarget(pWindow->m_swallowed->layoutTarget(), pWindow->m_workspace->m_space);
    } else {
        pWindow->m_swallowed->m_currentlySwallowed = true;
        pWindow->m_swallowed->setHidden(true);
        g_layoutManager->removeTarget(pWindow->m_swallowed->layoutTarget());
    }

    return {};
}

ActionResult Actions::dpms(eTogglableAction action, std::optional<PHLMONITOR> mon) {
    for (auto const& m : g_pCompositor->m_realMonitors) {
        if (!m->m_enabled)
            continue;

        if (mon.has_value() && m != mon.value())
            continue;

        bool enable;
        switch (action) {
            case TOGGLE_ACTION_TOGGLE: enable = !m->m_dpmsStatus; break;
            case TOGGLE_ACTION_ENABLE: enable = true; break;
            case TOGGLE_ACTION_DISABLE: enable = false; break;
        }

        m->setDPMS(enable);
        g_pCompositor->m_dpmsStateOn = enable;
    }

    g_pPointerManager->recheckEnteredOutputs();

    return {};
}

ActionResult Actions::forceIdle(float seconds) {
    PROTO::idle->setTimers(seconds * 1000.0);
    return {};
}

ActionResult Actions::event(const std::string& data) {
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "custom", .data = data});
    return {};
}

ActionResult Actions::lockGroups(eTogglableAction action) {
    switch (action) {
        case TOGGLE_ACTION_TOGGLE: g_pKeybindManager->m_groupsLocked = !g_pKeybindManager->m_groupsLocked; break;
        case TOGGLE_ACTION_ENABLE: g_pKeybindManager->m_groupsLocked = true; break;
        case TOGGLE_ACTION_DISABLE: g_pKeybindManager->m_groupsLocked = false; break;
    }

    g_pEventManager->postEvent(SHyprIPCEvent{.event = "lockgroups", .data = g_pKeybindManager->m_groupsLocked ? "1" : "0"});
    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    return {};
}

ActionResult Actions::lockActiveGroup(eTogglableAction action) {
    const auto PWINDOW = Desktop::focusState()->window();
    if (!PWINDOW)
        return std::unexpected("No window found");

    if (!PWINDOW->m_group)
        return std::unexpected("Window not in a group");

    switch (action) {
        case TOGGLE_ACTION_TOGGLE: PWINDOW->m_group->setLocked(!PWINDOW->m_group->locked()); break;
        case TOGGLE_ACTION_ENABLE: PWINDOW->m_group->setLocked(true); break;
        case TOGGLE_ACTION_DISABLE: PWINDOW->m_group->setLocked(false); break;
    }

    PWINDOW->updateDecorationValues();

    return {};
}

static void moveWindowIntoGroupHelper(PHLWINDOW pWindow, PHLWINDOW pWindowInDirection) {
    if (!pWindowInDirection->m_group || pWindowInDirection->m_group->denied())
        return;

    updateRelativeCursorCoords();

    if (pWindow->m_monitor != pWindowInDirection->m_monitor) {
        pWindow->moveToWorkspace(pWindowInDirection->m_workspace);
        pWindow->m_monitor = pWindowInDirection->m_monitor;
    }

    pWindowInDirection->m_group->add(pWindow);
    pWindowInDirection->m_group->setCurrent(pWindow);
    pWindow->updateWindowDecos();
    Desktop::focusState()->fullWindowFocus(pWindow, Desktop::FOCUS_REASON_KEYBIND);
    pWindow->warpCursor();

    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveintogroup", .data = std::format("{:x}", rc<uintptr_t>(pWindow.get()))});
}

static void moveWindowOutOfGroupHelper(PHLWINDOW pWindow, Math::eDirection direction = Math::DIRECTION_DEFAULT) {
    static auto BFOCUSREMOVEDWINDOW = CConfigValue<Config::INTEGER>("group:focus_removed_window");

    if (!pWindow->m_group)
        return;

    WP<Desktop::View::CGroup> group = pWindow->m_group;

    pWindow->m_group->remove(pWindow, direction);

    if (*BFOCUSREMOVEDWINDOW || !group) {
        Desktop::focusState()->fullWindowFocus(pWindow, Desktop::FOCUS_REASON_KEYBIND);
        pWindow->warpCursor();
    } else {
        Desktop::focusState()->fullWindowFocus(group->current(), Desktop::FOCUS_REASON_KEYBIND);
        group->current()->warpCursor();
    }

    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveoutofgroup", .data = std::format("{:x}", rc<uintptr_t>(pWindow.get()))});
}

ActionResult Actions::moveIntoGroup(Math::eDirection direction, std::optional<PHLWINDOW> w) {
    static auto PIGNOREGROUPLOCK = CConfigValue<Config::INTEGER>("binds:ignore_group_lock");

    if (!*PIGNOREGROUPLOCK && g_pKeybindManager->m_groupsLocked)
        return {};

    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    auto PWINDOWINDIR = g_pCompositor->getWindowInDirection(window, direction);

    if (!PWINDOWINDIR || !PWINDOWINDIR->m_group)
        return {};

    if (!*PIGNOREGROUPLOCK && (PWINDOWINDIR->m_group->locked() || (window->m_group && window->m_group->locked())))
        return {};

    moveWindowIntoGroupHelper(window, PWINDOWINDIR);

    return {};
}

ActionResult Actions::moveOutOfGroup(Math::eDirection direction, std::optional<PHLWINDOW> w) {
    static auto PIGNOREGROUPLOCK = CConfigValue<Config::INTEGER>("binds:ignore_group_lock");

    if (!*PIGNOREGROUPLOCK && g_pKeybindManager->m_groupsLocked)
        return std::unexpected("Groups locked");

    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    if (!window->m_group)
        return std::unexpected("Window not in a group");

    moveWindowOutOfGroupHelper(window, direction);

    return {};
}

ActionResult Actions::moveGroupWindow(bool forward) {
    const auto PLASTWINDOW = Desktop::focusState()->window();
    if (!PLASTWINDOW)
        return std::unexpected("No window found");

    if (!PLASTWINDOW->m_group)
        return std::unexpected("Window not in a group");

    if (forward)
        PLASTWINDOW->m_group->swapWithNext();
    else
        PLASTWINDOW->m_group->swapWithLast();

    return {};
}

ActionResult Actions::moveWindowOrGroup(Math::eDirection direction, std::optional<PHLWINDOW> w) {
    static auto PIGNOREGROUPLOCK = CConfigValue<Config::INTEGER>("binds:ignore_group_lock");

    auto        window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    if (window->isFullscreen())
        return {};

    if (!*PIGNOREGROUPLOCK && g_pKeybindManager->m_groupsLocked) {
        g_layoutManager->moveInDirection(window->layoutTarget(), dirToString(direction));
        return {};
    }

    const auto PWINDOWINDIR = g_pCompositor->getWindowInDirection(window, direction);

    const bool ISWINDOWGROUP       = window->m_group;
    const bool ISWINDOWGROUPLOCKED = ISWINDOWGROUP && window->m_group->locked();
    const bool ISWINDOWGROUPSINGLE = ISWINDOWGROUP && window->m_group->size() == 1;
    const bool ISWINDOWGROUPDENIED = ISWINDOWGROUP && window->m_group->denied();

    updateRelativeCursorCoords();

    if (PWINDOWINDIR && PWINDOWINDIR->m_group) {
        if (!*PIGNOREGROUPLOCK && (PWINDOWINDIR->m_group->locked() || ISWINDOWGROUPLOCKED || ISWINDOWGROUPDENIED)) {
            g_layoutManager->moveInDirection(window->layoutTarget(), dirToString(direction));
            window->warpCursor();
        } else
            moveWindowIntoGroupHelper(window, PWINDOWINDIR);
    } else if (PWINDOWINDIR) {
        if ((!*PIGNOREGROUPLOCK && ISWINDOWGROUPLOCKED) || !ISWINDOWGROUP || (ISWINDOWGROUPSINGLE && window->m_groupRules & Desktop::View::GROUP_SET_ALWAYS)) {
            g_layoutManager->moveInDirection(window->layoutTarget(), dirToString(direction));
            window->warpCursor();
        } else
            moveWindowOutOfGroupHelper(window, direction);
    } else if ((*PIGNOREGROUPLOCK || !ISWINDOWGROUPLOCKED) && ISWINDOWGROUP) {
        moveWindowOutOfGroupHelper(window, direction);
    } else if (!PWINDOWINDIR && !ISWINDOWGROUP) {
        g_layoutManager->moveInDirection(window->layoutTarget(), dirToString(direction));
        window->warpCursor();
    }

    window->updateDecorationValues();

    return {};
}

ActionResult Actions::denyWindowFromGroup(eTogglableAction action) {
    const auto PWINDOW = Desktop::focusState()->window();
    if (!PWINDOW || !PWINDOW->m_group)
        return {};

    switch (action) {
        case TOGGLE_ACTION_TOGGLE: PWINDOW->m_group->setDenied(!PWINDOW->m_group->denied()); break;
        case TOGGLE_ACTION_ENABLE: PWINDOW->m_group->setDenied(true); break;
        case TOGGLE_ACTION_DISABLE: PWINDOW->m_group->setDenied(false); break;
    }

    PWINDOW->updateDecorationValues();

    return {};
}

ActionResult Actions::pass(std::optional<PHLWINDOW> w) {
    auto window = xtract(w);
    if (!window)
        return std::unexpected("No target found.");

    if (!g_pSeatManager->m_keyboard)
        return std::unexpected("No keyboard");

    const auto& S             = *Config::Actions::state();
    const auto  XWTOXW        = window->m_isX11 && Desktop::focusState()->window() && Desktop::focusState()->window()->m_isX11;
    const auto  LASTMOUSESURF = g_pSeatManager->m_state.pointerFocus.lock();
    const auto  LASTKBSURF    = g_pSeatManager->m_state.keyboardFocus.lock();

    if (!XWTOXW) {
        if (S.m_lastCode != 0)
            g_pSeatManager->setKeyboardFocus(window->wlSurface()->resource());
        else
            g_pSeatManager->setPointerFocus(window->wlSurface()->resource(), {1, 1});
    }

    g_pSeatManager->sendKeyboardMods(g_pInputManager->getModsFromAllKBs(), 0, 0, 0);

    if (S.m_passPressed == 1) {
        if (S.m_lastCode != 0)
            g_pSeatManager->sendKeyboardKey(S.m_timeLastMs, S.m_lastCode - 8, WL_KEYBOARD_KEY_STATE_PRESSED);
        else
            g_pSeatManager->sendPointerButton(S.m_timeLastMs, S.m_lastMouseCode, WL_POINTER_BUTTON_STATE_PRESSED);
    } else if (S.m_passPressed == 0) {
        if (S.m_lastCode != 0)
            g_pSeatManager->sendKeyboardKey(S.m_timeLastMs, S.m_lastCode - 8, WL_KEYBOARD_KEY_STATE_RELEASED);
        else
            g_pSeatManager->sendPointerButton(S.m_timeLastMs, S.m_lastMouseCode, WL_POINTER_BUTTON_STATE_RELEASED);
    } else {
        if (S.m_lastCode != 0) {
            g_pSeatManager->sendKeyboardKey(S.m_timeLastMs, S.m_lastCode - 8, WL_KEYBOARD_KEY_STATE_PRESSED);
            g_pSeatManager->sendKeyboardKey(S.m_timeLastMs, S.m_lastCode - 8, WL_KEYBOARD_KEY_STATE_RELEASED);
        } else {
            g_pSeatManager->sendPointerButton(S.m_timeLastMs, S.m_lastMouseCode, WL_POINTER_BUTTON_STATE_PRESSED);
            g_pSeatManager->sendPointerButton(S.m_timeLastMs, S.m_lastMouseCode, WL_POINTER_BUTTON_STATE_RELEASED);
        }
    }

    if (XWTOXW)
        return {};

    if (window->m_isX11) {
        if (S.m_lastCode != 0) {
            g_pSeatManager->m_state.keyboardFocus.reset();
            g_pSeatManager->m_state.keyboardFocusResource.reset();
        } else {
            g_pSeatManager->m_state.pointerFocus.reset();
            g_pSeatManager->m_state.pointerFocusResource.reset();
        }
    }

    const auto SL = window->m_realPosition->goal() - g_pInputManager->getMouseCoordsInternal();

    if (S.m_lastCode != 0)
        g_pSeatManager->setKeyboardFocus(LASTKBSURF);
    else
        g_pSeatManager->setPointerFocus(LASTMOUSESURF, SL);

    return {};
}

ActionResult Actions::pass(uint32_t modMask, uint32_t key, std::optional<PHLWINDOW> w) {
    auto        window = xtract(w);

    const auto& S           = *Config::Actions::state();
    const bool  isMouse     = key >= 272 && key < 0x160; // mouse button range
    const auto  LASTSURFACE = Desktop::focusState()->surface();

    if (window) {
        if (!g_pSeatManager->m_keyboard)
            return std::unexpected("No keyboard");

        if (!isMouse)
            g_pSeatManager->setKeyboardFocus(window->wlSurface()->resource());
        else
            g_pSeatManager->setPointerFocus(window->wlSurface()->resource(), {1, 1});

        // if wl -> xwl, activate destination
        if (window->m_isX11 && Desktop::focusState()->window() && !Desktop::focusState()->window()->m_isX11)
            g_pXWaylandManager->activateSurface(window->wlSurface()->resource(), true);
        // if xwl -> xwl, send to current
        if (window->m_isX11 && Desktop::focusState()->window() && Desktop::focusState()->window()->m_isX11)
            window = nullptr;
    }

    g_pSeatManager->sendKeyboardMods(modMask, 0, 0, 0);

    if (S.m_passPressed == 1) {
        if (!isMouse)
            g_pSeatManager->sendKeyboardKey(S.m_timeLastMs, key - 8, WL_KEYBOARD_KEY_STATE_PRESSED);
        else
            g_pSeatManager->sendPointerButton(S.m_timeLastMs, key, WL_POINTER_BUTTON_STATE_PRESSED);
    } else if (S.m_passPressed == 0) {
        if (!isMouse)
            g_pSeatManager->sendKeyboardKey(S.m_timeLastMs, key - 8, WL_KEYBOARD_KEY_STATE_RELEASED);
        else
            g_pSeatManager->sendPointerButton(S.m_timeLastMs, key, WL_POINTER_BUTTON_STATE_RELEASED);
    } else {
        if (!isMouse) {
            g_pSeatManager->sendKeyboardKey(S.m_timeLastMs, key - 8, WL_KEYBOARD_KEY_STATE_PRESSED);
            g_pSeatManager->sendKeyboardKey(S.m_timeLastMs, key - 8, WL_KEYBOARD_KEY_STATE_RELEASED);
        } else {
            g_pSeatManager->sendPointerButton(S.m_timeLastMs, key, WL_POINTER_BUTTON_STATE_PRESSED);
            g_pSeatManager->sendPointerButton(S.m_timeLastMs, key, WL_POINTER_BUTTON_STATE_RELEASED);
        }
    }

    g_pSeatManager->sendKeyboardMods(0, 0, 0, 0);

    if (!window)
        return {};

    if (window->m_isX11) {
        if (!isMouse) {
            g_pSeatManager->m_state.keyboardFocus.reset();
            g_pSeatManager->m_state.keyboardFocusResource.reset();
        } else {
            g_pSeatManager->m_state.pointerFocus.reset();
            g_pSeatManager->m_state.pointerFocusResource.reset();
        }
    }

    const auto SL = window->m_realPosition->goal() - g_pInputManager->getMouseCoordsInternal();

    if (!isMouse)
        g_pSeatManager->setKeyboardFocus(LASTSURFACE);
    else
        g_pSeatManager->setPointerFocus(LASTSURFACE, SL);

    return {};
}

ActionResult Actions::sendKeyState(uint32_t modMask, uint32_t key, uint32_t keyState, std::optional<PHLWINDOW> w) {
    // keyState: 0 = up, 1 = down, 2 = repeat (down+down)
    const int oldPassPressed = Config::Actions::state()->m_passPressed;

    if (keyState == 1 || keyState == 2)
        Config::Actions::state()->m_passPressed = 1;
    else
        Config::Actions::state()->m_passPressed = 0;

    auto result = Actions::pass(modMask, key, w);

    if (keyState == 2 && result.has_value())
        result = Actions::pass(modMask, key, w);

    Config::Actions::state()->m_passPressed = oldPassPressed;

    return result;
}

ActionResult Actions::global(const std::string& action) {
    const auto COLONPOS = action.find_first_of(':');
    if (COLONPOS == std::string::npos)
        return {};

    const auto APPID = action.substr(0, COLONPOS);
    const auto NAME  = action.substr(COLONPOS + 1);

    if (NAME.empty())
        return {};

    if (!PROTO::globalShortcuts->isTaken(APPID, NAME))
        return {};

    PROTO::globalShortcuts->sendGlobalShortcutEvent(APPID, NAME, Config::Actions::state()->m_passPressed);

    return {};
}

ActionResult Actions::mouse(const std::string& action) {
    const bool PRESSED = Config::Actions::state()->m_passPressed == 1;

    if (!PRESSED)
        return SActionResult{.passEvent = CKeybindManager::changeMouseBindMode(MBIND_INVALID).passEvent};

    if (action == "movewindow")
        return SActionResult{.passEvent = CKeybindManager::changeMouseBindMode(MBIND_MOVE).passEvent};

    // resizewindow with optional ratio mode
    try {
        const auto SPACEPOS = action.find(' ');
        if (SPACEPOS != std::string::npos) {
            switch (std::stoi(action.substr(SPACEPOS + 1))) {
                case 1: return SActionResult{.passEvent = CKeybindManager::changeMouseBindMode(MBIND_RESIZE_FORCE_RATIO).passEvent};
                case 2: return SActionResult{.passEvent = CKeybindManager::changeMouseBindMode(MBIND_RESIZE_BLOCK_RATIO).passEvent};
                default: break;
            }
        }
    } catch (...) { /* stoi failed, fall through to default resize */
    }

    return SActionResult{.passEvent = CKeybindManager::changeMouseBindMode(MBIND_RESIZE).passEvent};
}

ActionResult Actions::setSubmap(const std::string& submap) {
    if (submap == "reset" || submap.empty()) {
        Config::Actions::state()->m_currentSubmap = "";
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "submap", .data = ""});
        Event::bus()->m_events.keybinds.submap.emit(std::string(""));
        return {};
    }

    for (const auto& k : g_pKeybindManager->m_keybinds) {
        if (k->submap.name == submap) {
            Config::Actions::state()->m_currentSubmap = submap;
            g_pEventManager->postEvent(SHyprIPCEvent{.event = "submap", .data = submap});
            Event::bus()->m_events.keybinds.submap.emit(submap);
            return {};
        }
    }

    return std::unexpected(std::format("Cannot set submap {}, submap doesn't exist (wasn't registered!)", submap));
}

ActionResult Actions::cycleNext(const bool next, std::optional<bool> onlyTiled, std::optional<bool> onlyFloating, std::optional<PHLWINDOW> w) {
    auto window = xtract(w);

    if (!window) {
        const auto PWS = Desktop::focusState()->monitor()->m_activeWorkspace;
        if (PWS && PWS->getWindows() > 0) {
            const auto PFIRST = PWS->getFirstWindow();
            switchToWindow(PFIRST);
        }
        return {};
    }

    // If requesting tiled-only and we're on a tiled window, try layout message for supported layouts
    if (onlyTiled.value_or(false) && !window->m_isFloating) {
        if (const auto SPACE = window->layoutTarget()->space(); SPACE) {
            constexpr const std::array<const std::type_info*, 2> LAYOUTS_WITH_CYCLE_NEXT = {
                &typeid(Layout::Tiled::CMonocleAlgorithm),
                &typeid(Layout::Tiled::CMasterAlgorithm),
            };

            if (std::ranges::contains(LAYOUTS_WITH_CYCLE_NEXT, &typeid(*SPACE->algorithm()->tiledAlgo().get()))) {
                // NOLINTNEXTLINE
                Actions::layoutMessage(!next ? "cyclenext, b" : "cyclenext");
                return {};
            }
        }
    }

    std::optional<bool> floatStatus = {};
    if (onlyFloating.value_or(false))
        floatStatus = true;

    const auto& cycled = g_pCompositor->getWindowCycle(window, true, floatStatus, false, !next);

    switchToWindow(cycled);

    return {};
}

ActionResult Actions::moveIntoOrCreateGroup(Math::eDirection dir, std::optional<PHLWINDOW> w) {
    static auto PIGNOREGROUPLOCK = CConfigValue<Hyprlang::INT>("binds:ignore_group_lock");

    if (!*PIGNOREGROUPLOCK && g_pKeybindManager->m_groupsLocked)
        return {};

    if (dir == Math::DIRECTION_DEFAULT) {
        Log::logger->log(Log::ERR, "Cannot move into or create group in direction {}, unsupported direction. Supported: l,r,u/t,d/b", dirToString(dir));
        return std::unexpected(std::format("Cannot move into or create group in direction {}, unsupported direction. Supported: l,r,u/t,d/b", dirToString(dir)));
    }

    const auto PWINDOW = xtract(w);

    if (!PWINDOW)
        return {};

    auto PWINDOWINDIR = g_pCompositor->getWindowInDirection(PWINDOW, dir);

    if (!PWINDOWINDIR)
        return {};

    if (!PWINDOWINDIR->m_group) {
        if (PWINDOWINDIR->isFullscreen())
            return {};

        PWINDOWINDIR->m_group = Desktop::View::CGroup::create({PWINDOWINDIR});
    }

    const auto GROUP = PWINDOWINDIR->m_group;

    if (!*PIGNOREGROUPLOCK && (GROUP->locked() || (PWINDOW->m_group && PWINDOW->m_group->locked())))
        return {};

    moveWindowIntoGroupHelper(PWINDOW, PWINDOWINDIR);

    return {};
}
