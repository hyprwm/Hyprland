#include "Events.hpp"

#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/TokenManager.hpp"
#include "../managers/SeatManager.hpp"
#include "../render/Renderer.hpp"
#include "../config/ConfigValue.hpp"
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
        PHLWINDOW->m_bAnimatingIn = false;
}

void Events::listener_mapWindow(void* owner, void* data) {
    PHLWINDOW   PWINDOW = ((CWindow*)owner)->m_pSelf.lock();

    static auto PINACTIVEALPHA     = CConfigValue<Hyprlang::FLOAT>("decoration:inactive_opacity");
    static auto PACTIVEALPHA       = CConfigValue<Hyprlang::FLOAT>("decoration:active_opacity");
    static auto PDIMSTRENGTH       = CConfigValue<Hyprlang::FLOAT>("decoration:dim_strength");
    static auto PNEWTAKESOVERFS    = CConfigValue<Hyprlang::INT>("misc:new_window_takes_over_fullscreen");
    static auto PINITIALWSTRACKING = CConfigValue<Hyprlang::INT>("misc:initial_workspace_tracking");

    auto        PMONITOR = g_pCompositor->m_pLastMonitor.lock();
    if (!g_pCompositor->m_pLastMonitor) {
        g_pCompositor->setActiveMonitor(g_pCompositor->getMonitorFromVector({}));
        PMONITOR = g_pCompositor->m_pLastMonitor.lock();
    }
    auto PWORKSPACE           = PMONITOR->activeSpecialWorkspace ? PMONITOR->activeSpecialWorkspace : PMONITOR->activeWorkspace;
    PWINDOW->m_pMonitor       = PMONITOR;
    PWINDOW->m_pWorkspace     = PWORKSPACE;
    PWINDOW->m_bIsMapped      = true;
    PWINDOW->m_bReadyToDelete = false;
    PWINDOW->m_bFadingOut     = false;
    PWINDOW->m_szTitle        = PWINDOW->fetchTitle();
    PWINDOW->m_bFirstMap      = true;
    PWINDOW->m_szInitialTitle = PWINDOW->m_szTitle;
    PWINDOW->m_szInitialClass = PWINDOW->fetchClass();

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
                SInitialWorkspaceToken WS = std::any_cast<SInitialWorkspaceToken>(TOKEN->data);

                Debug::log(LOG, "HL_INITIAL_WORKSPACE_TOKEN {} -> {}", SZTOKEN, WS.workspace);

                if (g_pCompositor->getWorkspaceByString(WS.workspace) != PWINDOW->m_pWorkspace) {
                    requestedWorkspace = WS.workspace;
                    workspaceSilent    = true;
                }

                if (*PINITIALWSTRACKING == 1) // one-shot token
                    g_pTokenManager->removeToken(TOKEN);
                else if (*PINITIALWSTRACKING == 2) { // persistent
                    if (WS.primaryOwner.expired()) {
                        WS.primaryOwner = PWINDOW;
                        TOKEN->data     = WS;
                    }

                    PWINDOW->m_szInitialWorkspaceToken = SZTOKEN;
                }
            }
        }
    }

    if (g_pInputManager->m_bLastFocusOnLS) // waybar fix
        g_pInputManager->releaseAllMouseButtons();

    // checks if the window wants borders and sets the appropriate flag
    g_pXWaylandManager->checkBorders(PWINDOW);

    // registers the animated vars and stuff
    PWINDOW->onMap();

    const auto PWINDOWSURFACE = PWINDOW->m_pWLSurface->resource();

    if (!PWINDOWSURFACE) {
        g_pCompositor->removeWindowFromVectorSafe(PWINDOW);
        return;
    }

    if (g_pXWaylandManager->shouldBeFloated(PWINDOW)) {
        PWINDOW->m_bIsFloating    = true;
        PWINDOW->m_bRequestsFloat = true;
    }

    PWINDOW->m_bX11ShouldntFocus = PWINDOW->m_bX11ShouldntFocus || (PWINDOW->m_bIsX11 && PWINDOW->isX11OverrideRedirect() && !PWINDOW->m_pXWaylandSurface->wantsFocus());

    // window rules
    PWINDOW->m_vMatchedRules = g_pConfigManager->getMatchingRules(PWINDOW, false);
    std::optional<eFullscreenMode>  requestedInternalFSMode, requestedClientFSMode;
    std::optional<SFullscreenState> requestedFSState;
    if (PWINDOW->m_bWantsInitialFullscreen || (PWINDOW->m_bIsX11 && PWINDOW->m_pXWaylandSurface->fullscreen))
        requestedClientFSMode = FSMODE_FULLSCREEN;
    MONITORID requestedFSMonitor = PWINDOW->m_iWantsInitialFullscreenMonitor;

    for (auto const& r : PWINDOW->m_vMatchedRules) {
        switch (r->ruleType) {
            case CWindowRule::RULE_MONITOR: {
                try {
                    const auto MONITORSTR = trim(r->szRule.substr(r->szRule.find(' ')));

                    if (MONITORSTR == "unset") {
                        PWINDOW->m_pMonitor = PMONITOR;
                    } else {
                        if (isNumber(MONITORSTR)) {
                            const MONITORID MONITOR = std::stoi(MONITORSTR);
                            if (const auto PM = g_pCompositor->getMonitorFromID(MONITOR); PM)
                                PWINDOW->m_pMonitor = PM;
                            else
                                PWINDOW->m_pMonitor = g_pCompositor->m_vMonitors.at(0);
                        } else {
                            const auto PMONITOR = g_pCompositor->getMonitorFromName(MONITORSTR);
                            if (PMONITOR)
                                PWINDOW->m_pMonitor = PMONITOR;
                            else {
                                Debug::log(ERR, "No monitor in monitor {} rule", MONITORSTR);
                                continue;
                            }
                        }
                    }

                    const auto PMONITORFROMID = PWINDOW->m_pMonitor.lock();

                    if (PWINDOW->m_pMonitor != PMONITOR) {
                        g_pKeybindManager->m_mDispatchers["focusmonitor"](std::to_string(PWINDOW->monitorID()));
                        PMONITOR = PMONITORFROMID;
                    }
                    PWINDOW->m_pWorkspace = PMONITOR->activeSpecialWorkspace ? PMONITOR->activeSpecialWorkspace : PMONITOR->activeWorkspace;
                    PWORKSPACE            = PWINDOW->m_pWorkspace;

                    Debug::log(LOG, "Rule monitor, applying to {:mw}", PWINDOW);
                    requestedFSMonitor = MONITOR_INVALID;
                } catch (std::exception& e) { Debug::log(ERR, "Rule monitor failed, rule: {} -> {} | err: {}", r->szRule, r->szValue, e.what()); }
                break;
            }
            case CWindowRule::RULE_WORKSPACE: {
                // check if it isnt unset
                const auto WORKSPACERQ = r->szRule.substr(r->szRule.find_first_of(' ') + 1);

                if (WORKSPACERQ == "unset")
                    requestedWorkspace = "";
                else
                    requestedWorkspace = WORKSPACERQ;

                const auto JUSTWORKSPACE = WORKSPACERQ.contains(' ') ? WORKSPACERQ.substr(0, WORKSPACERQ.find_first_of(' ')) : WORKSPACERQ;

                if (JUSTWORKSPACE == PWORKSPACE->m_szName || JUSTWORKSPACE == "name:" + PWORKSPACE->m_szName)
                    requestedWorkspace = "";

                Debug::log(LOG, "Rule workspace matched by {}, {} applied.", PWINDOW, r->szValue);
                requestedFSMonitor = MONITOR_INVALID;
                break;
            }
            case CWindowRule::RULE_FLOAT: {
                PWINDOW->m_bIsFloating = true;
                break;
            }
            case CWindowRule::RULE_TILE: {
                PWINDOW->m_bIsFloating = false;
                break;
            }
            case CWindowRule::RULE_PSEUDO: {
                PWINDOW->m_bIsPseudotiled = true;
                break;
            }
            case CWindowRule::RULE_NOINITIALFOCUS: {
                PWINDOW->m_bNoInitialFocus = true;
                break;
            }
            case CWindowRule::RULE_FULLSCREENSTATE: {
                const auto ARGS = CVarList(r->szRule.substr(r->szRule.find_first_of(' ') + 1), 2, ' ');
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
                CVarList vars(r->szRule, 0, 's', true);
                for (size_t i = 1; i < vars.size(); ++i) {
                    if (vars[i] == "fullscreen")
                        PWINDOW->m_eSuppressedEvents |= SUPPRESS_FULLSCREEN;
                    else if (vars[i] == "maximize")
                        PWINDOW->m_eSuppressedEvents |= SUPPRESS_MAXIMIZE;
                    else if (vars[i] == "activate")
                        PWINDOW->m_eSuppressedEvents |= SUPPRESS_ACTIVATE;
                    else if (vars[i] == "activatefocus")
                        PWINDOW->m_eSuppressedEvents |= SUPPRESS_ACTIVATE_FOCUSONLY;
                    else if (vars[i] == "fullscreenoutput")
                        PWINDOW->m_eSuppressedEvents |= SUPPRESS_FULLSCREEN_OUTPUT;
                    else
                        Debug::log(ERR, "Error while parsing suppressevent windowrule: unknown event type {}", vars[i]);
                }
                break;
            }
            case CWindowRule::RULE_PIN: {
                PWINDOW->m_bPinned = true;
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
                PWINDOW->m_bStayFocused = true;
                break;
            }
            case CWindowRule::RULE_GROUP: {
                if (PWINDOW->m_eGroupRules & GROUP_OVERRIDE)
                    continue;

                // `group` is a shorthand of `group set`
                if (trim(r->szRule) == "group") {
                    PWINDOW->m_eGroupRules |= GROUP_SET;
                    continue;
                }

                CVarList    vars(r->szRule, 0, 's');
                std::string vPrev = "";

                for (auto const& v : vars) {
                    if (v == "group")
                        continue;

                    if (v == "set") {
                        PWINDOW->m_eGroupRules |= GROUP_SET;
                    } else if (v == "new") {
                        // shorthand for `group barred set`
                        PWINDOW->m_eGroupRules |= (GROUP_SET | GROUP_BARRED);
                    } else if (v == "lock") {
                        PWINDOW->m_eGroupRules |= GROUP_LOCK;
                    } else if (v == "invade") {
                        PWINDOW->m_eGroupRules |= GROUP_INVADE;
                    } else if (v == "barred") {
                        PWINDOW->m_eGroupRules |= GROUP_BARRED;
                    } else if (v == "deny") {
                        PWINDOW->m_sGroupData.deny = true;
                    } else if (v == "override") {
                        // Clear existing rules
                        PWINDOW->m_eGroupRules = GROUP_OVERRIDE;
                    } else if (v == "unset") {
                        // Clear existing rules and stop processing
                        PWINDOW->m_eGroupRules = GROUP_OVERRIDE;
                        break;
                    } else if (v == "always") {
                        if (vPrev == "set" || vPrev == "group")
                            PWINDOW->m_eGroupRules |= GROUP_SET_ALWAYS;
                        else if (vPrev == "lock")
                            PWINDOW->m_eGroupRules |= GROUP_LOCK_ALWAYS;
                        else
                            Debug::log(ERR, "windowrule `group` does not support `{} always`", vPrev);
                    }
                    vPrev = v;
                }
                break;
            }
            case CWindowRule::RULE_CONTENT: {
                const CVarList VARS(r->szRule, 0, ' ');
                try {
                    PWINDOW->setContentType(NContentType::fromString(VARS[1]));
                } catch (std::exception& e) { Debug::log(ERR, "Rule \"{}\" failed with: {}", r->szRule, e.what()); }
                break;
            }
            default: break;
        }

        PWINDOW->applyDynamicRule(r);
    }

    // disallow tiled pinned
    if (PWINDOW->m_bPinned && !PWINDOW->m_bIsFloating)
        PWINDOW->m_bPinned = false;

    const CVarList WORKSPACEARGS = CVarList(requestedWorkspace, 0, ' ');

    if (!WORKSPACEARGS[0].empty()) {
        if (WORKSPACEARGS[WORKSPACEARGS.size() - 1].starts_with("silent"))
            workspaceSilent = true;

        const auto& [REQUESTEDWORKSPACEID, requestedWorkspaceName] = getWorkspaceIDNameFromString(WORKSPACEARGS.join(" ", 0, workspaceSilent ? WORKSPACEARGS.size() - 1 : 0));

        if (REQUESTEDWORKSPACEID != WORKSPACE_INVALID) {
            auto pWorkspace = g_pCompositor->getWorkspaceByID(REQUESTEDWORKSPACEID);

            if (!pWorkspace)
                pWorkspace = g_pCompositor->createNewWorkspace(REQUESTEDWORKSPACEID, PWINDOW->monitorID(), requestedWorkspaceName, false);

            PWORKSPACE = pWorkspace;

            PWINDOW->m_pWorkspace = pWorkspace;
            PWINDOW->m_pMonitor   = pWorkspace->m_pMonitor;

            if (PWINDOW->m_pMonitor.lock()->activeSpecialWorkspace && !pWorkspace->m_bIsSpecialWorkspace)
                workspaceSilent = true;

            if (!workspaceSilent) {
                if (pWorkspace->m_bIsSpecialWorkspace)
                    pWorkspace->m_pMonitor->setSpecialWorkspace(pWorkspace);
                else if (PMONITOR->activeWorkspaceID() != REQUESTEDWORKSPACEID)
                    g_pKeybindManager->m_mDispatchers["workspace"](requestedWorkspaceName);

                PMONITOR = g_pCompositor->m_pLastMonitor.lock();
            }

            requestedFSMonitor = MONITOR_INVALID;
        } else
            workspaceSilent = false;
    }

    if (PWINDOW->m_eSuppressedEvents & SUPPRESS_FULLSCREEN_OUTPUT)
        requestedFSMonitor = MONITOR_INVALID;
    else if (requestedFSMonitor != MONITOR_INVALID) {
        if (const auto PM = g_pCompositor->getMonitorFromID(requestedFSMonitor); PM)
            PWINDOW->m_pMonitor = PM;

        const auto PMONITORFROMID = PWINDOW->m_pMonitor.lock();

        if (PWINDOW->m_pMonitor != PMONITOR) {
            g_pKeybindManager->m_mDispatchers["focusmonitor"](std::to_string(PWINDOW->monitorID()));
            PMONITOR = PMONITORFROMID;
        }
        PWINDOW->m_pWorkspace = PMONITOR->activeSpecialWorkspace ? PMONITOR->activeSpecialWorkspace : PMONITOR->activeWorkspace;
        PWORKSPACE            = PWINDOW->m_pWorkspace;

        Debug::log(LOG, "Requested monitor, applying to {:mw}", PWINDOW);
    }

    if (PWORKSPACE->m_bDefaultFloating)
        PWINDOW->m_bIsFloating = true;

    if (PWORKSPACE->m_bDefaultPseudo) {
        PWINDOW->m_bIsPseudotiled = true;
        CBox desiredGeometry      = g_pXWaylandManager->getGeometryForWindow(PWINDOW);
        PWINDOW->m_vPseudoSize    = Vector2D(desiredGeometry.width, desiredGeometry.height);
    }

    PWINDOW->updateWindowData();

    // Verify window swallowing. Get the swallower before calling onWindowCreated(PWINDOW) because getSwallower() wouldn't get it after if PWINDOW gets auto grouped.
    const auto SWALLOWER  = PWINDOW->getSwallower();
    PWINDOW->m_pSwallowed = SWALLOWER;
    if (PWINDOW->m_pSwallowed)
        PWINDOW->m_pSwallowed->m_bCurrentlySwallowed = true;

    if (PWINDOW->m_bIsFloating) {
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(PWINDOW);
        PWINDOW->m_bCreatedOverFullscreen = true;

        // size and move rules
        for (auto const& r : PWINDOW->m_vMatchedRules) {
            switch (r->ruleType) {
                case CWindowRule::RULE_SIZE: {
                    try {
                        auto stringToFloatClamp = [](const std::string& VALUE, const float CURR, const float REL) {
                            if (VALUE.starts_with('<'))
                                return std::min(CURR, stringToPercentage(VALUE.substr(1, VALUE.length() - 1), REL));
                            else if (VALUE.starts_with('>'))
                                return std::max(CURR, stringToPercentage(VALUE.substr(1, VALUE.length() - 1), REL));

                            return stringToPercentage(VALUE, REL);
                        };

                        const auto  VALUE    = r->szRule.substr(r->szRule.find(' ') + 1);
                        const auto  SIZEXSTR = VALUE.substr(0, VALUE.find(' '));
                        const auto  SIZEYSTR = VALUE.substr(VALUE.find(' ') + 1);

                        const auto  MAXSIZE = PWINDOW->requestedMaxSize();

                        const float SIZEX = SIZEXSTR == "max" ? std::clamp(MAXSIZE.x, MIN_WINDOW_SIZE, PMONITOR->vecSize.x) :
                                                                stringToFloatClamp(SIZEXSTR, PWINDOW->m_vRealSize->goal().x, PMONITOR->vecSize.x);

                        const float SIZEY = SIZEYSTR == "max" ? std::clamp(MAXSIZE.y, MIN_WINDOW_SIZE, PMONITOR->vecSize.y) :
                                                                stringToFloatClamp(SIZEYSTR, PWINDOW->m_vRealSize->goal().y, PMONITOR->vecSize.y);

                        Debug::log(LOG, "Rule size, applying to {}", PWINDOW);

                        PWINDOW->clampWindowSize(Vector2D{SIZEXSTR.starts_with("<") ? 0 : SIZEX, SIZEYSTR.starts_with("<") ? 0 : SIZEY}, Vector2D{SIZEX, SIZEY});

                        PWINDOW->setHidden(false);
                    } catch (...) { Debug::log(LOG, "Rule size failed, rule: {} -> {}", r->szRule, r->szValue); }
                    break;
                }
                case CWindowRule::RULE_MOVE: {
                    try {
                        auto       value = r->szRule.substr(r->szRule.find(' ') + 1);

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
                            posX                      = PMONITOR->vecSize.x -
                                (!POSXRAW.contains('%') ? std::stoi(POSXRAW) : std::stof(POSXRAW.substr(0, POSXRAW.length() - 1)) * 0.01 * PMONITOR->vecSize.x);

                            if (subtractWindow)
                                posX -= PWINDOW->m_vRealSize->goal().x;

                            if (CURSOR)
                                Debug::log(ERR, "Cursor is not compatible with 100%-, ignoring cursor!");
                        } else if (!CURSOR) {
                            posX = !POSXSTR.contains('%') ? std::stoi(POSXSTR) : std::stof(POSXSTR.substr(0, POSXSTR.length() - 1)) * 0.01 * PMONITOR->vecSize.x;
                        } else {
                            // cursor
                            if (POSXSTR == "cursor") {
                                posX = g_pInputManager->getMouseCoordsInternal().x - PMONITOR->vecPosition.x;
                            } else {
                                posX = g_pInputManager->getMouseCoordsInternal().x - PMONITOR->vecPosition.x +
                                    (!POSXSTR.contains('%') ? std::stoi(POSXSTR) : std::stof(POSXSTR.substr(0, POSXSTR.length() - 1)) * 0.01 * PWINDOW->m_vRealSize->goal().x);
                            }
                        }

                        if (POSYSTR.starts_with("100%-")) {
                            const bool subtractWindow = POSYSTR.starts_with("100%-w-");
                            const auto POSYRAW        = (subtractWindow) ? POSYSTR.substr(7) : POSYSTR.substr(5);
                            posY                      = PMONITOR->vecSize.y -
                                (!POSYRAW.contains('%') ? std::stoi(POSYRAW) : std::stof(POSYRAW.substr(0, POSYRAW.length() - 1)) * 0.01 * PMONITOR->vecSize.y);

                            if (subtractWindow)
                                posY -= PWINDOW->m_vRealSize->goal().y;

                            if (CURSOR)
                                Debug::log(ERR, "Cursor is not compatible with 100%-, ignoring cursor!");
                        } else if (!CURSOR) {
                            posY = !POSYSTR.contains('%') ? std::stoi(POSYSTR) : std::stof(POSYSTR.substr(0, POSYSTR.length() - 1)) * 0.01 * PMONITOR->vecSize.y;
                        } else {
                            // cursor
                            if (POSYSTR == "cursor") {
                                posY = g_pInputManager->getMouseCoordsInternal().y - PMONITOR->vecPosition.y;
                            } else {
                                posY = g_pInputManager->getMouseCoordsInternal().y - PMONITOR->vecPosition.y +
                                    (!POSYSTR.contains('%') ? std::stoi(POSYSTR) : std::stof(POSYSTR.substr(0, POSYSTR.length() - 1)) * 0.01 * PWINDOW->m_vRealSize->goal().y);
                            }
                        }

                        if (ONSCREEN) {
                            int borderSize = PWINDOW->getRealBorderSize();

                            posX = std::clamp(posX, (int)(PMONITOR->vecReservedTopLeft.x + borderSize),
                                              (int)(PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x - PWINDOW->m_vRealSize->goal().x - borderSize));

                            posY = std::clamp(posY, (int)(PMONITOR->vecReservedTopLeft.y + borderSize),
                                              (int)(PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PWINDOW->m_vRealSize->goal().y - borderSize));
                        }

                        Debug::log(LOG, "Rule move, applying to {}", PWINDOW);

                        *PWINDOW->m_vRealPosition = Vector2D(posX, posY) + PMONITOR->vecPosition;

                        PWINDOW->setHidden(false);
                    } catch (...) { Debug::log(LOG, "Rule move failed, rule: {} -> {}", r->szRule, r->szValue); }
                    break;
                }
                case CWindowRule::RULE_CENTER: {
                    auto       RESERVEDOFFSET = Vector2D();
                    const auto ARGS           = CVarList(r->szRule, 2, ' ');
                    if (ARGS[1] == "1")
                        RESERVEDOFFSET = (PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight) / 2.f;

                    *PWINDOW->m_vRealPosition = PMONITOR->middle() - PWINDOW->m_vRealSize->goal() / 2.f + RESERVEDOFFSET;
                    break;
                }

                default: break;
            }
        }

        // set the pseudo size to the GOAL of our current size
        // because the windows are animated on RealSize
        PWINDOW->m_vPseudoSize = PWINDOW->m_vRealSize->goal();

        g_pCompositor->changeWindowZOrder(PWINDOW, true);
    } else {
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(PWINDOW);

        bool setPseudo = false;

        for (auto const& r : PWINDOW->m_vMatchedRules) {
            if (r->ruleType != CWindowRule::RULE_SIZE)
                continue;

            try {
                const auto  VALUE    = r->szRule.substr(r->szRule.find(' ') + 1);
                const auto  SIZEXSTR = VALUE.substr(0, VALUE.find(' '));
                const auto  SIZEYSTR = VALUE.substr(VALUE.find(' ') + 1);

                const auto  MAXSIZE = PWINDOW->requestedMaxSize();

                const float SIZEX = SIZEXSTR == "max" ? std::clamp(MAXSIZE.x, MIN_WINDOW_SIZE, PMONITOR->vecSize.x) : stringToPercentage(SIZEXSTR, PMONITOR->vecSize.x);

                const float SIZEY = SIZEYSTR == "max" ? std::clamp(MAXSIZE.y, MIN_WINDOW_SIZE, PMONITOR->vecSize.y) : stringToPercentage(SIZEYSTR, PMONITOR->vecSize.y);

                Debug::log(LOG, "Rule size (tiled), applying to {}", PWINDOW);

                setPseudo              = true;
                PWINDOW->m_vPseudoSize = Vector2D(SIZEX, SIZEY);

                PWINDOW->setHidden(false);
            } catch (...) { Debug::log(LOG, "Rule size failed, rule: {} -> {}", r->szRule, r->szValue); }
        }

        if (!setPseudo)
            PWINDOW->m_vPseudoSize = PWINDOW->m_vRealSize->goal() - Vector2D(10, 10);
    }

    const auto PFOCUSEDWINDOWPREV = g_pCompositor->m_pLastWindow.lock();

    if (PWINDOW->m_sWindowData.allowsInput.valueOrDefault()) { // if default value wasn't set to false getPriority() would throw an exception
        PWINDOW->m_sWindowData.noFocus = CWindowOverridableVar(false, PWINDOW->m_sWindowData.allowsInput.getPriority());
        PWINDOW->m_bNoInitialFocus     = false;
        PWINDOW->m_bX11ShouldntFocus   = false;
    }

    // check LS focus grab
    const auto PFORCEFOCUS  = g_pCompositor->getForceFocus();
    const auto PLSFROMFOCUS = g_pCompositor->getLayerSurfaceFromSurface(g_pCompositor->m_pLastFocus.lock());
    if (PLSFROMFOCUS && PLSFROMFOCUS->layerSurface->current.interactivity != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
        PWINDOW->m_bNoInitialFocus = true;

    if (PWINDOW->m_pWorkspace->m_bHasFullscreenWindow && !requestedInternalFSMode.has_value() && !requestedClientFSMode.has_value() && !PWINDOW->m_bIsFloating) {
        if (*PNEWTAKESOVERFS == 0)
            PWINDOW->m_bNoInitialFocus = true;
        else if (*PNEWTAKESOVERFS == 1)
            requestedInternalFSMode = PWINDOW->m_pWorkspace->m_efFullscreenMode;
        else if (*PNEWTAKESOVERFS == 2)
            g_pCompositor->setWindowFullscreenInternal(PWINDOW->m_pWorkspace->getFullscreenWindow(), FSMODE_NONE);
    }

    if (!PWINDOW->m_sWindowData.noFocus.valueOrDefault() && !PWINDOW->m_bNoInitialFocus &&
        (!PWINDOW->isX11OverrideRedirect() || (PWINDOW->m_bIsX11 && PWINDOW->m_pXWaylandSurface->wantsFocus())) && !workspaceSilent && (!PFORCEFOCUS || PFORCEFOCUS == PWINDOW) &&
        !g_pInputManager->isConstrained()) {
        g_pCompositor->focusWindow(PWINDOW);
        PWINDOW->m_fActiveInactiveAlpha->setValueAndWarp(*PACTIVEALPHA);
        PWINDOW->m_fDimPercent->setValueAndWarp(PWINDOW->m_sWindowData.noDim.valueOrDefault() ? 0.f : *PDIMSTRENGTH);
    } else {
        PWINDOW->m_fActiveInactiveAlpha->setValueAndWarp(*PINACTIVEALPHA);
        PWINDOW->m_fDimPercent->setValueAndWarp(0);
    }

    if (requestedClientFSMode.has_value() && (PWINDOW->m_eSuppressedEvents & SUPPRESS_FULLSCREEN))
        requestedClientFSMode = (eFullscreenMode)((uint8_t)requestedClientFSMode.value_or(FSMODE_NONE) & ~(uint8_t)FSMODE_FULLSCREEN);
    if (requestedClientFSMode.has_value() && (PWINDOW->m_eSuppressedEvents & SUPPRESS_MAXIMIZE))
        requestedClientFSMode = (eFullscreenMode)((uint8_t)requestedClientFSMode.value_or(FSMODE_NONE) & ~(uint8_t)FSMODE_MAXIMIZED);

    if (!PWINDOW->m_bNoInitialFocus && (requestedInternalFSMode.has_value() || requestedClientFSMode.has_value() || requestedFSState.has_value())) {
        // fix fullscreen on requested (basically do a switcheroo)
        if (PWINDOW->m_pWorkspace->m_bHasFullscreenWindow)
            g_pCompositor->setWindowFullscreenInternal(PWINDOW->m_pWorkspace->getFullscreenWindow(), FSMODE_NONE);

        PWINDOW->m_vRealPosition->warp();
        PWINDOW->m_vRealSize->warp();
        if (requestedFSState.has_value()) {
            PWINDOW->m_sWindowData.syncFullscreen = CWindowOverridableVar(false, PRIORITY_WINDOW_RULE);
            g_pCompositor->setWindowFullscreenState(PWINDOW, requestedFSState.value());
        } else if (requestedInternalFSMode.has_value() && requestedClientFSMode.has_value() && !PWINDOW->m_sWindowData.syncFullscreen.valueOrDefault())
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

    PWINDOW->m_bFirstMap = false;

    Debug::log(LOG, "Map request dispatched, monitor {}, window pos: {:5j}, window size: {:5j}", PMONITOR->szName, PWINDOW->m_vRealPosition->goal(), PWINDOW->m_vRealSize->goal());

    auto workspaceID = requestedWorkspace != "" ? requestedWorkspace : PWORKSPACE->m_szName;
    g_pEventManager->postEvent(SHyprIPCEvent{"openwindow", std::format("{:x},{},{},{}", PWINDOW, workspaceID, PWINDOW->m_szClass, PWINDOW->m_szTitle)});
    EMIT_HOOK_EVENT("openWindow", PWINDOW);

    // apply data from default decos. Borders, shadows.
    g_pDecorationPositioner->forceRecalcFor(PWINDOW);
    PWINDOW->updateWindowDecos();
    g_pLayoutManager->getCurrentLayout()->recalculateWindow(PWINDOW);

    // do animations
    g_pAnimationManager->onWindowPostCreateClose(PWINDOW, false);
    PWINDOW->m_fAlpha->setValueAndWarp(0.f);
    *PWINDOW->m_fAlpha = 1.f;

    PWINDOW->m_vRealPosition->setCallbackOnEnd(setVector2DAnimToMove);
    PWINDOW->m_vRealSize->setCallbackOnEnd(setVector2DAnimToMove);

    // recalc the values for this window
    g_pCompositor->updateWindowAnimatedDecorationValues(PWINDOW);
    // avoid this window being visible
    if (PWORKSPACE->m_bHasFullscreenWindow && !PWINDOW->isFullscreen() && !PWINDOW->m_bIsFloating)
        PWINDOW->m_fAlpha->setValueAndWarp(0.f);

    g_pCompositor->setPreferredScaleForSurface(PWINDOW->m_pWLSurface->resource(), PMONITOR->scale);
    g_pCompositor->setPreferredTransformForSurface(PWINDOW->m_pWLSurface->resource(), PMONITOR->transform);

    if (g_pSeatManager->mouse.expired() || !g_pInputManager->isConstrained())
        g_pInputManager->sendMotionEventsToFocused();

    // fix some xwayland apps that don't behave nicely
    PWINDOW->m_vReportedSize = PWINDOW->m_vPendingReportedSize;

    if (PWINDOW->m_pWorkspace)
        PWINDOW->m_pWorkspace->updateWindows();

    if (PMONITOR && PWINDOW->isX11OverrideRedirect())
        PWINDOW->m_fX11SurfaceScaledBy = PMONITOR->scale;

    // Fix some X11 popups being invisible / having incorrect size on open.
    // What the ACTUAL FUCK is going on?????? I HATE X11
    if (!PWINDOW->isX11OverrideRedirect() && PWINDOW->m_bIsX11 && PWINDOW->m_bIsFloating) {
        PWINDOW->sendWindowSize(PWINDOW->m_vRealSize->goal(), true, PWINDOW->m_vRealPosition->goal() - Vector2D{1, 1});
        PWINDOW->sendWindowSize(PWINDOW->m_vRealSize->goal(), true);
    }
}

void Events::listener_unmapWindow(void* owner, void* data) {
    PHLWINDOW PWINDOW = ((CWindow*)owner)->m_pSelf.lock();

    Debug::log(LOG, "{:c} unmapped", PWINDOW);

    static auto PEXITRETAINSFS = CConfigValue<Hyprlang::INT>("misc:exit_window_retains_fullscreen");

    const auto  CURRENTWINDOWFSSTATE = PWINDOW->isFullscreen();
    const auto  CURRENTFSMODE        = PWINDOW->m_sFullscreenState.internal;

    if (!PWINDOW->m_pWLSurface->exists() || !PWINDOW->m_bIsMapped) {
        Debug::log(WARN, "{} unmapped without being mapped??", PWINDOW);
        PWINDOW->m_bFadingOut = false;
        return;
    }

    const auto PMONITOR = PWINDOW->m_pMonitor.lock();
    if (PMONITOR) {
        PWINDOW->m_vOriginalClosedPos     = PWINDOW->m_vRealPosition->value() - PMONITOR->vecPosition;
        PWINDOW->m_vOriginalClosedSize    = PWINDOW->m_vRealSize->value();
        PWINDOW->m_eOriginalClosedExtents = PWINDOW->getFullWindowExtents();
    }

    g_pEventManager->postEvent(SHyprIPCEvent{"closewindow", std::format("{:x}", PWINDOW)});
    EMIT_HOOK_EVENT("closeWindow", PWINDOW);

    PROTO::toplevelExport->onWindowUnmap(PWINDOW);

    if (PWINDOW->isFullscreen())
        g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);

    // Allow the renderer to catch the last frame.
    g_pHyprRenderer->makeWindowSnapshot(PWINDOW);

    // swallowing
    if (valid(PWINDOW->m_pSwallowed)) {
        if (PWINDOW->m_pSwallowed->m_bCurrentlySwallowed) {
            PWINDOW->m_pSwallowed->m_bCurrentlySwallowed = false;
            PWINDOW->m_pSwallowed->setHidden(false);

            if (PWINDOW->m_sGroupData.pNextWindow.lock())
                PWINDOW->m_pSwallowed->m_bGroupSwallowed = true; // flag for the swallowed window to be created into the group where it belongs when auto_group = false.

            g_pLayoutManager->getCurrentLayout()->onWindowCreated(PWINDOW->m_pSwallowed.lock());
        }

        PWINDOW->m_pSwallowed->m_bGroupSwallowed = false;
        PWINDOW->m_pSwallowed.reset();
    }

    bool wasLastWindow = false;

    if (PWINDOW == g_pCompositor->m_pLastWindow.lock()) {
        wasLastWindow = true;
        g_pCompositor->m_pLastWindow.reset();
        g_pCompositor->m_pLastFocus.reset();

        g_pInputManager->releaseAllMouseButtons();
    }

    if (PWINDOW == g_pInputManager->currentlyDraggedWindow.lock())
        g_pKeybindManager->changeMouseBindMode(MBIND_INVALID);

    // remove the fullscreen window status from workspace if we closed it
    const auto PWORKSPACE = PWINDOW->m_pWorkspace;

    if (PWORKSPACE->m_bHasFullscreenWindow && PWINDOW->isFullscreen())
        PWORKSPACE->m_bHasFullscreenWindow = false;

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(PWINDOW);

    g_pHyprRenderer->damageWindow(PWINDOW);

    // do this after onWindowRemoved because otherwise it'll think the window is invalid
    PWINDOW->m_bIsMapped = false;

    // refocus on a new window if needed
    if (wasLastWindow) {
        static auto FOCUSONCLOSE     = CConfigValue<Hyprlang::INT>("input:focus_on_close");
        PHLWINDOW   PWINDOWCANDIDATE = nullptr;
        if (*FOCUSONCLOSE)
            PWINDOWCANDIDATE = (g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(), RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING));
        else
            PWINDOWCANDIDATE = g_pLayoutManager->getCurrentLayout()->getNextWindowCandidate(PWINDOW);

        Debug::log(LOG, "On closed window, new focused candidate is {}", PWINDOWCANDIDATE);

        if (PWINDOWCANDIDATE != g_pCompositor->m_pLastWindow.lock() && PWINDOWCANDIDATE) {
            g_pCompositor->focusWindow(PWINDOWCANDIDATE);
            if (*PEXITRETAINSFS && CURRENTWINDOWFSSTATE)
                g_pCompositor->setWindowFullscreenInternal(PWINDOWCANDIDATE, CURRENTFSMODE);
        }

        if (!PWINDOWCANDIDATE && PWINDOW->m_pWorkspace && PWINDOW->m_pWorkspace->getWindows() == 0)
            g_pInputManager->refocus();

        g_pInputManager->sendMotionEventsToFocused();

        // CWindow::onUnmap will remove this window's active status, but we can't really do it above.
        if (PWINDOW == g_pCompositor->m_pLastWindow.lock() || !g_pCompositor->m_pLastWindow.lock()) {
            g_pEventManager->postEvent(SHyprIPCEvent{"activewindow", ","});
            g_pEventManager->postEvent(SHyprIPCEvent{"activewindowv2", ""});
            EMIT_HOOK_EVENT("activeWindow", (PHLWINDOW) nullptr);
        }
    } else {
        Debug::log(LOG, "Unmapped was not focused, ignoring a refocus.");
    }

    PWINDOW->m_bFadingOut = true;

    g_pCompositor->addToFadingOutSafe(PWINDOW);

    if (!PWINDOW->m_bX11DoesntWantBorders)                                                      // don't animate out if they weren't animated in.
        *PWINDOW->m_vRealPosition = PWINDOW->m_vRealPosition->value() + Vector2D(0.01f, 0.01f); // it has to be animated, otherwise onWindowPostCreateClose will ignore it

    // anims
    g_pAnimationManager->onWindowPostCreateClose(PWINDOW, true);
    *PWINDOW->m_fAlpha = 0.f;

    // recheck idle inhibitors
    g_pInputManager->recheckIdleInhibitorStatus();

    // force report all sizes (QT sometimes has an issue with this)
    if (PWINDOW->m_pWorkspace)
        PWINDOW->m_pWorkspace->forceReportSizesToWindows();

    // update lastwindow after focus
    PWINDOW->onUnmap();
}

void Events::listener_commitWindow(void* owner, void* data) {
    PHLWINDOW PWINDOW = ((CWindow*)owner)->m_pSelf.lock();

    if (!PWINDOW->m_bIsX11 && PWINDOW->m_pXDGSurface->initialCommit) {
        Vector2D predSize = g_pLayoutManager->getCurrentLayout()->predictSizeForNewWindow(PWINDOW);

        Debug::log(LOG, "Layout predicts size {} for {}", predSize, PWINDOW);

        PWINDOW->m_pXDGSurface->toplevel->setSize(predSize);
        return;
    }

    if (!PWINDOW->m_bIsMapped || PWINDOW->isHidden())
        return;

    PWINDOW->m_vReportedSize = PWINDOW->m_vPendingReportedSize; // apply pending size. We pinged, the window ponged.

    if (!PWINDOW->m_bIsX11 && !PWINDOW->isFullscreen() && PWINDOW->m_bIsFloating) {
        const auto MINSIZE = PWINDOW->m_pXDGSurface->toplevel->layoutMinSize();
        const auto MAXSIZE = PWINDOW->m_pXDGSurface->toplevel->layoutMaxSize();

        PWINDOW->clampWindowSize(MINSIZE, MAXSIZE > Vector2D{1, 1} ? std::optional<Vector2D>{MAXSIZE} : std::nullopt);
        g_pHyprRenderer->damageWindow(PWINDOW);
    }

    if (!PWINDOW->m_pWorkspace->m_bVisible)
        return;

    const auto PMONITOR = PWINDOW->m_pMonitor.lock();

    if (PMONITOR)
        PMONITOR->debugLastPresentation(g_pSeatManager->isPointerFrameCommit ? "listener_commitWindow skip" : "listener_commitWindow");

    if (g_pSeatManager->isPointerFrameCommit) {
        g_pSeatManager->isPointerFrameSkipped = false;
        g_pSeatManager->isPointerFrameCommit  = false;
    } else
        g_pHyprRenderer->damageSurface(PWINDOW->m_pWLSurface->resource(), PWINDOW->m_vRealPosition->goal().x, PWINDOW->m_vRealPosition->goal().y,
                                       PWINDOW->m_bIsX11 ? 1.0 / PWINDOW->m_fX11SurfaceScaledBy : 1.0);

    if (g_pSeatManager->isPointerFrameSkipped) {
        g_pPointerManager->sendStoredMovement();
        g_pSeatManager->sendPointerFrame();
        g_pSeatManager->isPointerFrameCommit = true;
    }

    if (!PWINDOW->m_bIsX11) {
        PWINDOW->m_pSubsurfaceHead->recheckDamageForSubsurfaces();
        PWINDOW->m_pPopupHead->recheckTree();
    }

    // tearing: if solitary, redraw it. This still might be a single surface window
    if (PMONITOR && PMONITOR->solitaryClient.lock() == PWINDOW && PWINDOW->canBeTorn() && PMONITOR->tearingState.canTear && PWINDOW->m_pWLSurface->resource()->current.texture) {
        CRegion damageBox{PWINDOW->m_pWLSurface->resource()->accumulateCurrentBufferDamage()};

        if (!damageBox.empty()) {
            if (PMONITOR->tearingState.busy) {
                PMONITOR->tearingState.frameScheduledWhileBusy = true;
            } else {
                PMONITOR->tearingState.nextRenderTorn = true;
                g_pHyprRenderer->renderMonitor(PMONITOR);
            }
        }
    }
}

void Events::listener_destroyWindow(void* owner, void* data) {
    PHLWINDOW PWINDOW = ((CWindow*)owner)->m_pSelf.lock();

    Debug::log(LOG, "{:c} destroyed, queueing.", PWINDOW);

    if (PWINDOW == g_pCompositor->m_pLastWindow.lock()) {
        g_pCompositor->m_pLastWindow.reset();
        g_pCompositor->m_pLastFocus.reset();
    }

    PWINDOW->m_pWLSurface->unassign();

    PWINDOW->listeners = {};

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(PWINDOW);

    PWINDOW->m_bReadyToDelete = true;

    PWINDOW->m_pXDGSurface.reset();

    if (!PWINDOW->m_bFadingOut) {
        Debug::log(LOG, "Unmapped {} removed instantly", PWINDOW);
        g_pCompositor->removeWindowFromVectorSafe(PWINDOW); // most likely X11 unmanaged or sumn
    }

    PWINDOW->listeners.unmap.reset();
    PWINDOW->listeners.destroy.reset();
    PWINDOW->listeners.map.reset();
    PWINDOW->listeners.commit.reset();
}

void Events::listener_activateX11(void* owner, void* data) {
    PHLWINDOW PWINDOW = ((CWindow*)owner)->m_pSelf.lock();

    Debug::log(LOG, "X11 Activate request for window {}", PWINDOW);

    if (PWINDOW->isX11OverrideRedirect()) {

        Debug::log(LOG, "Unmanaged X11 {} requests activate", PWINDOW);

        if (g_pCompositor->m_pLastWindow.lock() && g_pCompositor->m_pLastWindow->getPID() != PWINDOW->getPID())
            return;

        if (!PWINDOW->m_pXWaylandSurface->wantsFocus())
            return;

        g_pCompositor->focusWindow(PWINDOW);
        return;
    }

    if (PWINDOW == g_pCompositor->m_pLastWindow.lock() || (PWINDOW->m_eSuppressedEvents & SUPPRESS_ACTIVATE))
        return;

    PWINDOW->activate();
}

void Events::listener_unmanagedSetGeometry(void* owner, void* data) {
    PHLWINDOW PWINDOW = ((CWindow*)owner)->m_pSelf.lock();

    if (!PWINDOW->m_bIsMapped || !PWINDOW->m_pXWaylandSurface || !PWINDOW->m_pXWaylandSurface->overrideRedirect)
        return;

    const auto POS = PWINDOW->m_vRealPosition->goal();
    const auto SIZ = PWINDOW->m_vRealSize->goal();

    if (PWINDOW->m_pXWaylandSurface->geometry.size() > Vector2D{1, 1})
        PWINDOW->setHidden(false);
    else
        PWINDOW->setHidden(true);

    if (PWINDOW->isFullscreen() || !PWINDOW->m_bIsFloating) {
        PWINDOW->sendWindowSize(PWINDOW->m_vRealSize->goal(), true);
        g_pHyprRenderer->damageWindow(PWINDOW);
        return;
    }

    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    const auto  LOGICALPOS = g_pXWaylandManager->xwaylandToWaylandCoords(PWINDOW->m_pXWaylandSurface->geometry.pos());

    if (abs(std::floor(POS.x) - LOGICALPOS.x) > 2 || abs(std::floor(POS.y) - LOGICALPOS.y) > 2 || abs(std::floor(SIZ.x) - PWINDOW->m_pXWaylandSurface->geometry.width) > 2 ||
        abs(std::floor(SIZ.y) - PWINDOW->m_pXWaylandSurface->geometry.height) > 2) {
        Debug::log(LOG, "Unmanaged window {} requests geometry update to {:j} {:j}", PWINDOW, LOGICALPOS, PWINDOW->m_pXWaylandSurface->geometry.size());

        g_pHyprRenderer->damageWindow(PWINDOW);
        PWINDOW->m_vRealPosition->setValueAndWarp(Vector2D(LOGICALPOS.x, LOGICALPOS.y));

        if (abs(std::floor(SIZ.x) - PWINDOW->m_pXWaylandSurface->geometry.w) > 2 || abs(std::floor(SIZ.y) - PWINDOW->m_pXWaylandSurface->geometry.h) > 2)
            PWINDOW->m_vRealSize->setValueAndWarp(PWINDOW->m_pXWaylandSurface->geometry.size());

        if (*PXWLFORCESCALEZERO) {
            if (const auto PMONITOR = PWINDOW->m_pMonitor.lock(); PMONITOR) {
                PWINDOW->m_vRealSize->setValueAndWarp(PWINDOW->m_vRealSize->goal() / PMONITOR->scale);
            }
        }

        PWINDOW->m_vPosition = PWINDOW->m_vRealPosition->goal();
        PWINDOW->m_vSize     = PWINDOW->m_vRealSize->goal();

        PWINDOW->m_pWorkspace = g_pCompositor->getMonitorFromVector(PWINDOW->m_vRealPosition->value() + PWINDOW->m_vRealSize->value() / 2.f)->activeWorkspace;

        g_pCompositor->changeWindowZOrder(PWINDOW, true);
        PWINDOW->updateWindowDecos();
        g_pHyprRenderer->damageWindow(PWINDOW);

        PWINDOW->m_vReportedPosition    = PWINDOW->m_vRealPosition->goal();
        PWINDOW->m_vPendingReportedSize = PWINDOW->m_vRealSize->goal();
    }
}
