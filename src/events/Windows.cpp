#include "Events.hpp"

#include "../Compositor.hpp"
#include "../desktop/Window.hpp"
#include "../helpers/WLClasses.hpp"
#include "../helpers/AsyncDialogBox.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/TokenManager.hpp"
#include "../managers/SeatManager.hpp"
#include "../render/Renderer.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "../protocols/LayerShell.hpp"
#include "../protocols/XDGShell.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../protocols/ToplevelExport.hpp"
#include "../protocols/types/ContentType.hpp"
#include "../xwayland/XSurface.hpp"
#include "managers/AnimationManager.hpp"
#include "managers/PointerManager.hpp"
#include "../desktop/LayerSurface.hpp"
#include "../managers/LayoutManager.hpp"
#include "../managers/EventManager.hpp"
#include "../managers/AnimationManager.hpp"

#include <hyprutils/string/String.hpp>
using namespace Hyprutils::String;
using namespace Hyprutils::Animation;

// ------------------------------------------------------------ //
//  __          _______ _   _ _____   ______          _______   //
//  \ \        / /_   _| \ | |  __ \ / __ \ \        / / ____|  //
//   \ \  /\  / /  | | |  \| | |  | | |  | \ \  /\  / / (___    //
//    \ \/  \/ /   | | | . ` | |  | | |  | |\ \/  \/ / \___ \   //
//     \  /\  /   _| |_| |\  | |__| | |__| | \  /\  /  ____) |  //
//      \/  \/   |_____|_| \_|_____/ \____/   \/  \/  |_____/   //
//                                                              //
// ------------------------------------------------------------ //

static void setVector2DAnimToMove(WP<CBaseAnimatedVariable> pav) {
    const auto PAV = pav.lock();
    if (!PAV)
        return;

    CAnimatedVariable<Vector2D>* animvar = dynamic_cast<CAnimatedVariable<Vector2D>*>(PAV.get());
    animvar->setConfig(g_pConfigManager->getAnimationPropertyConfig("windowsMove"));

    const auto PHLWINDOW = animvar->m_Context.pWindow.lock();
    if (PHLWINDOW)
        PHLWINDOW->m_animatingIn = false;
}

void Events::listener_mapWindow(void* owner, void* data) {
    PHLWINDOW   PWINDOW = ((CWindow*)owner)->m_self.lock();
    if (!PWINDOW) {
        Debug::log(ERR, "mapWindow: PWINDOW is null, owner: {}", owner);
        return;
    }

    if (!PWINDOW->initialize()) {
        Debug::log(ERR, "mapWindow: Failed to initialize window, aborting mapping for {:c}", PWINDOW);
        return;
    }

    static auto PINACTIVEALPHA     = CConfigValue<Hyprlang::FLOAT>("decoration:inactive_opacity");
    static auto PACTIVEALPHA       = CConfigValue<Hyprlang::FLOAT>("decoration:active_opacity");
    static auto PDIMSTRENGTH       = CConfigValue<Hyprlang::FLOAT>("decoration:dim_strength");
    static auto PNEWTAKESOVERFS    = CConfigValue<Hyprlang::INT>("misc:new_window_takes_over_fullscreen");
    static auto PINITIALWSTRACKING = CConfigValue<Hyprlang::INT>("misc:initial_workspace_tracking");

    // check for token
    std::string requestedWorkspace = "";
    bool        workspaceSilent    = false;

    if (*PINITIALWSTRACKING) {
        const auto WINDOWENV = PWINDOW->getEnv();
        if (WINDOWENV.contains("HL_INITIAL_WORKSPACE_TOKEN")) {
            const auto SZTOKEN = WINDOWENV.at("HL_INITIAL_WORKSPACE_TOKEN");
            Debug::log(LOG, "New window contains HL_INITIAL_WORKSPACE_TOKEN: {}", SZTOKEN);
            const auto TOKEN = g_pTokenManager->getToken(SZTOKEN);
            if (TOKEN) {
                // find workspace and use it
                SInitialWorkspaceToken WS = std::any_cast<SInitialWorkspaceToken>(TOKEN->m_data);

                Debug::log(LOG, "HL_INITIAL_WORKSPACE_TOKEN {} -> {}", SZTOKEN, WS.workspace);

                if (g_pCompositor->getWorkspaceByString(WS.workspace) != PWINDOW->m_workspace) {
                    requestedWorkspace = WS.workspace;
                    workspaceSilent    = true;
                }

                if (*PINITIALWSTRACKING == 1) // one-shot token
                    g_pTokenManager->removeToken(TOKEN);
                else if (*PINITIALWSTRACKING == 2) { // persistent
                    if (WS.primaryOwner.expired()) {
                        WS.primaryOwner = PWINDOW;
                        TOKEN->m_data   = WS;
                    }

                    PWINDOW->m_initialWorkspaceToken = SZTOKEN;
                }
            }
        }
    }

    if (g_pInputManager->m_lastFocusOnLS) // waybar fix
        g_pInputManager->releaseAllMouseButtons();

    // checks if the window wants borders and sets the appropriate flag
    g_pXWaylandManager->checkBorders(PWINDOW);

    // registers the animated vars and stuff
    PWINDOW->onMap();

    const auto PWINDOWSURFACE = PWINDOW->m_wlSurface->resource();

    if (!PWINDOWSURFACE) {
        g_pCompositor->removeWindowFromVectorSafe(PWINDOW);
        return;
    }

    if (g_pXWaylandManager->shouldBeFloated(PWINDOW)) {
        PWINDOW->m_isFloating    = true;
        PWINDOW->m_requestsFloat = true;
    }

    PWINDOW->m_X11ShouldntFocus = PWINDOW->m_X11ShouldntFocus || (PWINDOW->m_isX11 && PWINDOW->isX11OverrideRedirect() && !PWINDOW->m_xwaylandSurface->wantsFocus());

    // window rules
    PWINDOW->m_matchedRules = g_pConfigManager->getMatchingRules(PWINDOW, false);
    std::optional<eFullscreenMode>  requestedInternalFSMode, requestedClientFSMode;
    std::optional<SFullscreenState> requestedFSState;
    if (PWINDOW->m_wantsInitialFullscreen || (PWINDOW->m_isX11 && PWINDOW->m_xwaylandSurface->m_fullscreen))
        requestedClientFSMode = FSMODE_FULLSCREEN;
    MONITORID requestedFSMonitor = PWINDOW->m_wantsInitialFullscreenMonitor;

    for (auto const& r : PWINDOW->m_matchedRules) {
        switch (r->m_ruleType) {
            case CWindowRule::RULE_MONITOR: {
                try {
                    const auto MONITORSTR = trim(r->m_rule.substr(r->m_rule.find(' ')));

                    if (MONITORSTR == "unset") {
                        PWINDOW->m_monitor = PMONITOR;
                    } else {
                        if (isNumber(MONITORSTR)) {
                            const MONITORID MONITOR = std::stoi(MONITORSTR);
                            if (const auto PM = g_pCompositor->getMonitorFromID(MONITOR); PM)
                                PWINDOW->m_monitor = PM;
                            else
                                PWINDOW->m_monitor = g_pCompositor->m_monitors.at(0);
                        } else {
                            const auto PMONITOR = g_pCompositor->getMonitorFromName(MONITORSTR);
                            if (PMONITOR)
                                PWINDOW->m_monitor = PMONITOR;
                            else {
                                Debug::log(ERR, "No monitor in monitor {} rule", MONITORSTR);
                                continue;
                            }
                        }
                    }

                    const auto PMONITORFROMID = PWINDOW->m_monitor.lock();

                    if (PWINDOW->m_monitor != PMONITOR) {
                        g_pKeybindManager->m_dispatchers["focusmonitor"](std::to_string(PWINDOW->monitorID()));
                        PMONITOR = PMONITORFROMID;
                    }
                    PWINDOW->m_workspace = PMONITOR->m_activeSpecialWorkspace ? PMONITOR->m_activeSpecialWorkspace : PMONITOR->m_activeWorkspace;
                    PWORKSPACE           = PWINDOW->m_workspace;

                    Debug::log(LOG, "Rule monitor, applying to {:mw}", PWINDOW);
                    requestedFSMonitor = MONITOR_INVALID;
                } catch (std::exception& e) { Debug::log(ERR, "Rule monitor failed, rule: {} -> {} | err: {}", r->m_rule, r->m_value, e.what()); }
                break;
            }
            case CWindowRule::RULE_WORKSPACE: {
                // check if it isnt unset
                const auto WORKSPACERQ = r->m_rule.substr(r->m_rule.find_first_of(' ') + 1);

                if (WORKSPACERQ == "unset")
                    requestedWorkspace = "";
                else
                    requestedWorkspace = WORKSPACERQ;

                const auto JUSTWORKSPACE = WORKSPACERQ.contains(' ') ? WORKSPACERQ.substr(0, WORKSPACERQ.find_first_of(' ')) : WORKSPACERQ;

                if (JUSTWORKSPACE == PWORKSPACE->m_name || JUSTWORKSPACE == "name:" + PWORKSPACE->m_name)
                    requestedWorkspace = "";

                Debug::log(LOG, "Rule workspace matched by {}, {} applied.", PWINDOW, r->m_value);
                requestedFSMonitor = MONITOR_INVALID;
                break;
            }
            case CWindowRule::RULE_FLOAT: {
                PWINDOW->m_isFloating = true;
                break;
            }
            case CWindowRule::RULE_TILE: {
                PWINDOW->m_isFloating = false;
                break;
            }
            case CWindowRule::RULE_PSEUDO: {
                PWINDOW->m_isPseudotiled = true;
                break;
            }
            case CWindowRule::RULE_NOINITIALFOCUS: {
                PWINDOW->m_noInitialFocus = true;
                break;
            }
            case CWindowRule::RULE_FULLSCREENSTATE: {
                const auto ARGS = CVarList(r->m_rule.substr(r->m_rule.find_first_of(' ') + 1), 2, ' ');
                int        internalMode, clientMode;
                try {
                    internalMode = std::stoi(ARGS[0]);
                } catch (std::exception& e) { internalMode = 0; }
                try {
                    clientMode = std::stoi(ARGS[1]);
                } catch (std::exception& e) { clientMode = 0; }
                requestedFSState = SFullscreenState{.internal = (eFullscreenMode)internalMode, .client = (eFullscreenMode)clientMode};
                break;
            }
            case CWindowRule::RULE_SUPPRESSEVENT: {
                CVarList vars(r->m_rule, 0, 's', true);
                for (size_t i = 1; i < vars.size(); ++i) {
                    if (vars[i] == "fullscreen")
                        PWINDOW->m_suppressedEvents |= SUPPRESS_FULLSCREEN;
                    else if (vars[i] == "maximize")
                        PWINDOW->m_suppressedEvents |= SUPPRESS_MAXIMIZE;
                    else if (vars[i] == "activate")
                        PWINDOW->m_suppressedEvents |= SUPPRESS_ACTIVATE;
                    else if (vars[i] == "activatefocus")
                        PWINDOW->m_suppressedEvents |= SUPPRESS_ACTIVATE_FOCUSONLY;
                    else if (vars[i] == "fullscreenoutput")
                        PWINDOW->m_suppressedEvents |= SUPPRESS_FULLSCREEN_OUTPUT;
                    else
                        Debug::log(ERR, "Error while parsing suppressevent windowrule: unknown event type {}", vars[i]);
                }
                break;
            }
            case CWindowRule::RULE_PIN: {
                PWINDOW->m_pinned = true;
                break;
            }
            case CWindowRule::RULE_FULLSCREEN: {
                requestedInternalFSMode = FSMODE_FULLSCREEN;
                break;
            }
            case CWindowRule::RULE_MAXIMIZE: {
                requestedInternalFSMode = FSMODE_MAXIMIZED;
                break;
            }
            case CWindowRule::RULE_STAYFOCUSED: {
                PWINDOW->m_stayFocused = true;
                break;
            }
            case CWindowRule::RULE_GROUP: {
                if (PWINDOW->m_groupRules & GROUP_OVERRIDE)
                    continue;

                // `group` is a shorthand of `group set`
                if (trim(r->m_rule) == "group") {
                    PWINDOW->m_groupRules |= GROUP_SET;
                    continue;
                }

                CVarList    vars(r->m_rule, 0, 's');
                std::string vPrev = "";

                for (auto const& v : vars) {
                    if (v == "group")
                        continue;

                    if (v == "set") {
                        PWINDOW->m_groupRules |= GROUP_SET;
                    } else if (v == "new") {
                        // shorthand for `group barred set`
                        PWINDOW->m_groupRules |= (GROUP_SET | GROUP_BARRED);
                    } else if (v == "lock") {
                        PWINDOW->m_groupRules |= GROUP_LOCK;
                    } else if (v == "invade") {
                        PWINDOW->m_groupRules |= GROUP_INVADE;
                    } else if (v == "barred") {
                        PWINDOW->m_groupRules |= GROUP_BARRED;
                    } else if (v == "deny") {
                        PWINDOW->m_groupData.deny = true;
                    } else if (v == "override") {
                        // Clear existing rules
                        PWINDOW->m_groupRules = GROUP_OVERRIDE;
                    } else if (v == "unset") {
                        // Clear existing rules and stop processing
                        PWINDOW->m_groupRules = GROUP_OVERRIDE;
                        break;
                    } else if (v == "always") {
                        if (vPrev == "set" || vPrev == "group")
                            PWINDOW->m_groupRules |= GROUP_SET_ALWAYS;
                        else if (vPrev == "lock")
                            PWINDOW->m_groupRules |= GROUP_LOCK_ALWAYS;
                        else
                            Debug::log(ERR, "windowrule `group` does not support `{} always`", vPrev);
                    }
                    vPrev = v;
                }
                break;
            }
            case CWindowRule::RULE_CONTENT: {
                const CVarList VARS(r->m_rule, 0, ' ');
                try {
                    PWINDOW->setContentType(NContentType::fromString(VARS[1]));
                } catch (std::exception& e) { Debug::log(ERR, "Rule \"{}\" failed with: {}", r->m_rule, e.what()); }
                break;
            }
            case CWindowRule::RULE_NOCLOSEFOR: {
                const CVarList VARS(r->m_rule, 0, ' ');
                try {
                    PWINDOW->m_closeableSince = Time::steadyNow() + std::chrono::milliseconds(std::stoull(VARS[1]));
                } catch (std::exception& e) { Debug::log(ERR, "Rule \"{}\" failed with: {}", r->m_rule, e.what()); }
            }
            default: break;
        }

        PWINDOW->applyDynamicRule(r);
    }

    // make it uncloseable if it's a Hyprland dialog
    // TODO: make some closeable?
    if (CAsyncDialogBox::isAsyncDialogBox(PWINDOW->getPID()))
        PWINDOW->m_closeableSince = Time::steadyNow() + std::chrono::years(10 /* Should be enough, no? */);

    // disallow tiled pinned
    if (PWINDOW->m_pinned && !PWINDOW->m_isFloating)
        PWINDOW->m_pinned = false;

    CVarList WORKSPACEARGS = CVarList(requestedWorkspace, 0, ' ');

    if (!WORKSPACEARGS[0].empty()) {
        WORKSPACEID requestedWorkspaceID;
        std::string requestedWorkspaceName;
        if (WORKSPACEARGS.contains("silent"))
            workspaceSilent = true;

        if (WORKSPACEARGS.contains("empty") && PWORKSPACE->getWindows() <= 1) {
            requestedWorkspaceID   = PWORKSPACE->m_id;
            requestedWorkspaceName = PWORKSPACE->m_name;
        } else {
            auto result            = getWorkspaceIDNameFromString(WORKSPACEARGS.join(" ", 0, workspaceSilent ? WORKSPACEARGS.size() - 1 : 0));
            requestedWorkspaceID   = result.id;
            requestedWorkspaceName = result.name;
        }

        if (requestedWorkspaceID != WORKSPACE_INVALID) {
            auto pWorkspace = g_pCompositor->getWorkspaceByID(requestedWorkspaceID);

            if (!pWorkspace)
                pWorkspace = g_pCompositor->createNewWorkspace(requestedWorkspaceID, PWINDOW->monitorID(), requestedWorkspaceName, false);

            PWORKSPACE = pWorkspace;

            PWINDOW->m_workspace = pWorkspace;
            PWINDOW->m_monitor   = pWorkspace->m_monitor;

            if (PWINDOW->m_monitor.lock()->m_activeSpecialWorkspace && !pWorkspace->m_isSpecialWorkspace)
                workspaceSilent = true;

            if (!workspaceSilent) {
                if (pWorkspace->m_isSpecialWorkspace)
                    pWorkspace->m_monitor->setSpecialWorkspace(pWorkspace);
                else if (PMONITOR->activeWorkspaceID() != requestedWorkspaceID && !PWINDOW->m_noInitialFocus)
                    g_pKeybindManager->m_dispatchers["workspace"](requestedWorkspaceName);

                PMONITOR = g_pCompositor->m_lastMonitor.lock();
            }

            requestedFSMonitor = MONITOR_INVALID;
        } else
            workspaceSilent = false;
    }

    if (PWINDOW->m_suppressedEvents & SUPPRESS_FULLSCREEN_OUTPUT)
        requestedFSMonitor = MONITOR_INVALID;
    else if (requestedFSMonitor != MONITOR_INVALID) {
        if (const auto PM = g_pCompositor->getMonitorFromID(requestedFSMonitor); PM)
            PWINDOW->m_monitor = PM;

        const auto PMONITORFROMID = PWINDOW->m_monitor.lock();

        if (PWINDOW->m_monitor != PMONITOR) {
            g_pKeybindManager->m_dispatchers["focusmonitor"](std::to_string(PWINDOW->monitorID()));
            PMONITOR = PMONITORFROMID;
        }
        PWINDOW->m_workspace = PMONITOR->m_activeSpecialWorkspace ? PMONITOR->m_activeSpecialWorkspace : PMONITOR->m_activeWorkspace;
        PWORKSPACE           = PWINDOW->m_workspace;

        Debug::log(LOG, "Requested monitor, applying to {:mw}", PWINDOW);
    }

    if (PWORKSPACE->m_defaultFloating)
        PWINDOW->m_isFloating = true;

    if (PWORKSPACE->m_defaultPseudo) {
        PWINDOW->m_isPseudotiled = true;
        CBox desiredGeometry     = g_pXWaylandManager->getGeometryForWindow(PWINDOW);
        PWINDOW->m_pseudoSize    = Vector2D(desiredGeometry.width, desiredGeometry.height);
    }

    PWINDOW->updateWindowData();

    // Verify window swallowing. Get the swallower before calling onWindowCreated(PWINDOW) because getSwallower() wouldn't get it after if PWINDOW gets auto grouped.
    const auto SWALLOWER = PWINDOW->getSwallower();
    PWINDOW->m_swallowed = SWALLOWER;
    if (PWINDOW->m_swallowed)
        PWINDOW->m_swallowed->m_currentlySwallowed = true;

    // emit the IPC event before the layout might focus the window to avoid a focus event first
    g_pEventManager->postEvent(
        SHyprIPCEvent{"openwindow", std::format("{:x},{},{},{}", PWINDOW, requestedWorkspace != "" ? requestedWorkspace : PWORKSPACE->m_name, PWINDOW->m_class, PWINDOW->m_title)});

    if (PWINDOW->m_isFloating) {
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(PWINDOW);
        PWINDOW->m_createdOverFullscreen = true;

        // size and move rules
        for (auto const& r : PWINDOW->m_matchedRules) {
            switch (r->m_ruleType) {
                case CWindowRule::RULE_SIZE: {
                    try {
                        auto stringToFloatClamp = [](const std::string& VALUE, const float CURR, const float REL) {
                            if (VALUE.starts_with('<'))
                                return std::min(CURR, stringToPercentage(VALUE.substr(1, VALUE.length() - 1), REL));
                            else if (VALUE.starts_with('>'))
                                return std::max(CURR, stringToPercentage(VALUE.substr(1, VALUE.length() - 1), REL));

                            return stringToPercentage(VALUE, REL);
                        };

                        const auto  VALUE    = r->m_rule.substr(r->m_rule.find(' ') + 1);
                        const auto  SIZEXSTR = VALUE.substr(0, VALUE.find(' '));
                        const auto  SIZEYSTR = VALUE.substr(VALUE.find(' ') + 1);

                        const auto  MAXSIZE = PWINDOW->requestedMaxSize();

                        const float SIZEX = SIZEXSTR == "max" ? std::clamp(MAXSIZE.x, MIN_WINDOW_SIZE, PMONITOR->m_size.x) :
                                                                stringToFloatClamp(SIZEXSTR, PWINDOW->m_realSize->goal().x, PMONITOR->m_size.x);

                        const float SIZEY = SIZEYSTR == "max" ? std::clamp(MAXSIZE.y, MIN_WINDOW_SIZE, PMONITOR->m_size.y) :
                                                                stringToFloatClamp(SIZEYSTR, PWINDOW->m_realSize->goal().y, PMONITOR->m_size.y);

                        Debug::log(LOG, "Rule size, applying to {}", PWINDOW);

                        PWINDOW->clampWindowSize(Vector2D{SIZEXSTR.starts_with("<") ? 0 : SIZEX, SIZEYSTR.starts_with("<") ? 0 : SIZEY}, Vector2D{SIZEX, SIZEY});

                        PWINDOW->setHidden(false);
                    } catch (...) { Debug::log(LOG, "Rule size failed, rule: {} -> {}", r->m_rule, r->m_value); }
                    break;
                }
                case CWindowRule::RULE_MOVE: {
                    try {
                        auto       value = r->m_rule.substr(r->m_rule.find(' ') + 1);

                        const bool ONSCREEN = value.starts_with("onscreen");

                        if (ONSCREEN)
                            value = value.substr(value.find_first_of(' ') + 1);

                        const bool CURSOR = value.starts_with("cursor");

                        if (CURSOR)
                            value = value.substr(value.find_first_of(' ') + 1);

                        const auto POSXSTR = value.substr(0, value.find(' '));
                        const auto POSYSTR = value.substr(value.find(' ') + 1);

                        int        posX = 0;
                        int        posY = 0;

                        if (POSXSTR.starts_with("100%-")) {
                            const bool subtractWindow = POSXSTR.starts_with("100%-w-");
                            const auto POSXRAW        = (subtractWindow) ? POSXSTR.substr(7) : POSXSTR.substr(5);
                            posX =
                                PMONITOR->m_size.x - (!POSXRAW.contains('%') ? std::stoi(POSXRAW) : std::stof(POSXRAW.substr(0, POSXRAW.length() - 1)) * 0.01 * PMONITOR->m_size.x);

                            if (subtractWindow)
                                posX -= PWINDOW->m_realSize->goal().x;

                            if (CURSOR)
                                Debug::log(ERR, "Cursor is not compatible with 100%-, ignoring cursor!");
                        } else if (!CURSOR) {
                            posX = !POSXSTR.contains('%') ? std::stoi(POSXSTR) : std::stof(POSXSTR.substr(0, POSXSTR.length() - 1)) * 0.01 * PMONITOR->m_size.x;
                        } else {
                            // cursor
                            if (POSXSTR == "cursor") {
                                posX = g_pInputManager->getMouseCoordsInternal().x - PMONITOR->m_position.x;
                            } else {
                                posX = g_pInputManager->getMouseCoordsInternal().x - PMONITOR->m_position.x +
                                    (!POSXSTR.contains('%') ? std::stoi(POSXSTR) : std::stof(POSXSTR.substr(0, POSXSTR.length() - 1)) * 0.01 * PWINDOW->m_realSize->goal().x);
                            }
                        }

                        if (POSYSTR.starts_with("100%-")) {
                            const bool subtractWindow = POSYSTR.starts_with("100%-w-");
                            const auto POSYRAW        = (subtractWindow) ? POSYSTR.substr(7) : POSYSTR.substr(5);
                            posY =
                                PMONITOR->m_size.y - (!POSYRAW.contains('%') ? std::stoi(POSYRAW) : std::stof(POSYRAW.substr(0, POSYRAW.length() - 1)) * 0.01 * PMONITOR->m_size.y);

                            if (subtractWindow)
                                posY -= PWINDOW->m_realSize->goal().y;

                            if (CURSOR)
                                Debug::log(ERR, "Cursor is not compatible with 100%-, ignoring cursor!");
                        } else if (!CURSOR) {
                            posY = !POSYSTR.contains('%') ? std::stoi(POSYSTR) : std::stof(POSYSTR.substr(0, POSYSTR.length() - 1)) * 0.01 * PMONITOR->m_size.y;
                        } else {
                            // cursor
                            if (POSYSTR == "cursor") {
                                posY = g_pInputManager->getMouseCoordsInternal().y - PMONITOR->m_position.y;
                            } else {
                                posY = g_pInputManager->getMouseCoordsInternal().y - PMONITOR->m_position.y +
                                    (!POSYSTR.contains('%') ? std::stoi(POSYSTR) : std::stof(POSYSTR.substr(0, POSYSTR.length() - 1)) * 0.01 * PWINDOW->m_realSize->goal().y);
                            }
                        }

                        if (ONSCREEN) {
                            int borderSize = PWINDOW->getRealBorderSize();

                            posX = std::clamp(posX, (int)(PMONITOR->m_reservedTopLeft.x + borderSize),
                                              (int)(PMONITOR->m_size.x - PMONITOR->m_reservedBottomRight.x - PWINDOW->m_realSize->goal().x - borderSize));

                            posY = std::clamp(posY, (int)(PMONITOR->m_reservedTopLeft.y + borderSize),
                                              (int)(PMONITOR->m_size.y - PMONITOR->m_reservedBottomRight.y - PWINDOW->m_realSize->goal().y - borderSize));
                        }

                        Debug::log(LOG, "Rule move, applying to {}", PWINDOW);

                        *PWINDOW->m_realPosition = Vector2D(posX, posY) + PMONITOR->m_position;

                        PWINDOW->setHidden(false);
                    } catch (...) { Debug::log(LOG, "Rule move failed, rule: {} -> {}", r->m_rule, r->m_value); }
                    break;
                }
                case CWindowRule::RULE_CENTER: {
                    auto       RESERVEDOFFSET = Vector2D();
                    const auto ARGS           = CVarList(r->m_rule, 2, ' ');
                    if (ARGS[1] == "1")
                        RESERVEDOFFSET = (PMONITOR->m_reservedTopLeft - PMONITOR->m_reservedBottomRight) / 2.f;

                    *PWINDOW->m_realPosition = PMONITOR->middle() - PWINDOW->m_realSize->goal() / 2.f + RESERVEDOFFSET;
                    break;
                }

                default: break;
            }
        }

        // set the pseudo size to the GOAL of our current size
        // because the windows are animated on RealSize
        PWINDOW->m_pseudoSize = PWINDOW->m_realSize->goal();

        g_pCompositor->changeWindowZOrder(PWINDOW, true);
    } else {
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(PWINDOW);

        bool setPseudo = false;

        for (auto const& r : PWINDOW->m_matchedRules) {
            if (r->m_ruleType != CWindowRule::RULE_SIZE)
                continue;

            try {
                const auto  VALUE    = r->m_rule.substr(r->m_rule.find(' ') + 1);
                const auto  SIZEXSTR = VALUE.substr(0, VALUE.find(' '));
                const auto  SIZEYSTR = VALUE.substr(VALUE.find(' ') + 1);

                const auto  MAXSIZE = PWINDOW->requestedMaxSize();

                const float SIZEX = SIZEXSTR == "max" ? std::clamp(MAXSIZE.x, MIN_WINDOW_SIZE, PMONITOR->m_size.x) : stringToPercentage(SIZEXSTR, PMONITOR->m_size.x);

                const float SIZEY = SIZEYSTR == "max" ? std::clamp(MAXSIZE.y, MIN_WINDOW_SIZE, PMONITOR->m_size.y) : stringToPercentage(SIZEYSTR, PMONITOR->m_size.y);

                Debug::log(LOG, "Rule size (tiled), applying to {}", PWINDOW);

                setPseudo             = true;
                PWINDOW->m_pseudoSize = Vector2D(SIZEX, SIZEY);

                PWINDOW->setHidden(false);
            } catch (...) { Debug::log(LOG, "Rule size failed, rule: {} -> {}", r->m_rule, r->m_value); }
        }

        if (!setPseudo)
            PWINDOW->m_pseudoSize = PWINDOW->m_realSize->goal() - Vector2D(10, 10);
    }

    const auto PFOCUSEDWINDOWPREV = g_pCompositor->m_lastWindow.lock();

    if (PWINDOW->m_windowData.allowsInput.valueOrDefault()) { // if default value wasn't set to false getPriority() would throw an exception
        PWINDOW->m_windowData.noFocus = CWindowOverridableVar(false, PWINDOW->m_windowData.allowsInput.getPriority());
        PWINDOW->m_noInitialFocus     = false;
        PWINDOW->m_X11ShouldntFocus   = false;
    }

    // check LS focus grab
    const auto PFORCEFOCUS  = g_pCompositor->getForceFocus();
    const auto PLSFROMFOCUS = g_pCompositor->getLayerSurfaceFromSurface(g_pCompositor->m_lastFocus.lock());
    if (PLSFROMFOCUS && PLSFROMFOCUS->m_layerSurface->m_current.interactivity != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
        PWINDOW->m_noInitialFocus = true;

    if (PWINDOW->m_workspace->m_hasFullscreenWindow && !requestedInternalFSMode.has_value() && !requestedClientFSMode.has_value() && !PWINDOW->m_isFloating) {
        if (*PNEWTAKESOVERFS == 0)
            PWINDOW->m_noInitialFocus = true;
        else if (*PNEWTAKESOVERFS == 1)
            requestedInternalFSMode = PWINDOW->m_workspace->m_fullscreenMode;
        else if (*PNEWTAKESOVERFS == 2)
            g_pCompositor->setWindowFullscreenInternal(PWINDOW->m_workspace->getFullscreenWindow(), FSMODE_NONE);
    }

    if (!PWINDOW->m_windowData.noFocus.valueOrDefault() && !PWINDOW->m_noInitialFocus &&
        (!PWINDOW->isX11OverrideRedirect() || (PWINDOW->m_isX11 && PWINDOW->m_xwaylandSurface->wantsFocus())) && !workspaceSilent && (!PFORCEFOCUS || PFORCEFOCUS == PWINDOW) &&
        !g_pInputManager->isConstrained()) {
        g_pCompositor->focusWindow(PWINDOW);
        PWINDOW->m_activeInactiveAlpha->setValueAndWarp(*PACTIVEALPHA);
        PWINDOW->m_dimPercent->setValueAndWarp(PWINDOW->m_windowData.noDim.valueOrDefault() ? 0.f : *PDIMSTRENGTH);
    } else {
        PWINDOW->m_activeInactiveAlpha->setValueAndWarp(*PINACTIVEALPHA);
        PWINDOW->m_dimPercent->setValueAndWarp(0);
    }

    if (requestedClientFSMode.has_value() && (PWINDOW->m_suppressedEvents & SUPPRESS_FULLSCREEN))
        requestedClientFSMode = (eFullscreenMode)((uint8_t)requestedClientFSMode.value_or(FSMODE_NONE) & ~(uint8_t)FSMODE_FULLSCREEN);
    if (requestedClientFSMode.has_value() && (PWINDOW->m_suppressedEvents & SUPPRESS_MAXIMIZE))
        requestedClientFSMode = (eFullscreenMode)((uint8_t)requestedClientFSMode.value_or(FSMODE_NONE) & ~(uint8_t)FSMODE_MAXIMIZED);

    if (!PWINDOW->m_noInitialFocus && (requestedInternalFSMode.has_value() || requestedClientFSMode.has_value() || requestedFSState.has_value())) {
        // fix fullscreen on requested (basically do a switcheroo)
        if (PWINDOW->m_workspace->m_hasFullscreenWindow)
            g_pCompositor->setWindowFullscreenInternal(PWINDOW->m_workspace->getFullscreenWindow(), FSMODE_NONE);

        PWINDOW->m_realPosition->warp();
        PWINDOW->m_realSize->warp();
        if (requestedFSState.has_value()) {
            PWINDOW->m_windowData.syncFullscreen = CWindowOverridableVar(false, PRIORITY_WINDOW_RULE);
            g_pCompositor->setWindowFullscreenState(PWINDOW, requestedFSState.value());
        } else if (requestedInternalFSMode.has_value() && requestedClientFSMode.has_value() && !PWINDOW->m_windowData.syncFullscreen.valueOrDefault())
            g_pCompositor->setWindowFullscreenState(PWINDOW, SFullscreenState{.internal = requestedInternalFSMode.value(), .client = requestedClientFSMode.value()});
        else if (requestedInternalFSMode.has_value())
            g_pCompositor->setWindowFullscreenInternal(PWINDOW, requestedInternalFSMode.value());
        else if (requestedClientFSMode.has_value())
            g_pCompositor->setWindowFullscreenClient(PWINDOW, requestedClientFSMode.value());
    }

    // recheck idle inhibitors
    g_pInputManager->recheckIdleInhibitorStatus();

    PWINDOW->updateToplevel();

    if (workspaceSilent) {
        if (validMapped(PFOCUSEDWINDOWPREV)) {
            g_pCompositor->focusWindow(PFOCUSEDWINDOWPREV);
            PFOCUSEDWINDOWPREV->updateWindowDecos(); // need to for some reason i cba to find out why
        } else if (!PFOCUSEDWINDOWPREV)
            g_pCompositor->focusWindow(nullptr);
    }

    // swallow
    if (SWALLOWER) {
        g_pLayoutManager->getCurrentLayout()->onWindowRemoved(SWALLOWER);
        g_pHyprRenderer->damageWindow(SWALLOWER);
        SWALLOWER->setHidden(true);
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PWINDOW->monitorID());
    }

    PWINDOW->m_firstMap = false;

    Debug::log(LOG, "Map request dispatched, monitor {}, window pos: {:5j}, window size: {:5j}", PMONITOR->m_name, PWINDOW->m_realPosition->goal(), PWINDOW->m_realSize->goal());

    // emit the hook event here after basic stuff has been initialized
    EMIT_HOOK_EVENT("openWindow", PWINDOW);

    // apply data from default decos. Borders, shadows.
    g_pDecorationPositioner->forceRecalcFor(PWINDOW);
    PWINDOW->updateWindowDecos();
    g_pLayoutManager->getCurrentLayout()->recalculateWindow(PWINDOW);

    // do animations
    g_pAnimationManager->onWindowPostCreateClose(PWINDOW, false);
    PWINDOW->m_alpha->setValueAndWarp(0.f);
    *PWINDOW->m_alpha = 1.f;

    PWINDOW->m_realPosition->setCallbackOnEnd(setVector2DAnimToMove);
    PWINDOW->m_realSize->setCallbackOnEnd(setVector2DAnimToMove);

    // recalc the values for this window
    g_pCompositor->updateWindowAnimatedDecorationValues(PWINDOW);
    // avoid this window being visible
    if (PWORKSPACE->m_hasFullscreenWindow && !PWINDOW->isFullscreen() && !PWINDOW->m_isFloating)
        PWINDOW->m_alpha->setValueAndWarp(0.f);

    g_pCompositor->setPreferredScaleForSurface(PWINDOW->m_wlSurface->resource(), PMONITOR->m_scale);
    g_pCompositor->setPreferredTransformForSurface(PWINDOW->m_wlSurface->resource(), PMONITOR->m_transform);

    if (g_pSeatManager->m_mouse.expired() || !g_pInputManager->isConstrained())
        g_pInputManager->sendMotionEventsToFocused();

    // fix some xwayland apps that don't behave nicely
    PWINDOW->m_reportedSize = PWINDOW->m_pendingReportedSize;

    if (PWINDOW->m_workspace)
        PWINDOW->m_workspace->updateWindows();

    if (PMONITOR && PWINDOW->isX11OverrideRedirect())
        PWINDOW->m_X11SurfaceScaledBy = PMONITOR->m_scale;
}

void Events::listener_unmapWindow(void* owner, void* data) {
    PHLWINDOW PWINDOW = ((CWindow*)owner)->m_self.lock();

    Debug::log(LOG, "{:c} unmapped", PWINDOW);

    static auto PEXITRETAINSFS = CConfigValue<Hyprlang::INT>("misc:exit_window_retains_fullscreen");

    const auto  CURRENTWINDOWFSSTATE = PWINDOW->isFullscreen();
    const auto  CURRENTFSMODE        = PWINDOW->m_fullscreenState.internal;

    if (!PWINDOW->m_wlSurface->exists() || !PWINDOW->m_isMapped) {
        Debug::log(WARN, "{} unmapped without being mapped??", PWINDOW);
        PWINDOW->m_fadingOut = false;
        return;
    }

    const auto PMONITOR = PWINDOW->m_monitor.lock();
    if (PMONITOR) {
        PWINDOW->m_originalClosedPos     = PWINDOW->m_realPosition->value() - PMONITOR->m_position;
        PWINDOW->m_originalClosedSize    = PWINDOW->m_realSize->value();
        PWINDOW->m_originalClosedExtents = PWINDOW->getFullWindowExtents();
    }

    g_pEventManager->postEvent(SHyprIPCEvent{"closewindow", std::format("{:x}", PWINDOW)});
    EMIT_HOOK_EVENT("closeWindow", PWINDOW);

    if (PWINDOW->m_isFloating && !PWINDOW->m_isX11 &&
        std::any_of(PWINDOW->m_matchedRules.begin(), PWINDOW->m_matchedRules.end(), [](const auto& r) { return r->m_ruleType == CWindowRule::RULE_PERSISTENTSIZE; })) {
        Debug::log(LOG, "storing floating size {}x{} for window {}::{} on close", PWINDOW->m_realSize->value().x, PWINDOW->m_realSize->value().y, PWINDOW->m_class,
                   PWINDOW->m_title);
        g_pConfigManager->storeFloatingSize(PWINDOW, PWINDOW->m_realSize->value());
    }

    PROTO::toplevelExport->onWindowUnmap(PWINDOW);

    if (PWINDOW->isFullscreen())
        g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);

    // Allow the renderer to catch the last frame.
    if (g_pHyprRenderer->shouldRenderWindow(PWINDOW))
        g_pHyprRenderer->makeWindowSnapshot(PWINDOW);

    // swallowing
    if (valid(PWINDOW->m_swallowed)) {
        if (PWINDOW->m_swallowed->m_currentlySwallowed) {
            PWINDOW->m_swallowed->m_currentlySwallowed = false;
            PWINDOW->m_swallowed->setHidden(false);

            if (PWINDOW->m_groupData.pNextWindow.lock())
                PWINDOW->m_swallowed->m_groupSwallowed = true; // flag for the swallowed window to be created into the group where it belongs when auto_group = false.

            g_pLayoutManager->getCurrentLayout()->onWindowCreated(PWINDOW->m_swallowed.lock());
        }

        PWINDOW->m_swallowed->m_groupSwallowed = false;
        PWINDOW->m_swallowed.reset();
    }

    bool wasLastWindow = false;

    if (PWINDOW == g_pCompositor->m_lastWindow.lock()) {
        wasLastWindow = true;
        g_pCompositor->m_lastWindow.reset();
        g_pCompositor->m_lastFocus.reset();

        g_pInputManager->releaseAllMouseButtons();
    }

    if (PWINDOW == g_pInputManager->m_currentlyDraggedWindow.lock())
        g_pKeybindManager->changeMouseBindMode(MBIND_INVALID);

    // remove the fullscreen window status from workspace if we closed it
    const auto PWORKSPACE = PWINDOW->m_workspace;

    if (PWORKSPACE->m_hasFullscreenWindow && PWINDOW->isFullscreen())
        PWORKSPACE->m_hasFullscreenWindow = false;

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(PWINDOW);

    g_pHyprRenderer->damageWindow(PWINDOW);

    // do this after onWindowRemoved because otherwise it'll think the window is invalid
    PWINDOW->m_isMapped = false;

    // refocus on a new window if needed
    if (wasLastWindow) {
        static auto FOCUSONCLOSE     = CConfigValue<Hyprlang::INT>("input:focus_on_close");
        PHLWINDOW   PWINDOWCANDIDATE = nullptr;
        if (*FOCUSONCLOSE)
            PWINDOWCANDIDATE = (g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(), RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING));
        else
            PWINDOWCANDIDATE = g_pLayoutManager->getCurrentLayout()->getNextWindowCandidate(PWINDOW);

        Debug::log(LOG, "On closed window, new focused candidate is {}", PWINDOWCANDIDATE);

        if (PWINDOWCANDIDATE != g_pCompositor->m_lastWindow.lock() && PWINDOWCANDIDATE) {
            g_pCompositor->focusWindow(PWINDOWCANDIDATE);
            if (*PEXITRETAINSFS && CURRENTWINDOWFSSTATE)
                g_pCompositor->setWindowFullscreenInternal(PWINDOWCANDIDATE, CURRENTFSMODE);
        }

        if (!PWINDOWCANDIDATE && PWINDOW->m_workspace && PWINDOW->m_workspace->getWindows() == 0)
            g_pInputManager->refocus();

        g_pInputManager->sendMotionEventsToFocused();

        // CWindow::onUnmap will remove this window's active status, but we can't really do it above.
        if (PWINDOW == g_pCompositor->m_lastWindow.lock() || !g_pCompositor->m_lastWindow.lock()) {
            g_pEventManager->postEvent(SHyprIPCEvent{"activewindow", ","});
            g_pEventManager->postEvent(SHyprIPCEvent{"activewindowv2", ""});
            EMIT_HOOK_EVENT("activeWindow", (PHLWINDOW) nullptr);
        }
    } else {
        Debug::log(LOG, "Unmapped was not focused, ignoring a refocus.");
    }

    PWINDOW->m_fadingOut = true;

    g_pCompositor->addToFadingOutSafe(PWINDOW);

    if (!PWINDOW->m_X11DoesntWantBorders)                                                     // don't animate out if they weren't animated in.
        *PWINDOW->m_realPosition = PWINDOW->m_realPosition->value() + Vector2D(0.01f, 0.01f); // it has to be animated, otherwise onWindowPostCreateClose will ignore it

    // anims
    g_pAnimationManager->onWindowPostCreateClose(PWINDOW, true);
    *PWINDOW->m_alpha = 0.f;

    // recheck idle inhibitors
    g_pInputManager->recheckIdleInhibitorStatus();

    // force report all sizes (QT sometimes has an issue with this)
    if (PWINDOW->m_workspace)
        PWINDOW->m_workspace->forceReportSizesToWindows();

    // update lastwindow after focus
    PWINDOW->onUnmap();
}

void Events::listener_commitWindow(void* owner, void* data) {
    PHLWINDOW PWINDOW = ((CWindow*)owner)->m_self.lock();

    if (!PWINDOW->m_isX11 && PWINDOW->m_xdgSurface->m_initialCommit) {
        Vector2D predSize = g_pLayoutManager->getCurrentLayout()->predictSizeForNewWindow(PWINDOW);

        Debug::log(LOG, "Layout predicts size {} for {}", predSize, PWINDOW);

        PWINDOW->m_xdgSurface->m_toplevel->setSize(predSize);
        return;
    }

    if (!PWINDOW->m_isMapped || PWINDOW->isHidden())
        return;

    PWINDOW->m_reportedSize = PWINDOW->m_pendingReportedSize; // apply pending size. We pinged, the window ponged.

    if (!PWINDOW->m_isX11 && !PWINDOW->isFullscreen() && PWINDOW->m_isFloating) {
        const auto MINSIZE = PWINDOW->m_xdgSurface->m_toplevel->layoutMinSize();
        const auto MAXSIZE = PWINDOW->m_xdgSurface->m_toplevel->layoutMaxSize();

        PWINDOW->clampWindowSize(MINSIZE, MAXSIZE > Vector2D{1, 1} ? std::optional<Vector2D>{MAXSIZE} : std::nullopt);
        g_pHyprRenderer->damageWindow(PWINDOW);
    }

    if (!PWINDOW->m_workspace->m_visible)
        return;

    const auto PMONITOR = PWINDOW->m_monitor.lock();

    if (PMONITOR)
        PMONITOR->debugLastPresentation(g_pSeatManager->m_isPointerFrameCommit ? "listener_commitWindow skip" : "listener_commitWindow");

    if (g_pSeatManager->m_isPointerFrameCommit) {
        g_pSeatManager->m_isPointerFrameSkipped = false;
        g_pSeatManager->m_isPointerFrameCommit  = false;
    } else
        g_pHyprRenderer->damageSurface(PWINDOW->m_wlSurface->resource(), PWINDOW->m_realPosition->goal().x, PWINDOW->m_realPosition->goal().y,
                                       PWINDOW->m_isX11 ? 1.0 / PWINDOW->m_X11SurfaceScaledBy : 1.0);

    if (g_pSeatManager->m_isPointerFrameSkipped) {
        g_pPointerManager->sendStoredMovement();
        g_pSeatManager->sendPointerFrame();
        g_pSeatManager->m_isPointerFrameCommit = true;
    }

    if (!PWINDOW->m_isX11) {
        PWINDOW->m_subsurfaceHead->recheckDamageForSubsurfaces();
        PWINDOW->m_popupHead->recheckTree();
    }

    // tearing: if solitary, redraw it. This still might be a single surface window
    if (PMONITOR && PMONITOR->m_solitaryClient.lock() == PWINDOW && PWINDOW->canBeTorn() && PMONITOR->m_tearingState.canTear &&
        PWINDOW->m_wlSurface->resource()->m_current.texture) {
        CRegion damageBox{PWINDOW->m_wlSurface->resource()->m_current.accumulateBufferDamage()};

        if (!damageBox.empty()) {
            if (PMONITOR->m_tearingState.busy) {
                PMONITOR->m_tearingState.frameScheduledWhileBusy = true;
            } else {
                PMONITOR->m_tearingState.nextRenderTorn = true;
                g_pHyprRenderer->renderMonitor(PMONITOR);
            }
        }
    }
}

void Events::listener_destroyWindow(void* owner, void* data) {
    PHLWINDOW PWINDOW = ((CWindow*)owner)->m_self.lock();

    Debug::log(LOG, "{:c} destroyed, queueing.", PWINDOW);

    if (PWINDOW == g_pCompositor->m_lastWindow.lock()) {
        g_pCompositor->m_lastWindow.reset();
        g_pCompositor->m_lastFocus.reset();
    }

    PWINDOW->m_wlSurface->unassign();

    PWINDOW->m_listeners = {};

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(PWINDOW);

    PWINDOW->m_readyToDelete = true;

    PWINDOW->m_xdgSurface.reset();

    if (!PWINDOW->m_fadingOut) {
        Debug::log(LOG, "Unmapped {} removed instantly", PWINDOW);
        g_pCompositor->removeWindowFromVectorSafe(PWINDOW); // most likely X11 unmanaged or sumn
    }

    PWINDOW->m_listeners.unmap.reset();
    PWINDOW->m_listeners.destroy.reset();
    PWINDOW->m_listeners.map.reset();
    PWINDOW->m_listeners.commit.reset();
}

void Events::listener_activateX11(void* owner, void* data) {
    PHLWINDOW PWINDOW = ((CWindow*)owner)->m_self.lock();

    Debug::log(LOG, "X11 Activate request for window {}", PWINDOW);

    if (PWINDOW->isX11OverrideRedirect()) {

        Debug::log(LOG, "Unmanaged X11 {} requests activate", PWINDOW);

        if (g_pCompositor->m_lastWindow.lock() && g_pCompositor->m_lastWindow->getPID() != PWINDOW->getPID())
            return;

        if (!PWINDOW->m_xwaylandSurface->wantsFocus())
            return;

        g_pCompositor->focusWindow(PWINDOW);
        return;
    }

    if (PWINDOW == g_pCompositor->m_lastWindow.lock() || (PWINDOW->m_suppressedEvents & SUPPRESS_ACTIVATE))
        return;

    PWINDOW->activate();
}

void Events::listener_unmanagedSetGeometry(void* owner, void* data) {
    PHLWINDOW PWINDOW = ((CWindow*)owner)->m_self.lock();

    if (!PWINDOW->m_isMapped || !PWINDOW->m_xwaylandSurface || !PWINDOW->m_xwaylandSurface->m_overrideRedirect)
        return;

    const auto POS = PWINDOW->m_realPosition->goal();
    const auto SIZ = PWINDOW->m_realSize->goal();

    if (PWINDOW->m_xwaylandSurface->m_geometry.size() > Vector2D{1, 1})
        PWINDOW->setHidden(false);
    else
        PWINDOW->setHidden(true);

    if (PWINDOW->isFullscreen() || !PWINDOW->m_isFloating) {
        PWINDOW->sendWindowSize(true);
        g_pHyprRenderer->damageWindow(PWINDOW);
        return;
    }

    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    const auto  LOGICALPOS = g_pXWaylandManager->xwaylandToWaylandCoords(PWINDOW->m_xwaylandSurface->m_geometry.pos());

    if (abs(std::floor(POS.x) - LOGICALPOS.x) > 2 || abs(std::floor(POS.y) - LOGICALPOS.y) > 2 || abs(std::floor(SIZ.x) - PWINDOW->m_xwaylandSurface->m_geometry.width) > 2 ||
        abs(std::floor(SIZ.y) - PWINDOW->m_xwaylandSurface->m_geometry.height) > 2) {
        Debug::log(LOG, "Unmanaged window {} requests geometry update to {:j} {:j}", PWINDOW, LOGICALPOS, PWINDOW->m_xwaylandSurface->m_geometry.size());

        g_pHyprRenderer->damageWindow(PWINDOW);
        PWINDOW->m_realPosition->setValueAndWarp(Vector2D(LOGICALPOS.x, LOGICALPOS.y));

        if (abs(std::floor(SIZ.x) - PWINDOW->m_xwaylandSurface->m_geometry.w) > 2 || abs(std::floor(SIZ.y) - PWINDOW->m_xwaylandSurface->m_geometry.h) > 2)
            PWINDOW->m_realSize->setValueAndWarp(PWINDOW->m_xwaylandSurface->m_geometry.size());

        if (*PXWLFORCESCALEZERO) {
            if (const auto PMONITOR = PWINDOW->m_monitor.lock(); PMONITOR) {
                PWINDOW->m_realSize->setValueAndWarp(PWINDOW->m_realSize->goal() / PMONITOR->m_scale);
            }
        }

        PWINDOW->m_position = PWINDOW->m_realPosition->goal();
        PWINDOW->m_size     = PWINDOW->m_realSize->goal();

        PWINDOW->m_workspace = g_pCompositor->getMonitorFromVector(PWINDOW->m_realPosition->value() + PWINDOW->m_realSize->value() / 2.f)->m_activeWorkspace;

        g_pCompositor->changeWindowZOrder(PWINDOW, true);
        PWINDOW->updateWindowDecos();
        g_pHyprRenderer->damageWindow(PWINDOW);

        PWINDOW->m_reportedPosition    = PWINDOW->m_realPosition->goal();
        PWINDOW->m_pendingReportedSize = PWINDOW->m_realSize->goal();
    }
}
