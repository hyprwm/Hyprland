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
#include "../xwayland/XSurface.hpp"

// ------------------------------------------------------------ //
//  __          _______ _   _ _____   ______          _______   //
//  \ \        / /_   _| \ | |  __ \ / __ \ \        / / ____|  //
//   \ \  /\  / /  | | |  \| | |  | | |  | \ \  /\  / / (___    //
//    \ \/  \/ /   | | | . ` | |  | | |  | |\ \/  \/ / \___ \   //
//     \  /\  /   _| |_| |\  | |__| | |__| | \  /\  /  ____) |  //
//      \/  \/   |_____|_| \_|_____/ \____/   \/  \/  |_____/   //
//                                                              //
// ------------------------------------------------------------ //

void setAnimToMove(void* data) {
    auto* const            PANIMCFG = g_pConfigManager->getAnimationPropertyConfig("windowsMove");

    CBaseAnimatedVariable* animvar = (CBaseAnimatedVariable*)data;

    animvar->setConfig(PANIMCFG);

    if (animvar->getWindow() && !animvar->getWindow()->m_vRealPosition.isBeingAnimated() && !animvar->getWindow()->m_vRealSize.isBeingAnimated())
        animvar->getWindow()->m_bAnimatingIn = false;
}

void Events::listener_mapWindow(void* owner, void* data) {
    PHLWINDOW   PWINDOW = ((CWindow*)owner)->m_pSelf.lock();

    static auto PINACTIVEALPHA     = CConfigValue<Hyprlang::FLOAT>("decoration:inactive_opacity");
    static auto PACTIVEALPHA       = CConfigValue<Hyprlang::FLOAT>("decoration:active_opacity");
    static auto PDIMSTRENGTH       = CConfigValue<Hyprlang::FLOAT>("decoration:dim_strength");
    static auto PSWALLOW           = CConfigValue<Hyprlang::INT>("misc:enable_swallow");
    static auto PSWALLOWREGEX      = CConfigValue<std::string>("misc:swallow_regex");
    static auto PSWALLOWEXREGEX    = CConfigValue<std::string>("misc:swallow_exception_regex");
    static auto PNEWTAKESOVERFS    = CConfigValue<Hyprlang::INT>("misc:new_window_takes_over_fullscreen");
    static auto PINITIALWSTRACKING = CConfigValue<Hyprlang::INT>("misc:initial_workspace_tracking");

    auto        PMONITOR = g_pCompositor->m_pLastMonitor.get();
    if (!g_pCompositor->m_pLastMonitor) {
        g_pCompositor->setActiveMonitor(g_pCompositor->getMonitorFromVector({}));
        PMONITOR = g_pCompositor->m_pLastMonitor.get();
    }
    auto PWORKSPACE           = PMONITOR->activeSpecialWorkspace ? PMONITOR->activeSpecialWorkspace : PMONITOR->activeWorkspace;
    PWINDOW->m_iMonitorID     = PMONITOR->ID;
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

    PWINDOW->m_bX11ShouldntFocus = PWINDOW->m_bX11ShouldntFocus || (PWINDOW->m_bIsX11 && PWINDOW->m_iX11Type == 2 && !PWINDOW->m_pXWaylandSurface->wantsFocus());

    if (PWORKSPACE->m_bDefaultFloating)
        PWINDOW->m_bIsFloating = true;

    if (PWORKSPACE->m_bDefaultPseudo) {
        PWINDOW->m_bIsPseudotiled = true;
        CBox desiredGeometry      = {0};
        g_pXWaylandManager->getGeometryForWindow(PWINDOW, &desiredGeometry);
        PWINDOW->m_vPseudoSize = Vector2D(desiredGeometry.width, desiredGeometry.height);
    }

    // window rules
    PWINDOW->m_vMatchedRules    = g_pConfigManager->getMatchingRules(PWINDOW, false);
    bool requestsFullscreen     = PWINDOW->m_bWantsInitialFullscreen || (PWINDOW->m_bIsX11 && PWINDOW->m_pXWaylandSurface->fullscreen);
    bool requestsFakeFullscreen = false;
    bool requestsMaximize       = false;
    bool overridingNoFullscreen = false;
    bool overridingNoMaximize   = false;

    for (auto& r : PWINDOW->m_vMatchedRules) {
        if (r.szRule.starts_with("monitor")) {
            try {
                const auto MONITORSTR = removeBeginEndSpacesTabs(r.szRule.substr(r.szRule.find(' ')));

                if (MONITORSTR == "unset") {
                    PWINDOW->m_iMonitorID = PMONITOR->ID;
                } else {
                    if (isNumber(MONITORSTR)) {
                        const long int MONITOR = std::stoi(MONITORSTR);
                        if (!g_pCompositor->getMonitorFromID(MONITOR))
                            PWINDOW->m_iMonitorID = 0;
                        else
                            PWINDOW->m_iMonitorID = MONITOR;
                    } else {
                        const auto PMONITOR = g_pCompositor->getMonitorFromName(MONITORSTR);
                        if (PMONITOR)
                            PWINDOW->m_iMonitorID = PMONITOR->ID;
                        else {
                            Debug::log(ERR, "No monitor in monitor {} rule", MONITORSTR);
                            continue;
                        }
                    }
                }

                const auto PMONITORFROMID = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);

                if (PWINDOW->m_iMonitorID != PMONITOR->ID) {
                    g_pKeybindManager->m_mDispatchers["focusmonitor"](std::to_string(PWINDOW->m_iMonitorID));
                    PMONITOR = PMONITORFROMID;
                }
                PWINDOW->m_pWorkspace = PMONITOR->activeSpecialWorkspace ? PMONITOR->activeSpecialWorkspace : PMONITOR->activeWorkspace;

                Debug::log(LOG, "Rule monitor, applying to {:mw}", PWINDOW);
            } catch (std::exception& e) { Debug::log(ERR, "Rule monitor failed, rule: {} -> {} | err: {}", r.szRule, r.szValue, e.what()); }
        } else if (r.szRule.starts_with("workspace")) {
            // check if it isnt unset
            const auto WORKSPACERQ = r.szRule.substr(r.szRule.find_first_of(' ') + 1);

            if (WORKSPACERQ == "unset") {
                requestedWorkspace = "";
            } else {
                requestedWorkspace = WORKSPACERQ;
            }

            const auto JUSTWORKSPACE = WORKSPACERQ.contains(' ') ? WORKSPACERQ.substr(0, WORKSPACERQ.find_first_of(' ')) : WORKSPACERQ;

            if (JUSTWORKSPACE == PWORKSPACE->m_szName || JUSTWORKSPACE == "name:" + PWORKSPACE->m_szName)
                requestedWorkspace = "";

            Debug::log(LOG, "Rule workspace matched by {}, {} applied.", PWINDOW, r.szValue);
        } else if (r.szRule.starts_with("float")) {
            PWINDOW->m_bIsFloating = true;
        } else if (r.szRule.starts_with("tile")) {
            PWINDOW->m_bIsFloating = false;
        } else if (r.szRule.starts_with("pseudo")) {
            PWINDOW->m_bIsPseudotiled = true;
        } else if (r.szRule.starts_with("nofocus")) {
            PWINDOW->m_sAdditionalConfigData.noFocus = true;
        } else if (r.szRule.starts_with("noinitialfocus")) {
            PWINDOW->m_bNoInitialFocus = true;
        } else if (r.szRule.starts_with("suppressevent")) {
            CVarList vars(r.szRule, 0, 's', true);
            for (size_t i = 1; i < vars.size(); ++i) {
                if (vars[i] == "fullscreen")
                    PWINDOW->m_eSuppressedEvents |= SUPPRESS_FULLSCREEN;
                else if (vars[i] == "maximize")
                    PWINDOW->m_eSuppressedEvents |= SUPPRESS_MAXIMIZE;
                else if (vars[i] == "activate")
                    PWINDOW->m_eSuppressedEvents |= SUPPRESS_ACTIVATE;
                else if (vars[i] == "activatefocus")
                    PWINDOW->m_eSuppressedEvents |= SUPPRESS_ACTIVATE_FOCUSONLY;
                else
                    Debug::log(ERR, "Error while parsing suppressevent windowrule: unknown event type {}", vars[i]);
            }
        } else if (r.szRule == "fullscreen") {
            requestsFullscreen     = true;
            overridingNoFullscreen = true;
        } else if (r.szRule == "fakefullscreen") {
            requestsFakeFullscreen = true;
        } else if (r.szRule == "windowdance") {
            PWINDOW->m_sAdditionalConfigData.windowDanceCompat = true;
        } else if (r.szRule == "nomaxsize") {
            PWINDOW->m_sAdditionalConfigData.noMaxSize = true;
        } else if (r.szRule == "forceinput") {
            PWINDOW->m_sAdditionalConfigData.forceAllowsInput = true;
        } else if (r.szRule == "pin") {
            PWINDOW->m_bPinned = true;
        } else if (r.szRule == "maximize") {
            requestsMaximize     = true;
            overridingNoMaximize = true;
        } else if (r.szRule == "stayfocused") {
            PWINDOW->m_bStayFocused = true;
        } else if (r.szRule.starts_with("group")) {
            if (PWINDOW->m_eGroupRules & GROUP_OVERRIDE)
                continue;

            // `group` is a shorthand of `group set`
            if (removeBeginEndSpacesTabs(r.szRule) == "group") {
                PWINDOW->m_eGroupRules |= GROUP_SET;
                continue;
            }

            CVarList    vars(r.szRule, 0, 's');
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

        std::string requestedWorkspaceName;
        const int   REQUESTEDWORKSPACEID = getWorkspaceIDFromString(WORKSPACEARGS.join(" ", 0, workspaceSilent ? WORKSPACEARGS.size() - 1 : 0), requestedWorkspaceName);

        if (REQUESTEDWORKSPACEID != WORKSPACE_INVALID) {
            auto pWorkspace = g_pCompositor->getWorkspaceByID(REQUESTEDWORKSPACEID);

            if (!pWorkspace)
                pWorkspace = g_pCompositor->createNewWorkspace(REQUESTEDWORKSPACEID, PWINDOW->m_iMonitorID, requestedWorkspaceName);

            PWORKSPACE = pWorkspace;

            PWINDOW->m_pWorkspace = pWorkspace;
            PWINDOW->m_iMonitorID = pWorkspace->m_iMonitorID;

            if (g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID)->activeSpecialWorkspace && !pWorkspace->m_bIsSpecialWorkspace)
                workspaceSilent = true;

            if (!workspaceSilent) {
                if (pWorkspace->m_bIsSpecialWorkspace)
                    g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID)->setSpecialWorkspace(pWorkspace);
                else if (PMONITOR->activeWorkspaceID() != REQUESTEDWORKSPACEID)
                    g_pKeybindManager->m_mDispatchers["workspace"](requestedWorkspaceName);

                PMONITOR = g_pCompositor->m_pLastMonitor.get();
            }
        } else
            workspaceSilent = false;
    }

    PWINDOW->updateSpecialRenderData();

    if (PWINDOW->m_bIsFloating) {
        g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(PWINDOW);
        PWINDOW->m_bCreatedOverFullscreen = true;

        // size and move rules
        for (auto& r : PWINDOW->m_vMatchedRules) {
            if (r.szRule.starts_with("size")) {
                try {
                    const auto VALUE    = r.szRule.substr(r.szRule.find(' ') + 1);
                    const auto SIZEXSTR = VALUE.substr(0, VALUE.find(' '));
                    const auto SIZEYSTR = VALUE.substr(VALUE.find(' ') + 1);

                    const auto MAXSIZE = g_pXWaylandManager->getMaxSizeForWindow(PWINDOW);

                    const auto SIZEX = SIZEXSTR == "max" ?
                        std::clamp(MAXSIZE.x, 20.0, PMONITOR->vecSize.x) :
                        (!SIZEXSTR.contains('%') ? std::stoi(SIZEXSTR) : std::stof(SIZEXSTR.substr(0, SIZEXSTR.length() - 1)) * 0.01 * PMONITOR->vecSize.x);
                    const auto SIZEY = SIZEYSTR == "max" ?
                        std::clamp(MAXSIZE.y, 20.0, PMONITOR->vecSize.y) :
                        (!SIZEYSTR.contains('%') ? std::stoi(SIZEYSTR) : std::stof(SIZEYSTR.substr(0, SIZEYSTR.length() - 1)) * 0.01 * PMONITOR->vecSize.y);

                    Debug::log(LOG, "Rule size, applying to {}", PWINDOW);

                    PWINDOW->m_vRealSize = Vector2D(SIZEX, SIZEY);
                    g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vRealSize.goal());

                    PWINDOW->setHidden(false);
                } catch (...) { Debug::log(LOG, "Rule size failed, rule: {} -> {}", r.szRule, r.szValue); }
            } else if (r.szRule.starts_with("move")) {
                try {
                    auto       value = r.szRule.substr(r.szRule.find(' ') + 1);

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
                            PMONITOR->vecSize.x - (!POSXRAW.contains('%') ? std::stoi(POSXRAW) : std::stof(POSXRAW.substr(0, POSXRAW.length() - 1)) * 0.01 * PMONITOR->vecSize.x);

                        if (subtractWindow)
                            posX -= PWINDOW->m_vRealSize.goal().x;

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
                                (!POSXSTR.contains('%') ? std::stoi(POSXSTR) : std::stof(POSXSTR.substr(0, POSXSTR.length() - 1)) * 0.01 * PWINDOW->m_vRealSize.goal().x);
                        }
                    }

                    if (POSYSTR.starts_with("100%-")) {
                        const bool subtractWindow = POSYSTR.starts_with("100%-w-");
                        const auto POSYRAW        = (subtractWindow) ? POSYSTR.substr(7) : POSYSTR.substr(5);
                        posY =
                            PMONITOR->vecSize.y - (!POSYRAW.contains('%') ? std::stoi(POSYRAW) : std::stof(POSYRAW.substr(0, POSYRAW.length() - 1)) * 0.01 * PMONITOR->vecSize.y);

                        if (subtractWindow)
                            posY -= PWINDOW->m_vRealSize.goal().y;

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
                                (!POSYSTR.contains('%') ? std::stoi(POSYSTR) : std::stof(POSYSTR.substr(0, POSYSTR.length() - 1)) * 0.01 * PWINDOW->m_vRealSize.goal().y);
                        }
                    }

                    if (ONSCREEN) {
                        int borderSize = PWINDOW->getRealBorderSize();

                        posX = std::clamp(posX, (int)(PMONITOR->vecReservedTopLeft.x + borderSize),
                                          (int)(PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x - PWINDOW->m_vRealSize.goal().x - borderSize));

                        posY = std::clamp(posY, (int)(PMONITOR->vecReservedTopLeft.y + borderSize),
                                          (int)(PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PWINDOW->m_vRealSize.goal().y - borderSize));
                    }

                    Debug::log(LOG, "Rule move, applying to {}", PWINDOW);

                    PWINDOW->m_vRealPosition = Vector2D(posX, posY) + PMONITOR->vecPosition;

                    PWINDOW->setHidden(false);
                } catch (...) { Debug::log(LOG, "Rule move failed, rule: {} -> {}", r.szRule, r.szValue); }
            } else if (r.szRule.starts_with("center")) {
                auto       RESERVEDOFFSET = Vector2D();
                const auto ARGS           = CVarList(r.szRule, 2, ' ');
                if (ARGS[1] == "1")
                    RESERVEDOFFSET = (PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight) / 2.f;

                PWINDOW->m_vRealPosition = PMONITOR->middle() - PWINDOW->m_vRealSize.goal() / 2.f + RESERVEDOFFSET;
            }
        }

        // set the pseudo size to the GOAL of our current size
        // because the windows are animated on RealSize
        PWINDOW->m_vPseudoSize = PWINDOW->m_vRealSize.goal();

        g_pCompositor->changeWindowZOrder(PWINDOW, true);
    } else {
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(PWINDOW);

        // Set the pseudo size here too so that it doesnt end up being 0x0
        PWINDOW->m_vPseudoSize = PWINDOW->m_vRealSize.goal() - Vector2D(10, 10);
    }

    const auto PFOCUSEDWINDOWPREV = g_pCompositor->m_pLastWindow.lock();

    if (PWINDOW->m_sAdditionalConfigData.forceAllowsInput) {
        PWINDOW->m_sAdditionalConfigData.noFocus = false;
        PWINDOW->m_bNoInitialFocus               = false;
        PWINDOW->m_bX11ShouldntFocus             = false;
    }

    // check LS focus grab
    const auto PFORCEFOCUS  = g_pCompositor->getForceFocus();
    const auto PLSFROMFOCUS = g_pCompositor->getLayerSurfaceFromSurface(g_pCompositor->m_pLastFocus.lock());
    if (PLSFROMFOCUS && PLSFROMFOCUS->layerSurface->current.interactivity != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
        PWINDOW->m_bNoInitialFocus = true;
    if (PWINDOW->m_pWorkspace->m_bHasFullscreenWindow && !requestsFullscreen && !PWINDOW->m_bIsFloating) {
        if (*PNEWTAKESOVERFS == 0)
            PWINDOW->m_bNoInitialFocus = true;
        else if (*PNEWTAKESOVERFS == 2)
            g_pCompositor->setWindowFullscreen(g_pCompositor->getFullscreenWindowOnWorkspace(PWINDOW->m_pWorkspace->m_iID), false, FULLSCREEN_INVALID);
        else if (PWINDOW->m_pWorkspace->m_efFullscreenMode == FULLSCREEN_MAXIMIZED)
            requestsMaximize = true;
        else
            requestsFullscreen = true;
    }

    if (!PWINDOW->m_sAdditionalConfigData.noFocus && !PWINDOW->m_bNoInitialFocus &&
        (PWINDOW->m_iX11Type != 2 || (PWINDOW->m_bIsX11 && PWINDOW->m_pXWaylandSurface->wantsFocus())) && !workspaceSilent && (!PFORCEFOCUS || PFORCEFOCUS == PWINDOW) &&
        !g_pInputManager->isConstrained()) {
        g_pCompositor->focusWindow(PWINDOW);
        PWINDOW->m_fActiveInactiveAlpha.setValueAndWarp(*PACTIVEALPHA);
        PWINDOW->m_fDimPercent.setValueAndWarp(PWINDOW->m_sAdditionalConfigData.forceNoDim ? 0.f : *PDIMSTRENGTH);
    } else {
        PWINDOW->m_fActiveInactiveAlpha.setValueAndWarp(*PINACTIVEALPHA);
        PWINDOW->m_fDimPercent.setValueAndWarp(0);
    }

    if ((requestsFullscreen && (!(PWINDOW->m_eSuppressedEvents & SUPPRESS_FULLSCREEN) || overridingNoFullscreen)) ||
        (requestsMaximize && (!(PWINDOW->m_eSuppressedEvents & SUPPRESS_MAXIMIZE) || overridingNoMaximize)) || requestsFakeFullscreen) {
        // fix fullscreen on requested (basically do a switcheroo)
        if (PWINDOW->m_pWorkspace->m_bHasFullscreenWindow) {
            const auto PFULLWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(PWINDOW->m_pWorkspace->m_iID);
            g_pCompositor->setWindowFullscreen(PFULLWINDOW, false, FULLSCREEN_FULL);
        }

        if (requestsFakeFullscreen && !PWINDOW->m_bFakeFullscreenState) {
            PWINDOW->m_bFakeFullscreenState = !PWINDOW->m_bFakeFullscreenState;
            g_pXWaylandManager->setWindowFullscreen(PWINDOW, true);
        } else {
            overridingNoFullscreen = false;
            overridingNoMaximize   = false;
            PWINDOW->m_vRealPosition.warp();
            PWINDOW->m_vRealSize.warp();
            g_pCompositor->setWindowFullscreen(PWINDOW, true, requestsFullscreen ? FULLSCREEN_FULL : FULLSCREEN_MAXIMIZED);
        }
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

    // verify swallowing
    if (*PSWALLOW && std::string{*PSWALLOWREGEX} != STRVAL_EMPTY) {
        // don't swallow ourselves
        std::regex rgx(*PSWALLOWREGEX);
        if (!std::regex_match(PWINDOW->m_szClass, rgx)) {
            // check parent
            int ppid = getPPIDof(PWINDOW->getPID());

            int curppid = 0;

            for (int i = 0; i < 5; ++i) {
                curppid = getPPIDof(ppid);

                if (curppid < 10) {
                    break;
                }

                ppid = curppid;
            }

            if (ppid) {
                // get window by pid
                std::vector<PHLWINDOW> found;
                PHLWINDOW              finalFound;
                for (auto& w : g_pCompositor->m_vWindows) {
                    if (!w->m_bIsMapped || w->isHidden())
                        continue;

                    if (w->getPID() == ppid) {
                        found.push_back(w);
                    }
                }

                if (found.size() > 1) {
                    for (auto& w : found) {
                        // try get the focus, otherwise we'll ignore to avoid swallowing incorrect windows
                        if (w == PFOCUSEDWINDOWPREV) {
                            finalFound = w;
                            break;
                        }
                    }
                } else if (found.size() == 1) {
                    finalFound = found[0];
                }

                if (finalFound) {
                    bool valid = std::regex_match(PWINDOW->m_szClass, rgx);

                    if (std::string{*PSWALLOWEXREGEX} != STRVAL_EMPTY) {
                        std::regex exc(*PSWALLOWEXREGEX);

                        valid = valid && !std::regex_match(PWINDOW->m_szTitle, exc);
                    }

                    // check if it's the window we want & not exempt from getting swallowed
                    if (valid) {
                        // swallow
                        PWINDOW->m_pSwallowed = finalFound;

                        g_pLayoutManager->getCurrentLayout()->onWindowRemoved(finalFound);

                        finalFound->setHidden(true);

                        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PWINDOW->m_iMonitorID);
                    }
                }
            }
        }
    }

    PWINDOW->m_bFirstMap = false;

    Debug::log(LOG, "Map request dispatched, monitor {}, window pos: {:5j}, window size: {:5j}", PMONITOR->szName, PWINDOW->m_vRealPosition.goal(), PWINDOW->m_vRealSize.goal());

    auto workspaceID = requestedWorkspace != "" ? requestedWorkspace : PWORKSPACE->m_szName;
    g_pEventManager->postEvent(SHyprIPCEvent{"openwindow", std::format("{:x},{},{},{}", PWINDOW, workspaceID, PWINDOW->m_szClass, PWINDOW->m_szTitle)});
    EMIT_HOOK_EVENT("openWindow", PWINDOW);

    // apply data from default decos. Borders, shadows.
    g_pDecorationPositioner->forceRecalcFor(PWINDOW);
    PWINDOW->updateWindowDecos();
    g_pLayoutManager->getCurrentLayout()->recalculateWindow(PWINDOW);

    // do animations
    g_pAnimationManager->onWindowPostCreateClose(PWINDOW, false);
    PWINDOW->m_fAlpha.setValueAndWarp(0.f);
    PWINDOW->m_fAlpha = 1.f;

    PWINDOW->m_vRealPosition.setCallbackOnEnd(setAnimToMove);
    PWINDOW->m_vRealSize.setCallbackOnEnd(setAnimToMove);

    // recalc the values for this window
    g_pCompositor->updateWindowAnimatedDecorationValues(PWINDOW);
    // avoid this window being visible
    if (PWORKSPACE->m_bHasFullscreenWindow && !PWINDOW->m_bIsFullscreen && !PWINDOW->m_bIsFloating)
        PWINDOW->m_fAlpha.setValueAndWarp(0.f);

    g_pCompositor->setPreferredScaleForSurface(PWINDOW->m_pWLSurface->resource(), PMONITOR->scale);
    g_pCompositor->setPreferredTransformForSurface(PWINDOW->m_pWLSurface->resource(), PMONITOR->transform);

    if (g_pSeatManager->mouse.expired() || !g_pInputManager->isConstrained())
        g_pInputManager->sendMotionEventsToFocused();

    // fix some xwayland apps that don't behave nicely
    PWINDOW->m_vReportedSize = PWINDOW->m_vPendingReportedSize;

    g_pCompositor->updateWorkspaceWindows(PWINDOW->workspaceID());

    if (PMONITOR && PWINDOW->m_iX11Type == 2)
        PWINDOW->m_fX11SurfaceScaledBy = PMONITOR->scale;
}

void Events::listener_unmapWindow(void* owner, void* data) {
    PHLWINDOW PWINDOW = ((CWindow*)owner)->m_pSelf.lock();

    Debug::log(LOG, "{:c} unmapped", PWINDOW);

    if (!PWINDOW->m_pWLSurface->exists() || !PWINDOW->m_bIsMapped) {
        Debug::log(WARN, "{} unmapped without being mapped??", PWINDOW);
        PWINDOW->m_bFadingOut = false;
        return;
    }

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);
    if (PMONITOR) {
        PWINDOW->m_vOriginalClosedPos     = PWINDOW->m_vRealPosition.value() - PMONITOR->vecPosition;
        PWINDOW->m_vOriginalClosedSize    = PWINDOW->m_vRealSize.value();
        PWINDOW->m_eOriginalClosedExtents = PWINDOW->getFullWindowExtents();
    }

    g_pEventManager->postEvent(SHyprIPCEvent{"closewindow", std::format("{:x}", PWINDOW)});
    EMIT_HOOK_EVENT("closeWindow", PWINDOW);

    g_pProtocolManager->m_pToplevelExportProtocolManager->onWindowUnmap(PWINDOW);

    if (PWINDOW->m_bIsFullscreen)
        g_pCompositor->setWindowFullscreen(PWINDOW, false, FULLSCREEN_FULL);

    // Allow the renderer to catch the last frame.
    g_pHyprOpenGL->makeWindowSnapshot(PWINDOW);

    // swallowing
    if (valid(PWINDOW->m_pSwallowed)) {
        PWINDOW->m_pSwallowed->setHidden(false);
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(PWINDOW->m_pSwallowed.lock());
        PWINDOW->m_pSwallowed.reset();
    }

    bool wasLastWindow = false;

    if (PWINDOW == g_pCompositor->m_pLastWindow.lock()) {
        wasLastWindow = true;
        g_pCompositor->m_pLastWindow.reset();
        g_pCompositor->m_pLastFocus.reset();

        g_pInputManager->releaseAllMouseButtons();
    }

    // remove the fullscreen window status from workspace if we closed it
    const auto PWORKSPACE = PWINDOW->m_pWorkspace;

    if (PWORKSPACE->m_bHasFullscreenWindow && PWINDOW->m_bIsFullscreen)
        PWORKSPACE->m_bHasFullscreenWindow = false;

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(PWINDOW);

    // do this after onWindowRemoved because otherwise it'll think the window is invalid
    PWINDOW->m_bIsMapped = false;

    // refocus on a new window if needed
    if (wasLastWindow) {
        const auto PWINDOWCANDIDATE = g_pLayoutManager->getCurrentLayout()->getNextWindowCandidate(PWINDOW);

        Debug::log(LOG, "On closed window, new focused candidate is {}", PWINDOWCANDIDATE);

        if (PWINDOWCANDIDATE != g_pCompositor->m_pLastWindow.lock() && PWINDOWCANDIDATE)
            g_pCompositor->focusWindow(PWINDOWCANDIDATE);

        if (!PWINDOWCANDIDATE && g_pCompositor->getWindowsOnWorkspace(PWINDOW->workspaceID()) == 0)
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

    g_pHyprRenderer->damageMonitor(g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID));

    if (!PWINDOW->m_bX11DoesntWantBorders)                                                    // don't animate out if they weren't animated in.
        PWINDOW->m_vRealPosition = PWINDOW->m_vRealPosition.value() + Vector2D(0.01f, 0.01f); // it has to be animated, otherwise onWindowPostCreateClose will ignore it

    // anims
    g_pAnimationManager->onWindowPostCreateClose(PWINDOW, true);
    PWINDOW->m_fAlpha = 0.f;

    // recheck idle inhibitors
    g_pInputManager->recheckIdleInhibitorStatus();

    // force report all sizes (QT sometimes has an issue with this)
    g_pCompositor->forceReportSizesToWindowsOnWorkspace(PWINDOW->workspaceID());

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

    if (PWINDOW->m_bIsX11)
        PWINDOW->m_vReportedSize = PWINDOW->m_vPendingReportedSize; // apply pending size. We pinged, the window ponged.
    else if (PWINDOW->m_pPendingSizeAck.has_value()) {
        PWINDOW->m_vReportedSize = PWINDOW->m_pPendingSizeAck->second;
        PWINDOW->m_pPendingSizeAck.reset();
    }

    if (!PWINDOW->m_bIsX11 && !PWINDOW->m_bIsFullscreen && PWINDOW->m_bIsFloating) {
        const auto MINSIZE = PWINDOW->m_pXDGSurface->toplevel->current.minSize;
        const auto MAXSIZE = PWINDOW->m_pXDGSurface->toplevel->current.maxSize;

        if (MAXSIZE > Vector2D{1, 1}) {
            const auto REALSIZE = PWINDOW->m_vRealSize.goal();
            Vector2D   newSize  = REALSIZE;

            if (MAXSIZE.x < newSize.x)
                newSize.x = MAXSIZE.x;
            if (MAXSIZE.y < newSize.y)
                newSize.y = MAXSIZE.y;
            if (MINSIZE.x > newSize.x)
                newSize.x = MINSIZE.x;
            if (MINSIZE.y > newSize.y)
                newSize.y = MINSIZE.y;

            const Vector2D DELTA = REALSIZE - newSize;

            PWINDOW->m_vRealPosition = PWINDOW->m_vRealPosition.goal() + DELTA / 2.0;
            PWINDOW->m_vRealSize     = newSize;
            g_pXWaylandManager->setWindowSize(PWINDOW, newSize);
            g_pHyprRenderer->damageWindow(PWINDOW);
        }
    }

    if (!PWINDOW->m_pWorkspace->m_bVisible)
        return;

    g_pHyprRenderer->damageSurface(PWINDOW->m_pWLSurface->resource(), PWINDOW->m_vRealPosition.goal().x, PWINDOW->m_vRealPosition.goal().y,
                                   PWINDOW->m_bIsX11 ? 1.0 / PWINDOW->m_fX11SurfaceScaledBy : 1.0);

    if (!PWINDOW->m_bIsX11) {
        PWINDOW->m_pSubsurfaceHead->recheckDamageForSubsurfaces();
        PWINDOW->m_pPopupHead->recheckTree();
    }

    // tearing: if solitary, redraw it. This still might be a single surface window
    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);
    if (PMONITOR && PMONITOR->solitaryClient.lock() == PWINDOW && PWINDOW->canBeTorn() && PMONITOR->tearingState.canTear && PWINDOW->m_pWLSurface->resource()->current.buffer) {
        CRegion damageBox{PWINDOW->m_pWLSurface->resource()->current.bufferDamage};

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
}

void Events::listener_setTitleWindow(void* owner, void* data) {
    PHLWINDOW PWINDOW = ((CWindow*)owner)->m_pSelf.lock();

    if (!validMapped(PWINDOW))
        return;

    PWINDOW->onUpdateMeta();
}

void Events::listener_activateX11(void* owner, void* data) {
    PHLWINDOW PWINDOW = ((CWindow*)owner)->m_pSelf.lock();

    Debug::log(LOG, "X11 Activate request for window {}", PWINDOW);

    if (PWINDOW->m_iX11Type == 2) {

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

    const auto POS = PWINDOW->m_vRealPosition.goal();
    const auto SIZ = PWINDOW->m_vRealSize.goal();

    if (PWINDOW->m_pXWaylandSurface->geometry.size() > Vector2D{1, 1})
        PWINDOW->setHidden(false);
    else
        PWINDOW->setHidden(true);

    if (PWINDOW->m_bIsFullscreen || !PWINDOW->m_bIsFloating) {
        g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vRealSize.goal(), true);
        g_pHyprRenderer->damageWindow(PWINDOW);
        return;
    }

    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    const auto  LOGICALPOS = g_pXWaylandManager->xwaylandToWaylandCoords(PWINDOW->m_pXWaylandSurface->geometry.pos());

    if (abs(std::floor(POS.x) - LOGICALPOS.x) > 2 || abs(std::floor(POS.y) - LOGICALPOS.y) > 2 || abs(std::floor(SIZ.x) - PWINDOW->m_pXWaylandSurface->geometry.width) > 2 ||
        abs(std::floor(SIZ.y) - PWINDOW->m_pXWaylandSurface->geometry.height) > 2) {
        Debug::log(LOG, "Unmanaged window {} requests geometry update to {:j} {:j}", PWINDOW, LOGICALPOS, PWINDOW->m_pXWaylandSurface->geometry.size());

        g_pHyprRenderer->damageWindow(PWINDOW);
        PWINDOW->m_vRealPosition.setValueAndWarp(Vector2D(LOGICALPOS.x, LOGICALPOS.y));

        if (abs(std::floor(SIZ.x) - PWINDOW->m_pXWaylandSurface->geometry.w) > 2 || abs(std::floor(SIZ.y) - PWINDOW->m_pXWaylandSurface->geometry.h) > 2)
            PWINDOW->m_vRealSize.setValueAndWarp(PWINDOW->m_pXWaylandSurface->geometry.size());

        if (*PXWLFORCESCALEZERO) {
            if (const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID); PMONITOR) {
                PWINDOW->m_vRealSize.setValueAndWarp(PWINDOW->m_vRealSize.goal() / PMONITOR->scale);
            }
        }

        PWINDOW->m_vPosition = PWINDOW->m_vRealPosition.goal();
        PWINDOW->m_vSize     = PWINDOW->m_vRealSize.goal();

        PWINDOW->m_pWorkspace = g_pCompositor->getMonitorFromVector(PWINDOW->m_vRealPosition.value() + PWINDOW->m_vRealSize.value() / 2.f)->activeWorkspace;

        g_pCompositor->changeWindowZOrder(PWINDOW, true);
        PWINDOW->updateWindowDecos();
        g_pHyprRenderer->damageWindow(PWINDOW);

        PWINDOW->m_vReportedPosition    = PWINDOW->m_vRealPosition.goal();
        PWINDOW->m_vPendingReportedSize = PWINDOW->m_vRealSize.goal();
    }
}
