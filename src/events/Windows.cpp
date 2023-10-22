#include "Events.hpp"

#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/input/InputManager.hpp"
#include "../render/Renderer.hpp"

// ------------------------------------------------------------ //
//  __          _______ _   _ _____   ______          _______   //
//  \ \        / /_   _| \ | |  __ \ / __ \ \        / / ____|  //
//   \ \  /\  / /  | | |  \| | |  | | |  | \ \  /\  / / (___    //
//    \ \/  \/ /   | | | . ` | |  | | |  | |\ \/  \/ / \___ \   //
//     \  /\  /   _| |_| |\  | |__| | |__| | \  /\  /  ____) |  //
//      \/  \/   |_____|_| \_|_____/ \____/   \/  \/  |_____/   //
//                                                              //
// ------------------------------------------------------------ //

void addViewCoords(void* pWindow, int* x, int* y) {
    const auto PWINDOW = (CWindow*)pWindow;
    *x += PWINDOW->m_vRealPosition.goalv().x;
    *y += PWINDOW->m_vRealPosition.goalv().y;

    if (!PWINDOW->m_bIsX11 && PWINDOW->m_bIsMapped) {
        wlr_box geom;
        wlr_xdg_surface_get_geometry(PWINDOW->m_uSurface.xdg, &geom);

        *x -= geom.x;
        *y -= geom.y;
    }
}

void setAnimToMove(void* data) {
    auto* const        PANIMCFG = g_pConfigManager->getAnimationPropertyConfig("windowsMove");

    CAnimatedVariable* animvar = (CAnimatedVariable*)data;

    animvar->setConfig(PANIMCFG);
}

void Events::listener_mapWindow(void* owner, void* data) {
    CWindow*           PWINDOW = (CWindow*)owner;

    static auto* const PINACTIVEALPHA  = &g_pConfigManager->getConfigValuePtr("decoration:inactive_opacity")->floatValue;
    static auto* const PACTIVEALPHA    = &g_pConfigManager->getConfigValuePtr("decoration:active_opacity")->floatValue;
    static auto* const PDIMSTRENGTH    = &g_pConfigManager->getConfigValuePtr("decoration:dim_strength")->floatValue;
    static auto* const PSWALLOW        = &g_pConfigManager->getConfigValuePtr("misc:enable_swallow")->intValue;
    static auto* const PSWALLOWREGEX   = &g_pConfigManager->getConfigValuePtr("misc:swallow_regex")->strValue;
    static auto* const PSWALLOWEXREGEX = &g_pConfigManager->getConfigValuePtr("misc:swallow_exception_regex")->strValue;
    static auto* const PNEWTAKESOVERFS = &g_pConfigManager->getConfigValuePtr("misc:new_window_takes_over_fullscreen")->intValue;

    auto               PMONITOR = g_pCompositor->m_pLastMonitor;
    const auto         PWORKSPACE =
        PMONITOR->specialWorkspaceID ? g_pCompositor->getWorkspaceByID(PMONITOR->specialWorkspaceID) : g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);
    PWINDOW->m_iMonitorID     = PMONITOR->ID;
    PWINDOW->m_bMappedX11     = true;
    PWINDOW->m_iWorkspaceID   = PMONITOR->specialWorkspaceID ? PMONITOR->specialWorkspaceID : PMONITOR->activeWorkspace;
    PWINDOW->m_bIsMapped      = true;
    PWINDOW->m_bReadyToDelete = false;
    PWINDOW->m_bFadingOut     = false;
    PWINDOW->m_szTitle        = g_pXWaylandManager->getTitle(PWINDOW);
    PWINDOW->m_iX11Type       = PWINDOW->m_bIsX11 ? (PWINDOW->m_uSurface.xwayland->override_redirect ? 2 : 1) : 1;
    PWINDOW->m_bFirstMap      = true;

    if (g_pInputManager->m_bLastFocusOnLS) // waybar fix
        g_pInputManager->releaseAllMouseButtons();

    // Set all windows tiled regardless of anything
    g_pXWaylandManager->setWindowStyleTiled(PWINDOW, WLR_EDGE_LEFT | WLR_EDGE_RIGHT | WLR_EDGE_TOP | WLR_EDGE_BOTTOM);

    // Foreign Toplevel
    PWINDOW->createToplevelHandle();

    // checks if the window wants borders and sets the appropriate flag
    g_pXWaylandManager->checkBorders(PWINDOW);

    // registers the animated vars and stuff
    PWINDOW->onMap();

    const auto PWINDOWSURFACE = PWINDOW->m_pWLSurface.wlr();

    if (!PWINDOWSURFACE) {
        g_pCompositor->removeWindowFromVectorSafe(PWINDOW);
        return;
    }

    if (g_pXWaylandManager->shouldBeFloated(PWINDOW)) {
        PWINDOW->m_bIsFloating    = true;
        PWINDOW->m_bRequestsFloat = true;
    }

    PWINDOW->m_bX11ShouldntFocus =
        PWINDOW->m_bX11ShouldntFocus || (PWINDOW->m_bIsX11 && PWINDOW->m_iX11Type == 2 && !wlr_xwayland_or_surface_wants_focus(PWINDOW->m_uSurface.xwayland));

    if (PWORKSPACE->m_bDefaultFloating)
        PWINDOW->m_bIsFloating = true;

    if (PWORKSPACE->m_bDefaultPseudo) {
        PWINDOW->m_bIsPseudotiled = true;
        wlr_box desiredGeometry   = {0};
        g_pXWaylandManager->getGeometryForWindow(PWINDOW, &desiredGeometry);
        PWINDOW->m_vPseudoSize = Vector2D(desiredGeometry.width, desiredGeometry.height);
    }

    // window rules
    const auto  WINDOWRULES        = g_pConfigManager->getMatchingRules(PWINDOW);
    std::string requestedWorkspace = "";
    bool        workspaceSilent    = false;
    bool        requestsFullscreen = PWINDOW->m_bWantsInitialFullscreen ||
        (!PWINDOW->m_bIsX11 && PWINDOW->m_uSurface.xdg->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL && PWINDOW->m_uSurface.xdg->toplevel->requested.fullscreen) ||
        (PWINDOW->m_bIsX11 && PWINDOW->m_uSurface.xwayland->fullscreen);
    bool requestsFakeFullscreen = false;
    bool requestsMaximize       = false;
    bool overridingNoFullscreen = false;
    bool overridingNoMaximize   = false;

    PWINDOW->m_szInitialTitle = g_pXWaylandManager->getTitle(PWINDOW);
    PWINDOW->m_szInitialClass = g_pXWaylandManager->getAppIDClass(PWINDOW);

    for (auto& r : WINDOWRULES) {
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
                PWINDOW->m_iWorkspaceID = PMONITOR->specialWorkspaceID ? PMONITOR->specialWorkspaceID : PMONITOR->activeWorkspace;

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
            PWINDOW->m_bNoFocus = true;
        } else if (r.szRule.starts_with("noinitialfocus")) {
            PWINDOW->m_bNoInitialFocus = true;
        } else if (r.szRule.starts_with("nofullscreenrequest")) {
            PWINDOW->m_bNoFullscreenRequest = true;
        } else if (r.szRule.starts_with("nomaximizerequest")) {
            PWINDOW->m_bNoMaximizeRequest = true;
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
        } else if (r.szRule.starts_with("idleinhibit")) {
            auto IDLERULE = r.szRule.substr(r.szRule.find_first_of(' ') + 1);

            if (IDLERULE == "none") {
                PWINDOW->m_eIdleInhibitMode = IDLEINHIBIT_NONE;
            } else if (IDLERULE == "always") {
                PWINDOW->m_eIdleInhibitMode = IDLEINHIBIT_ALWAYS;
            } else if (IDLERULE == "focus") {
                PWINDOW->m_eIdleInhibitMode = IDLEINHIBIT_FOCUS;
            } else if (IDLERULE == "fullscreen") {
                PWINDOW->m_eIdleInhibitMode = IDLEINHIBIT_FULLSCREEN;
            } else {
                Debug::log(ERR, "Rule idleinhibit: unknown mode {}", IDLERULE);
            }
        }
        PWINDOW->applyDynamicRule(r);
    }

    PWINDOW->updateSpecialRenderData();

    // disallow tiled pinned
    if (PWINDOW->m_bPinned && !PWINDOW->m_bIsFloating)
        PWINDOW->m_bPinned = false;

    const CVarList WORKSPACEARGS = CVarList(requestedWorkspace, 0, ' ');

    if (!WORKSPACEARGS[0].empty()) {
        if (WORKSPACEARGS[WORKSPACEARGS.size() - 1].starts_with("silent"))
            workspaceSilent = true;

        std::string requestedWorkspaceName;
        const int   REQUESTEDWORKSPACEID = getWorkspaceIDFromString(WORKSPACEARGS.join(" ", 0, workspaceSilent ? WORKSPACEARGS.size() - 1 : 0), requestedWorkspaceName);

        if (REQUESTEDWORKSPACEID != INT_MAX) {
            auto pWorkspace = g_pCompositor->getWorkspaceByID(REQUESTEDWORKSPACEID);

            if (!pWorkspace)
                pWorkspace = g_pCompositor->createNewWorkspace(REQUESTEDWORKSPACEID, PWINDOW->m_iMonitorID, requestedWorkspaceName);

            PWINDOW->m_iWorkspaceID = pWorkspace->m_iID;
            PWINDOW->m_iMonitorID   = pWorkspace->m_iMonitorID;

            if (g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID)->specialWorkspaceID && !pWorkspace->m_bIsSpecialWorkspace)
                workspaceSilent = true;

            if (!workspaceSilent) {
                if (pWorkspace->m_bIsSpecialWorkspace)
                    g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID)->setSpecialWorkspace(pWorkspace);
                else
                    g_pKeybindManager->m_mDispatchers["workspace"](requestedWorkspaceName);

                PMONITOR = g_pCompositor->m_pLastMonitor;
            }
        } else
            workspaceSilent = false;
    }

    if (PWINDOW->m_bIsFloating) {
        g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(PWINDOW);
        PWINDOW->m_bCreatedOverFullscreen = true;

        // size and move rules
        for (auto& r : WINDOWRULES) {
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
                    g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vRealSize.goalv());

                    PWINDOW->setHidden(false);
                } catch (...) { Debug::log(LOG, "Rule size failed, rule: {} -> {}", r.szRule, r.szValue); }
            } else if (r.szRule.starts_with("minsize")) {
                try {
                    const auto VALUE    = r.szRule.substr(r.szRule.find(' ') + 1);
                    const auto SIZEXSTR = VALUE.substr(0, VALUE.find(' '));
                    const auto SIZEYSTR = VALUE.substr(VALUE.find(' ') + 1);

                    const auto SIZE =
                        Vector2D(std::max((double)std::stoll(SIZEXSTR), PWINDOW->m_vRealSize.goalv().x), std::max((double)std::stoll(SIZEYSTR), PWINDOW->m_vRealSize.goalv().y));

                    PWINDOW->m_vRealSize = SIZE;
                    g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vRealSize.goalv());

                    PWINDOW->setHidden(false);
                } catch (...) { Debug::log(LOG, "Rule minsize failed, rule: {} -> {}", r.szRule, r.szValue); }
            } else if (r.szRule.starts_with("maxsize")) {
                try {
                    const auto VALUE    = r.szRule.substr(r.szRule.find(' ') + 1);
                    const auto SIZEXSTR = VALUE.substr(0, VALUE.find(' '));
                    const auto SIZEYSTR = VALUE.substr(VALUE.find(' ') + 1);

                    const auto SIZE =
                        Vector2D(std::min((double)std::stoll(SIZEXSTR), PWINDOW->m_vRealSize.goalv().x), std::min((double)std::stoll(SIZEYSTR), PWINDOW->m_vRealSize.goalv().y));

                    PWINDOW->m_vRealSize = SIZE;
                    g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vRealSize.goalv());

                    PWINDOW->setHidden(false);
                } catch (...) { Debug::log(LOG, "Rule maxsize failed, rule: {} -> {}", r.szRule, r.szValue); }
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
                        const auto POSXRAW = POSXSTR.substr(5);
                        posX =
                            PMONITOR->vecSize.x - (!POSXRAW.contains('%') ? std::stoi(POSXRAW) : std::stof(POSXRAW.substr(0, POSXRAW.length() - 1)) * 0.01 * PMONITOR->vecSize.x);

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
                                (!POSXSTR.contains('%') ? std::stoi(POSXSTR) : std::stof(POSXSTR.substr(0, POSXSTR.length() - 1)) * 0.01 * PWINDOW->m_vRealSize.goalv().x);
                        }
                    }

                    if (POSYSTR.starts_with("100%-")) {
                        const auto POSYRAW = POSYSTR.substr(5);
                        posY =
                            PMONITOR->vecSize.y - (!POSYRAW.contains('%') ? std::stoi(POSYRAW) : std::stof(POSYRAW.substr(0, POSYRAW.length() - 1)) * 0.01 * PMONITOR->vecSize.y);

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
                                (!POSYSTR.contains('%') ? std::stoi(POSYSTR) : std::stof(POSYSTR.substr(0, POSYSTR.length() - 1)) * 0.01 * PWINDOW->m_vRealSize.goalv().y);
                        }
                    }

                    if (ONSCREEN) {
                        int borderSize = PWINDOW->getRealBorderSize();

                        posX = std::clamp(posX, (int)(PMONITOR->vecReservedTopLeft.x + borderSize),
                                          (int)(PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x - PWINDOW->m_vRealSize.goalv().x - borderSize));

                        posY = std::clamp(posY, (int)(PMONITOR->vecReservedTopLeft.y + borderSize),
                                          (int)(PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PWINDOW->m_vRealSize.goalv().y - borderSize));
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

                PWINDOW->m_vRealPosition = PMONITOR->middle() - PWINDOW->m_vRealSize.goalv() / 2.f + RESERVEDOFFSET;
            }
        }

        // set the pseudo size to the GOAL of our current size
        // because the windows are animated on RealSize
        PWINDOW->m_vPseudoSize = PWINDOW->m_vRealSize.goalv();

        g_pCompositor->changeWindowZOrder(PWINDOW, true);
    } else {
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(PWINDOW);

        // Set the pseudo size here too so that it doesnt end up being 0x0
        PWINDOW->m_vPseudoSize = PWINDOW->m_vRealSize.goalv() - Vector2D(10, 10);
    }

    const auto PFOCUSEDWINDOWPREV = g_pCompositor->m_pLastWindow;

    if (PWINDOW->m_sAdditionalConfigData.forceAllowsInput) {
        PWINDOW->m_bNoFocus          = false;
        PWINDOW->m_bNoInitialFocus   = false;
        PWINDOW->m_bX11ShouldntFocus = false;
    }

    // check LS focus grab
    const auto PFORCEFOCUS  = g_pCompositor->getForceFocus();
    const auto PLSFROMFOCUS = g_pCompositor->getLayerSurfaceFromSurface(g_pCompositor->m_pLastFocus);
    if (PLSFROMFOCUS && PLSFROMFOCUS->layerSurface->current.keyboard_interactive)
        PWINDOW->m_bNoInitialFocus = true;
    if (PWORKSPACE->m_bHasFullscreenWindow && !requestsFullscreen) {
        if (*PNEWTAKESOVERFS == 0)
            PWINDOW->m_bNoInitialFocus = true;
        else if (*PNEWTAKESOVERFS == 2)
            g_pCompositor->setWindowFullscreen(g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID), false, FULLSCREEN_INVALID);
        else if (PWORKSPACE->m_efFullscreenMode == FULLSCREEN_MAXIMIZED)
            requestsMaximize = true;
        else
            requestsFullscreen = true;
    }

    if (!PWINDOW->m_bNoFocus && !PWINDOW->m_bNoInitialFocus &&
        (PWINDOW->m_iX11Type != 2 || (PWINDOW->m_bIsX11 && wlr_xwayland_or_surface_wants_focus(PWINDOW->m_uSurface.xwayland))) && !workspaceSilent &&
        (!PFORCEFOCUS || PFORCEFOCUS == PWINDOW)) {
        g_pCompositor->focusWindow(PWINDOW);
        PWINDOW->m_fActiveInactiveAlpha.setValueAndWarp(*PACTIVEALPHA);
        PWINDOW->m_fDimPercent.setValueAndWarp(PWINDOW->m_sAdditionalConfigData.forceNoDim ? 0.f : *PDIMSTRENGTH);
    } else {
        PWINDOW->m_fActiveInactiveAlpha.setValueAndWarp(*PINACTIVEALPHA);
        PWINDOW->m_fDimPercent.setValueAndWarp(0);
    }

    Debug::log(LOG, "Window got assigned a surfaceTreeNode {:x}", (uintptr_t)PWINDOW->m_pSurfaceTree);

    if (!PWINDOW->m_bIsX11) {
        PWINDOW->hyprListener_commitWindow.initCallback(&PWINDOW->m_uSurface.xdg->surface->events.commit, &Events::listener_commitWindow, PWINDOW, "XDG Window Late");
        PWINDOW->hyprListener_setTitleWindow.initCallback(&PWINDOW->m_uSurface.xdg->toplevel->events.set_title, &Events::listener_setTitleWindow, PWINDOW, "XDG Window Late");
        PWINDOW->hyprListener_newPopupXDG.initCallback(&PWINDOW->m_uSurface.xdg->events.new_popup, &Events::listener_newPopupXDG, PWINDOW, "XDG Window Late");
        PWINDOW->hyprListener_requestMaximize.initCallback(&PWINDOW->m_uSurface.xdg->toplevel->events.request_maximize, &Events::listener_requestMaximize, PWINDOW,
                                                           "XDG Window Late");
        PWINDOW->hyprListener_requestMinimize.initCallback(&PWINDOW->m_uSurface.xdg->toplevel->events.request_minimize, &Events::listener_requestMinimize, PWINDOW,
                                                           "XDG Window Late");
        PWINDOW->hyprListener_requestMove.initCallback(&PWINDOW->m_uSurface.xdg->toplevel->events.request_move, &Events::listener_requestMove, PWINDOW, "XDG Window Late");
        PWINDOW->hyprListener_requestResize.initCallback(&PWINDOW->m_uSurface.xdg->toplevel->events.request_resize, &Events::listener_requestResize, PWINDOW, "XDG Window Late");
        PWINDOW->hyprListener_fullscreenWindow.initCallback(&PWINDOW->m_uSurface.xdg->toplevel->events.request_fullscreen, &Events::listener_fullscreenWindow, PWINDOW,
                                                            "XDG Window Late");
    } else {
        PWINDOW->hyprListener_fullscreenWindow.initCallback(&PWINDOW->m_uSurface.xwayland->events.request_fullscreen, &Events::listener_fullscreenWindow, PWINDOW,
                                                            "XWayland Window Late");
        PWINDOW->hyprListener_activateX11.initCallback(&PWINDOW->m_uSurface.xwayland->events.request_activate, &Events::listener_activateX11, PWINDOW, "XWayland Window Late");
        PWINDOW->hyprListener_setTitleWindow.initCallback(&PWINDOW->m_uSurface.xwayland->events.set_title, &Events::listener_setTitleWindow, PWINDOW, "XWayland Window Late");
        PWINDOW->hyprListener_requestMinimize.initCallback(&PWINDOW->m_uSurface.xwayland->events.request_minimize, &Events::listener_requestMinimize, PWINDOW,
                                                           "Xwayland Window Late");
        PWINDOW->hyprListener_requestMaximize.initCallback(&PWINDOW->m_uSurface.xwayland->events.request_maximize, &Events::listener_requestMaximize, PWINDOW,
                                                           "Xwayland Window Late");

        if (PWINDOW->m_iX11Type == 2)
            PWINDOW->hyprListener_setGeometryX11U.initCallback(&PWINDOW->m_uSurface.xwayland->events.set_geometry, &Events::listener_unmanagedSetGeometry, PWINDOW,
                                                               "XWayland Window Late");
    }

    // do the animation thing
    g_pAnimationManager->onWindowPostCreateClose(PWINDOW, false);
    PWINDOW->m_fAlpha.setValueAndWarp(0.f);
    PWINDOW->m_fAlpha = 1.f;

    PWINDOW->m_vRealPosition.setCallbackOnEnd(setAnimToMove);
    PWINDOW->m_vRealSize.setCallbackOnEnd(setAnimToMove);

    if ((requestsFullscreen && (!PWINDOW->m_bNoFullscreenRequest || overridingNoFullscreen)) || (requestsMaximize && (!PWINDOW->m_bNoMaximizeRequest || overridingNoMaximize)) ||
        requestsFakeFullscreen) {
        // fix fullscreen on requested (basically do a switcheroo)
        if (PWORKSPACE->m_bHasFullscreenWindow) {
            const auto PFULLWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);
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

    PWINDOW->m_pSurfaceTree = SubsurfaceTree::createTreeRoot(PWINDOW->m_pWLSurface.wlr(), addViewCoords, PWINDOW, PWINDOW);

    PWINDOW->updateToplevel();

    if (workspaceSilent) {
        if (g_pCompositor->windowValidMapped(PFOCUSEDWINDOWPREV)) {
            g_pCompositor->focusWindow(PFOCUSEDWINDOWPREV);
            PFOCUSEDWINDOWPREV->updateWindowDecos(); // need to for some reason i cba to find out why
        } else if (!PFOCUSEDWINDOWPREV)
            g_pCompositor->focusWindow(nullptr);
    }

    // verify swallowing
    if (*PSWALLOW && *PSWALLOWREGEX != STRVAL_EMPTY) {
        // don't swallow ourselves
        std::regex rgx(*PSWALLOWREGEX);
        if (!std::regex_match(g_pXWaylandManager->getAppIDClass(PWINDOW), rgx)) {
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
                std::vector<CWindow*> found;
                CWindow*              finalFound = nullptr;
                for (auto& w : g_pCompositor->m_vWindows) {
                    if (!w->m_bIsMapped || w->isHidden())
                        continue;

                    if (w->getPID() == ppid) {
                        found.push_back(w.get());
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
                    bool valid = std::regex_match(g_pXWaylandManager->getAppIDClass(finalFound), rgx);

                    if (*PSWALLOWEXREGEX != STRVAL_EMPTY) {
                        std::regex exc(*PSWALLOWEXREGEX);

                        valid = valid && !std::regex_match(g_pXWaylandManager->getTitle(finalFound), exc);
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

    Debug::log(LOG, "Map request dispatched, monitor {}, window pos: {:5j}, window size: {:5j}", PMONITOR->szName, PWINDOW->m_vRealPosition.goalv(), PWINDOW->m_vRealSize.goalv());

    auto workspaceID = requestedWorkspace != "" ? requestedWorkspace : PWORKSPACE->m_szName;
    g_pEventManager->postEvent(SHyprIPCEvent{"openwindow", std::format("{:x},{},{},{}", PWINDOW, workspaceID, g_pXWaylandManager->getAppIDClass(PWINDOW), PWINDOW->m_szTitle)});
    EMIT_HOOK_EVENT("openWindow", PWINDOW);

    // recalc the values for this window
    g_pCompositor->updateWindowAnimatedDecorationValues(PWINDOW);
    // avoid this window being visible
    if (PWORKSPACE->m_bHasFullscreenWindow && !PWINDOW->m_bIsFullscreen && !PWINDOW->m_bIsFloating)
        PWINDOW->m_fAlpha.setValueAndWarp(0.f);

    g_pCompositor->setPreferredScaleForSurface(PWINDOW->m_pWLSurface.wlr(), PMONITOR->scale);
    g_pCompositor->setPreferredTransformForSurface(PWINDOW->m_pWLSurface.wlr(), PMONITOR->transform);
}

void Events::listener_unmapWindow(void* owner, void* data) {
    CWindow* PWINDOW = (CWindow*)owner;

    Debug::log(LOG, "{:c} unmapped", PWINDOW);

    if (!PWINDOW->m_pWLSurface.exists() || !PWINDOW->m_bIsMapped) {
        Debug::log(WARN, "{} unmapped without being mapped??", PWINDOW);
        PWINDOW->m_bFadingOut = false;
        return;
    }

    g_pEventManager->postEvent(SHyprIPCEvent{"closewindow", std::format("{:x}", PWINDOW)});
    EMIT_HOOK_EVENT("closeWindow", PWINDOW);

    g_pProtocolManager->m_pToplevelExportProtocolManager->onWindowUnmap(PWINDOW);

    if (!PWINDOW->m_bIsX11) {
        Debug::log(LOG, "Unregistered late callbacks XDG");
        PWINDOW->hyprListener_commitWindow.removeCallback();
        PWINDOW->hyprListener_setTitleWindow.removeCallback();
        PWINDOW->hyprListener_newPopupXDG.removeCallback();
        PWINDOW->hyprListener_requestMaximize.removeCallback();
        PWINDOW->hyprListener_requestMinimize.removeCallback();
        PWINDOW->hyprListener_requestMove.removeCallback();
        PWINDOW->hyprListener_requestResize.removeCallback();
        PWINDOW->hyprListener_fullscreenWindow.removeCallback();
    } else {
        Debug::log(LOG, "Unregistered late callbacks XWL");
        PWINDOW->hyprListener_fullscreenWindow.removeCallback();
        PWINDOW->hyprListener_activateX11.removeCallback();
        PWINDOW->hyprListener_setTitleWindow.removeCallback();
        PWINDOW->hyprListener_setGeometryX11U.removeCallback();
        PWINDOW->hyprListener_requestMaximize.removeCallback();
        PWINDOW->hyprListener_requestMinimize.removeCallback();
    }

    if (PWINDOW->m_bIsFullscreen)
        g_pCompositor->setWindowFullscreen(PWINDOW, false, FULLSCREEN_FULL);

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);
    if (PMONITOR) {
        PWINDOW->m_vOriginalClosedPos     = PWINDOW->m_vRealPosition.vec() - PMONITOR->vecPosition;
        PWINDOW->m_vOriginalClosedSize    = PWINDOW->m_vRealSize.vec();
        PWINDOW->m_eOriginalClosedExtents = PWINDOW->getFullWindowExtents();
    }

    // Allow the renderer to catch the last frame.
    g_pHyprOpenGL->makeWindowSnapshot(PWINDOW);

    // swallowing
    if (PWINDOW->m_pSwallowed && g_pCompositor->windowExists(PWINDOW->m_pSwallowed)) {
        PWINDOW->m_pSwallowed->setHidden(false);
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(PWINDOW->m_pSwallowed);
        PWINDOW->m_pSwallowed = nullptr;
    }

    bool wasLastWindow = false;

    if (PWINDOW == g_pCompositor->m_pLastWindow) {
        wasLastWindow                = true;
        g_pCompositor->m_pLastWindow = nullptr;
        g_pCompositor->m_pLastFocus  = nullptr;

        g_pInputManager->releaseAllMouseButtons();
    }

    PWINDOW->m_bMappedX11 = false;

    // remove the fullscreen window status from workspace if we closed it
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow && PWINDOW->m_bIsFullscreen)
        PWORKSPACE->m_bHasFullscreenWindow = false;

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(PWINDOW);

    // do this after onWindowRemoved because otherwise it'll think the window is invalid
    PWINDOW->m_bIsMapped = false;

    // refocus on a new window if needed
    if (wasLastWindow) {
        const auto PWINDOWCANDIDATE = g_pLayoutManager->getCurrentLayout()->getNextWindowCandidate(PWINDOW);

        Debug::log(LOG, "On closed window, new focused candidate is {}", PWINDOWCANDIDATE);

        if (PWINDOWCANDIDATE != g_pCompositor->m_pLastWindow) {
            if (!PWINDOWCANDIDATE)
                g_pInputManager->simulateMouseMovement();
            else
                g_pCompositor->focusWindow(PWINDOWCANDIDATE);
        } else
            g_pInputManager->simulateMouseMovement();

        // CWindow::onUnmap will remove this window's active status, but we can't really do it above.
        if (PWINDOW == g_pCompositor->m_pLastWindow || !g_pCompositor->m_pLastWindow) {
            g_pEventManager->postEvent(SHyprIPCEvent{"activewindow", ","});
            g_pEventManager->postEvent(SHyprIPCEvent{"activewindowv2", ","});
            EMIT_HOOK_EVENT("activeWindow", (CWindow*)nullptr);
        }
    } else {
        Debug::log(LOG, "Unmapped was not focused, ignoring a refocus.");
    }

    Debug::log(LOG, "Destroying the SubSurface tree of unmapped window {}", PWINDOW);
    SubsurfaceTree::destroySurfaceTree(PWINDOW->m_pSurfaceTree);

    PWINDOW->m_pSurfaceTree = nullptr;

    PWINDOW->m_bFadingOut = true;

    g_pCompositor->addToFadingOutSafe(PWINDOW);

    g_pHyprRenderer->damageMonitor(g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID));

    if (!PWINDOW->m_bX11DoesntWantBorders)                                                  // don't animate out if they weren't animated in.
        PWINDOW->m_vRealPosition = PWINDOW->m_vRealPosition.vec() + Vector2D(0.01f, 0.01f); // it has to be animated, otherwise onWindowPostCreateClose will ignore it

    // anims
    g_pAnimationManager->onWindowPostCreateClose(PWINDOW, true);
    PWINDOW->m_fAlpha = 0.f;

    // Destroy Foreign Toplevel
    PWINDOW->destroyToplevelHandle();

    // recheck idle inhibitors
    g_pInputManager->recheckIdleInhibitorStatus();

    // force report all sizes (QT sometimes has an issue with this)
    g_pCompositor->forceReportSizesToWindowsOnWorkspace(PWINDOW->m_iWorkspaceID);

    // update lastwindow after focus
    PWINDOW->onUnmap();
}

void Events::listener_commitWindow(void* owner, void* data) {
    CWindow* PWINDOW = (CWindow*)owner;

    if (!PWINDOW->m_bMappedX11 || PWINDOW->isHidden() || (PWINDOW->m_bIsX11 && !PWINDOW->m_bMappedX11))
        return;

    PWINDOW->m_vReportedSize = PWINDOW->m_vPendingReportedSize; // apply pending size. We pinged, the window ponged.

    PWINDOW->updateSurfaceOutputs();

    g_pHyprRenderer->damageSurface(PWINDOW->m_pWLSurface.wlr(), PWINDOW->m_vRealPosition.goalv().x, PWINDOW->m_vRealPosition.goalv().y,
                                   PWINDOW->m_bIsX11 ? 1.0 / PWINDOW->m_fX11SurfaceScaledBy : 1.0);

    if (PWINDOW->m_bIsX11 || !PWINDOW->m_bIsFloating || PWINDOW->m_bIsFullscreen)
        return;

    const auto ISRIGID = PWINDOW->m_uSurface.xdg->toplevel->current.max_height == PWINDOW->m_uSurface.xdg->toplevel->current.min_height &&
        PWINDOW->m_uSurface.xdg->toplevel->current.max_width == PWINDOW->m_uSurface.xdg->toplevel->current.min_width;

    if (!ISRIGID)
        return;

    const Vector2D REQUESTEDSIZE = {PWINDOW->m_uSurface.xdg->toplevel->current.max_width, PWINDOW->m_uSurface.xdg->toplevel->current.max_height};

    if (REQUESTEDSIZE == PWINDOW->m_vReportedSize || REQUESTEDSIZE.x < 5 || REQUESTEDSIZE.y < 5)
        return;

    const Vector2D DELTA = PWINDOW->m_vReportedSize - REQUESTEDSIZE;

    PWINDOW->m_vRealPosition = PWINDOW->m_vRealPosition.goalv() + DELTA / 2.0;
    PWINDOW->m_vRealSize     = REQUESTEDSIZE;
    g_pXWaylandManager->setWindowSize(PWINDOW, REQUESTEDSIZE, true);
    g_pHyprRenderer->damageWindow(PWINDOW);
}

void Events::listener_destroyWindow(void* owner, void* data) {
    CWindow* PWINDOW = (CWindow*)owner;

    Debug::log(LOG, "{:c} destroyed, queueing.", PWINDOW);

    if (PWINDOW->m_bIsX11)
        Debug::log(LOG, "XWayland class raw: {}", PWINDOW->m_uSurface.xwayland->_class ? PWINDOW->m_uSurface.xwayland->_class : "null");

    if (PWINDOW == g_pCompositor->m_pLastWindow) {
        g_pCompositor->m_pLastWindow = nullptr;
        g_pCompositor->m_pLastFocus  = nullptr;
    }

    PWINDOW->hyprListener_mapWindow.removeCallback();
    PWINDOW->hyprListener_unmapWindow.removeCallback();
    PWINDOW->hyprListener_destroyWindow.removeCallback();
    PWINDOW->hyprListener_configureX11.removeCallback();
    PWINDOW->hyprListener_setOverrideRedirect.removeCallback();
    PWINDOW->hyprListener_associateX11.removeCallback();
    PWINDOW->hyprListener_dissociateX11.removeCallback();

    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(PWINDOW);

    if (PWINDOW->m_pSurfaceTree) {
        Debug::log(LOG, "Destroying Subsurface tree of {} in destroyWindow", PWINDOW);
        SubsurfaceTree::destroySurfaceTree(PWINDOW->m_pSurfaceTree);
        PWINDOW->m_pSurfaceTree = nullptr;
    }

    PWINDOW->m_bReadyToDelete = true;

    if (!PWINDOW->m_bFadingOut) {
        g_pCompositor->removeWindowFromVectorSafe(PWINDOW); // most likely X11 unmanaged or sumn
        Debug::log(LOG, "Unmapped {} removed instantly", PWINDOW);
    }
}

void Events::listener_setTitleWindow(void* owner, void* data) {
    CWindow* PWINDOW = (CWindow*)owner;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    PWINDOW->m_szTitle = g_pXWaylandManager->getTitle(PWINDOW);
    g_pEventManager->postEvent(SHyprIPCEvent{"windowtitle", std::format("{:x}", (uintptr_t)PWINDOW)});
    EMIT_HOOK_EVENT("windowTitle", PWINDOW);

    if (PWINDOW == g_pCompositor->m_pLastWindow) { // if it's the active, let's post an event to update others
        g_pEventManager->postEvent(SHyprIPCEvent{"activewindow", g_pXWaylandManager->getAppIDClass(PWINDOW) + "," + PWINDOW->m_szTitle});
        g_pEventManager->postEvent(SHyprIPCEvent{"activewindowv2", std::format("{:x}", (uintptr_t)PWINDOW)});
        EMIT_HOOK_EVENT("activeWindow", PWINDOW);
    }

    PWINDOW->updateDynamicRules();
    g_pCompositor->updateWindowAnimatedDecorationValues(PWINDOW);
    PWINDOW->updateToplevel();

    Debug::log(LOG, "Window {:x} set title to {}", PWINDOW, PWINDOW->m_szTitle);
}

void Events::listener_fullscreenWindow(void* owner, void* data) {
    CWindow* PWINDOW = (CWindow*)owner;

    if (!PWINDOW->m_bIsMapped) {
        PWINDOW->m_bWantsInitialFullscreen = true;
        return;
    }

    if (PWINDOW->isHidden() || PWINDOW->m_bNoFullscreenRequest)
        return;

    bool requestedFullState = false;

    if (!PWINDOW->m_bIsX11) {
        const auto REQUESTED = &PWINDOW->m_uSurface.xdg->toplevel->requested;

        if (REQUESTED->fullscreen && PWINDOW->m_bIsFullscreen) {
            const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID);
            if (PWORKSPACE->m_efFullscreenMode != FULLSCREEN_FULL) {
                // Store that we were maximized
                PWINDOW->m_bWasMaximized = true;
                g_pCompositor->setWindowFullscreen(PWINDOW, false, FULLSCREEN_MAXIMIZED);
                g_pCompositor->setWindowFullscreen(PWINDOW, true, FULLSCREEN_FULL);
            } else
                PWINDOW->m_bWasMaximized = false;
        } else if (REQUESTED->fullscreen != PWINDOW->m_bIsFullscreen && !PWINDOW->m_bFakeFullscreenState) {
            g_pCompositor->setWindowFullscreen(PWINDOW, REQUESTED->fullscreen, FULLSCREEN_FULL);
            if (PWINDOW->m_bWasMaximized && !REQUESTED->fullscreen) {
                // Was maximized before the fullscreen request, return now back to maximized instead of normal
                g_pCompositor->setWindowFullscreen(PWINDOW, true, FULLSCREEN_MAXIMIZED);
            }
        }

        // Disable the maximize flag when we receive a de-fullscreen request
        PWINDOW->m_bWasMaximized &= REQUESTED->fullscreen;

        requestedFullState = REQUESTED->fullscreen;

        wlr_xdg_surface_schedule_configure(PWINDOW->m_uSurface.xdg);
    } else {
        if (!PWINDOW->m_uSurface.xwayland->surface->mapped)
            return;

        if (!PWINDOW->m_bFakeFullscreenState)
            g_pCompositor->setWindowFullscreen(PWINDOW, PWINDOW->m_uSurface.xwayland->fullscreen, FULLSCREEN_FULL);

        requestedFullState = PWINDOW->m_uSurface.xwayland->fullscreen;
    }

    if (!requestedFullState && PWINDOW->m_bFakeFullscreenState) {
        g_pXWaylandManager->setWindowFullscreen(PWINDOW, false); // fixes for apps expecting a de-fullscreen (e.g. ff)
        g_pXWaylandManager->setWindowFullscreen(PWINDOW, true);
    }

    PWINDOW->updateToplevel();

    Debug::log(LOG, "{} fullscreen to {}", PWINDOW, PWINDOW->m_bIsFullscreen);
}

void Events::listener_activateXDG(wl_listener* listener, void* data) {
    const auto         E = (wlr_xdg_activation_v1_request_activate_event*)data;

    static auto* const PFOCUSONACTIVATE = &g_pConfigManager->getConfigValuePtr("misc:focus_on_activate")->intValue;

    Debug::log(LOG, "Activate request for surface at {:x}", (uintptr_t)E->surface);

    if (!wlr_xdg_surface_try_from_wlr_surface(E->surface))
        return;

    const auto PWINDOW = g_pCompositor->getWindowFromSurface(E->surface);

    if (!PWINDOW || PWINDOW == g_pCompositor->m_pLastWindow)
        return;

    g_pEventManager->postEvent(SHyprIPCEvent{"urgent", std::format("{:x}", (uintptr_t)PWINDOW)});
    EMIT_HOOK_EVENT("urgent", PWINDOW);

    PWINDOW->m_bIsUrgent = true;

    if (!*PFOCUSONACTIVATE)
        return;

    if (PWINDOW->m_bIsFloating)
        g_pCompositor->changeWindowZOrder(PWINDOW, true);

    g_pCompositor->focusWindow(PWINDOW);
    g_pCompositor->warpCursorTo(PWINDOW->middle());
}

void Events::listener_activateX11(void* owner, void* data) {
    const auto         PWINDOW = (CWindow*)owner;

    static auto* const PFOCUSONACTIVATE = &g_pConfigManager->getConfigValuePtr("misc:focus_on_activate")->intValue;

    Debug::log(LOG, "X11 Activate request for window {}", PWINDOW);

    if (PWINDOW->m_iX11Type == 2) {

        Debug::log(LOG, "Unmanaged X11 {} requests activate", PWINDOW);

        if (g_pCompositor->m_pLastWindow && g_pCompositor->m_pLastWindow->getPID() != PWINDOW->getPID())
            return;

        g_pCompositor->focusWindow(PWINDOW);
        return;
    }

    if (PWINDOW == g_pCompositor->m_pLastWindow)
        return;

    g_pEventManager->postEvent(SHyprIPCEvent{"urgent", std::format("{:x}", (uintptr_t)PWINDOW)});
    EMIT_HOOK_EVENT("urgent", PWINDOW);

    if (!*PFOCUSONACTIVATE)
        return;

    if (PWINDOW->m_bIsFloating)
        g_pCompositor->changeWindowZOrder(PWINDOW, true);

    g_pCompositor->focusWindow(PWINDOW);
    g_pCompositor->warpCursorTo(PWINDOW->middle());
}

void Events::listener_configureX11(void* owner, void* data) {
    CWindow*   PWINDOW = (CWindow*)owner;

    const auto E = (wlr_xwayland_surface_configure_event*)data;

    if (!PWINDOW->m_uSurface.xwayland->surface || !PWINDOW->m_uSurface.xwayland->surface->mapped || !PWINDOW->m_bMappedX11) {
        wlr_xwayland_surface_configure(PWINDOW->m_uSurface.xwayland, E->x, E->y, E->width, E->height);
        return;
    }

    g_pHyprRenderer->damageWindow(PWINDOW);

    if (!PWINDOW->m_bIsFloating || PWINDOW->m_bIsFullscreen || g_pInputManager->currentlyDraggedWindow == PWINDOW) {
        g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vRealSize.goalv(), true);
        g_pInputManager->refocus();
        g_pHyprRenderer->damageWindow(PWINDOW);
        return;
    }

    if (E->width > 1 && E->height > 1)
        PWINDOW->setHidden(false);
    else
        PWINDOW->setHidden(true);

    const auto LOGICALPOS = g_pXWaylandManager->xwaylandToWaylandCoords({E->x, E->y});

    PWINDOW->m_vRealPosition.setValueAndWarp(LOGICALPOS);
    PWINDOW->m_vRealSize.setValueAndWarp(Vector2D(E->width, E->height));

    static auto* const PXWLFORCESCALEZERO = &g_pConfigManager->getConfigValuePtr("xwayland:force_zero_scaling")->intValue;
    if (*PXWLFORCESCALEZERO) {
        if (const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID); PMONITOR)
            PWINDOW->m_vRealSize.setValueAndWarp(PWINDOW->m_vRealSize.goalv() / PMONITOR->scale);
    }

    PWINDOW->m_vPosition = PWINDOW->m_vRealPosition.vec();
    PWINDOW->m_vSize     = PWINDOW->m_vRealSize.vec();

    wlr_xwayland_surface_configure(PWINDOW->m_uSurface.xwayland, E->x, E->y, E->width, E->height);

    PWINDOW->m_iWorkspaceID = g_pCompositor->getMonitorFromVector(PWINDOW->m_vRealPosition.vec() + PWINDOW->m_vRealSize.vec() / 2.f)->activeWorkspace;

    g_pCompositor->changeWindowZOrder(PWINDOW, true);

    PWINDOW->m_bCreatedOverFullscreen = true;

    if (!PWINDOW->m_sAdditionalConfigData.windowDanceCompat)
        g_pInputManager->refocus();

    g_pHyprRenderer->damageWindow(PWINDOW);

    PWINDOW->updateWindowDecos();
}

void Events::listener_unmanagedSetGeometry(void* owner, void* data) {
    CWindow* PWINDOW = (CWindow*)owner;

    if (!PWINDOW->m_bMappedX11)
        return;

    const auto POS = PWINDOW->m_vRealPosition.goalv();
    const auto SIZ = PWINDOW->m_vRealSize.goalv();

    if (PWINDOW->m_uSurface.xwayland->width > 1 && PWINDOW->m_uSurface.xwayland->height > 1)
        PWINDOW->setHidden(false);
    else
        PWINDOW->setHidden(true);

    if (PWINDOW->m_bIsFullscreen || !PWINDOW->m_bIsFloating) {
        g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vRealSize.goalv(), true);
        g_pHyprRenderer->damageWindow(PWINDOW);
        return;
    }

    static auto* const PXWLFORCESCALEZERO = &g_pConfigManager->getConfigValuePtr("xwayland:force_zero_scaling")->intValue;

    const auto         LOGICALPOS = g_pXWaylandManager->xwaylandToWaylandCoords({PWINDOW->m_uSurface.xwayland->x, PWINDOW->m_uSurface.xwayland->y});

    if (abs(std::floor(POS.x) - LOGICALPOS.x) > 2 || abs(std::floor(POS.y) - LOGICALPOS.y) > 2 || abs(std::floor(SIZ.x) - PWINDOW->m_uSurface.xwayland->width) > 2 ||
        abs(std::floor(SIZ.y) - PWINDOW->m_uSurface.xwayland->height) > 2) {
        Debug::log(LOG, "Unmanaged window {} requests geometry update to {:j} {} {}", PWINDOW, LOGICALPOS, (int)PWINDOW->m_uSurface.xwayland->width,
                   (int)PWINDOW->m_uSurface.xwayland->height);

        g_pHyprRenderer->damageWindow(PWINDOW);
        PWINDOW->m_vRealPosition.setValueAndWarp(Vector2D(LOGICALPOS.x, LOGICALPOS.y));

        if (abs(std::floor(SIZ.x) - PWINDOW->m_uSurface.xwayland->width) > 2 || abs(std::floor(SIZ.y) - PWINDOW->m_uSurface.xwayland->height) > 2)
            PWINDOW->m_vRealSize.setValueAndWarp(Vector2D(PWINDOW->m_uSurface.xwayland->width, PWINDOW->m_uSurface.xwayland->height));

        if (*PXWLFORCESCALEZERO) {
            if (const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID); PMONITOR)
                PWINDOW->m_vRealSize.setValueAndWarp(PWINDOW->m_vRealSize.goalv() / PMONITOR->scale);
        }

        PWINDOW->m_vPosition = PWINDOW->m_vRealPosition.goalv();
        PWINDOW->m_vSize     = PWINDOW->m_vRealSize.goalv();

        PWINDOW->m_iWorkspaceID = g_pCompositor->getMonitorFromVector(PWINDOW->m_vRealPosition.vec() + PWINDOW->m_vRealSize.vec() / 2.f)->activeWorkspace;

        g_pCompositor->changeWindowZOrder(PWINDOW, true);
        PWINDOW->updateWindowDecos();
        g_pHyprRenderer->damageWindow(PWINDOW);
    }
}

void Events::listener_setOverrideRedirect(void* owner, void* data) {
    // const auto PWINDOW = (CWindow*)owner;

    //if (!PWINDOW->m_bIsMapped && PWINDOW->m_uSurface.xwayland->mapped) {
    //    Events::listener_mapWindow(PWINDOW, nullptr);
    //}
}

void Events::listener_associateX11(void* owner, void* data) {
    const auto PWINDOW = (CWindow*)owner;

    PWINDOW->hyprListener_mapWindow.initCallback(&PWINDOW->m_uSurface.xwayland->surface->events.map, &Events::listener_mapWindow, PWINDOW, "XWayland Window");
}

void Events::listener_dissociateX11(void* owner, void* data) {
    const auto PWINDOW = (CWindow*)owner;

    PWINDOW->hyprListener_mapWindow.removeCallback();
}

void Events::listener_surfaceXWayland(wl_listener* listener, void* data) {
    const auto XWSURFACE = (wlr_xwayland_surface*)data;

    Debug::log(LOG, "New XWayland Surface created (class {}).", XWSURFACE->_class ? XWSURFACE->_class : "null");
    if (XWSURFACE->parent)
        Debug::log(LOG, "Window parent data: {} at {:x}", XWSURFACE->parent->_class ? XWSURFACE->parent->_class : "null", (uintptr_t)XWSURFACE->parent);

    const auto PNEWWINDOW = (CWindow*)g_pCompositor->m_vWindows.emplace_back(std::make_unique<CWindow>()).get();

    PNEWWINDOW->m_uSurface.xwayland = XWSURFACE;
    PNEWWINDOW->m_iX11Type          = XWSURFACE->override_redirect ? 2 : 1;
    PNEWWINDOW->m_bIsX11            = true;

    PNEWWINDOW->m_pX11Parent = g_pCompositor->getX11Parent(PNEWWINDOW);

    PNEWWINDOW->hyprListener_associateX11.initCallback(&XWSURFACE->events.associate, &Events::listener_associateX11, PNEWWINDOW, "XWayland Window");
    PNEWWINDOW->hyprListener_dissociateX11.initCallback(&XWSURFACE->events.dissociate, &Events::listener_dissociateX11, PNEWWINDOW, "XWayland Window");
    PNEWWINDOW->hyprListener_destroyWindow.initCallback(&XWSURFACE->events.destroy, &Events::listener_destroyWindow, PNEWWINDOW, "XWayland Window");
    PNEWWINDOW->hyprListener_setOverrideRedirect.initCallback(&XWSURFACE->events.set_override_redirect, &Events::listener_setOverrideRedirect, PNEWWINDOW, "XWayland Window");
    PNEWWINDOW->hyprListener_configureX11.initCallback(&XWSURFACE->events.request_configure, &Events::listener_configureX11, PNEWWINDOW, "XWayland Window");
}

void Events::listener_newXDGSurface(wl_listener* listener, void* data) {
    // A window got opened
    const auto XDGSURFACE = (wlr_xdg_surface*)data;

    if (XDGSURFACE->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
        return;

    Debug::log(LOG, "New XDG Surface created. (class: {})", XDGSURFACE->toplevel->app_id ? XDGSURFACE->toplevel->app_id : "null");

    const auto PNEWWINDOW      = g_pCompositor->m_vWindows.emplace_back(std::make_unique<CWindow>()).get();
    PNEWWINDOW->m_uSurface.xdg = XDGSURFACE;

    PNEWWINDOW->hyprListener_mapWindow.initCallback(&XDGSURFACE->surface->events.map, &Events::listener_mapWindow, PNEWWINDOW, "XDG Window");
    PNEWWINDOW->hyprListener_destroyWindow.initCallback(&XDGSURFACE->events.destroy, &Events::listener_destroyWindow, PNEWWINDOW, "XDG Window");
}

void Events::listener_NewXDGDeco(wl_listener* listener, void* data) {
    const auto WLRDECO = (wlr_xdg_toplevel_decoration_v1*)data;
    wlr_xdg_toplevel_decoration_v1_set_mode(WLRDECO, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

void Events::listener_requestMaximize(void* owner, void* data) {
    const auto PWINDOW = (CWindow*)owner;

    if (PWINDOW->m_bNoMaximizeRequest)
        return;

    Debug::log(LOG, "Maximize request for {}", PWINDOW);
    if (!PWINDOW->m_bIsX11) {
        const auto EV = (wlr_foreign_toplevel_handle_v1_maximized_event*)data;

        g_pCompositor->setWindowFullscreen(PWINDOW, EV ? EV->maximized : !PWINDOW->m_bIsFullscreen,
                                           FULLSCREEN_MAXIMIZED); // this will be rejected if there already is a fullscreen window

        wlr_xdg_surface_schedule_configure(PWINDOW->m_uSurface.xdg);
    } else {
        if (!PWINDOW->m_bMappedX11 || PWINDOW->m_iX11Type != 1)
            return;

        g_pCompositor->setWindowFullscreen(PWINDOW, !PWINDOW->m_bIsFullscreen, FULLSCREEN_MAXIMIZED);
    }
}

void Events::listener_requestMinimize(void* owner, void* data) {
    const auto PWINDOW = (CWindow*)owner;

    Debug::log(LOG, "Minimize request for {}", PWINDOW);

    if (PWINDOW->m_bIsX11) {
        if (!PWINDOW->m_bMappedX11 || PWINDOW->m_iX11Type != 1)
            return;

        const auto E = (wlr_xwayland_minimize_event*)data;

        g_pEventManager->postEvent({"minimize", std::format("{:x},{}", (uintptr_t)PWINDOW, (int)E->minimize)});
        EMIT_HOOK_EVENT("minimize", (std::vector<void*>{PWINDOW, (void*)E->minimize}));

        wlr_xwayland_surface_set_minimized(PWINDOW->m_uSurface.xwayland, E->minimize && g_pCompositor->m_pLastWindow != PWINDOW); // fucking DXVK
    } else {
        const auto E = (wlr_foreign_toplevel_handle_v1_minimized_event*)data;
        g_pEventManager->postEvent({"minimize", std::format("{:x},{}", (uintptr_t)PWINDOW, E ? (int)E->minimized : 1)});
        EMIT_HOOK_EVENT("minimize", (std::vector<void*>{PWINDOW, (void*)(E ? (uint64_t)E->minimized : 1)}));
    }
}

void Events::listener_requestMove(void* owner, void* data) {
    const auto PWINDOW = (CWindow*)owner;

    // ignore
    wlr_xdg_surface_schedule_configure(PWINDOW->m_uSurface.xdg);
}

void Events::listener_requestResize(void* owner, void* data) {
    const auto PWINDOW = (CWindow*)owner;

    // ignore
    wlr_xdg_surface_schedule_configure(PWINDOW->m_uSurface.xdg);
}
