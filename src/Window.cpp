#include "Window.hpp"
#include "Compositor.hpp"
#include "render/decorations/CHyprDropShadowDecoration.hpp"

CWindow::CWindow() {
    m_vRealPosition.create(AVARTYPE_VECTOR, g_pConfigManager->getAnimationPropertyConfig("windowsIn"), (void*)this, AVARDAMAGE_ENTIRE);
    m_vRealSize.create(AVARTYPE_VECTOR, g_pConfigManager->getAnimationPropertyConfig("windowsIn"), (void*)this, AVARDAMAGE_ENTIRE);
    m_fBorderFadeAnimationProgress.create(AVARTYPE_FLOAT, g_pConfigManager->getAnimationPropertyConfig("border"), (void*)this, AVARDAMAGE_BORDER);
    m_fBorderAngleAnimationProgress.create(AVARTYPE_FLOAT, g_pConfigManager->getAnimationPropertyConfig("borderangle"), (void*)this, AVARDAMAGE_BORDER);
    m_fAlpha.create(AVARTYPE_FLOAT, g_pConfigManager->getAnimationPropertyConfig("fadeIn"), (void*)this, AVARDAMAGE_ENTIRE);
    m_fActiveInactiveAlpha.create(AVARTYPE_FLOAT, g_pConfigManager->getAnimationPropertyConfig("fadeSwitch"), (void*)this, AVARDAMAGE_ENTIRE);
    m_cRealShadowColor.create(AVARTYPE_COLOR, g_pConfigManager->getAnimationPropertyConfig("fadeShadow"), (void*)this, AVARDAMAGE_SHADOW);
    m_fDimPercent.create(AVARTYPE_FLOAT, g_pConfigManager->getAnimationPropertyConfig("fadeDim"), (void*)this, AVARDAMAGE_ENTIRE);

    m_dWindowDecorations.emplace_back(std::make_unique<CHyprDropShadowDecoration>(this)); // put the shadow so it's the first deco (has to be rendered first)
}

CWindow::~CWindow() {
    if (g_pCompositor->isWindowActive(this)) {
        g_pCompositor->m_pLastFocus  = nullptr;
        g_pCompositor->m_pLastWindow = nullptr;
    }
}

wlr_box CWindow::getFullWindowBoundingBox() {
    static auto* const PBORDERSIZE = &g_pConfigManager->getConfigValuePtr("general:border_size")->intValue;

    if (m_sAdditionalConfigData.dimAround) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(m_iMonitorID);
        return {PMONITOR->vecPosition.x, PMONITOR->vecPosition.y, PMONITOR->vecSize.x, PMONITOR->vecSize.y};
    }

    SWindowDecorationExtents maxExtents = {{*PBORDERSIZE + 2, *PBORDERSIZE + 2}, {*PBORDERSIZE + 2, *PBORDERSIZE + 2}};

    for (auto& wd : m_dWindowDecorations) {

        const auto EXTENTS = wd->getWindowDecorationExtents();

        if (EXTENTS.topLeft.x > maxExtents.topLeft.x)
            maxExtents.topLeft.x = EXTENTS.topLeft.x;

        if (EXTENTS.topLeft.y > maxExtents.topLeft.y)
            maxExtents.topLeft.y = EXTENTS.topLeft.y;

        if (EXTENTS.bottomRight.x > maxExtents.bottomRight.x)
            maxExtents.bottomRight.x = EXTENTS.bottomRight.x;

        if (EXTENTS.bottomRight.y > maxExtents.bottomRight.y)
            maxExtents.bottomRight.y = EXTENTS.bottomRight.y;
    }

    // Add extents to the real base BB and return
    wlr_box finalBox = {m_vRealPosition.vec().x - maxExtents.topLeft.x, m_vRealPosition.vec().y - maxExtents.topLeft.y,
                        m_vRealSize.vec().x + maxExtents.topLeft.x + maxExtents.bottomRight.x, m_vRealSize.vec().y + maxExtents.topLeft.y + maxExtents.bottomRight.y};

    return finalBox;
}

wlr_box CWindow::getWindowIdealBoundingBoxIgnoreReserved() {

    const auto PMONITOR = g_pCompositor->getMonitorFromID(m_iMonitorID);

    auto       POS  = m_vPosition;
    auto       SIZE = m_vSize;

    if (m_bIsFullscreen) {
        POS  = PMONITOR->vecPosition;
        SIZE = PMONITOR->vecSize;

        return wlr_box{(int)POS.x, (int)POS.y, (int)SIZE.x, (int)SIZE.y};
    }

    if (DELTALESSTHAN(POS.y - PMONITOR->vecPosition.y, PMONITOR->vecReservedTopLeft.y, 1)) {
        POS.y = PMONITOR->vecPosition.y;
        SIZE.y += PMONITOR->vecReservedTopLeft.y;
    }
    if (DELTALESSTHAN(POS.x - PMONITOR->vecPosition.x, PMONITOR->vecReservedTopLeft.x, 1)) {
        POS.x = PMONITOR->vecPosition.x;
        SIZE.x += PMONITOR->vecReservedTopLeft.x;
    }
    if (DELTALESSTHAN(POS.x + SIZE.x - PMONITOR->vecPosition.x, PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x, 1)) {
        SIZE.x += PMONITOR->vecReservedBottomRight.x;
    }
    if (DELTALESSTHAN(POS.y + SIZE.y - PMONITOR->vecPosition.y, PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y, 1)) {
        SIZE.y += PMONITOR->vecReservedBottomRight.y;
    }

    return wlr_box{(int)POS.x, (int)POS.y, (int)SIZE.x, (int)SIZE.y};
}

wlr_box CWindow::getWindowInputBox() {
    static auto* const PBORDERSIZE = &g_pConfigManager->getConfigValuePtr("general:border_size")->intValue;

    if (m_sAdditionalConfigData.dimAround) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(m_iMonitorID);
        return {PMONITOR->vecPosition.x, PMONITOR->vecPosition.y, PMONITOR->vecSize.x, PMONITOR->vecSize.y};
    }

    SWindowDecorationExtents maxExtents = {{*PBORDERSIZE + 2, *PBORDERSIZE + 2}, {*PBORDERSIZE + 2, *PBORDERSIZE + 2}};

    for (auto& wd : m_dWindowDecorations) {

        if (!wd->allowsInput())
            continue;

        const auto EXTENTS = wd->getWindowDecorationExtents();

        if (EXTENTS.topLeft.x > maxExtents.topLeft.x)
            maxExtents.topLeft.x = EXTENTS.topLeft.x;

        if (EXTENTS.topLeft.y > maxExtents.topLeft.y)
            maxExtents.topLeft.y = EXTENTS.topLeft.y;

        if (EXTENTS.bottomRight.x > maxExtents.bottomRight.x)
            maxExtents.bottomRight.x = EXTENTS.bottomRight.x;

        if (EXTENTS.bottomRight.y > maxExtents.bottomRight.y)
            maxExtents.bottomRight.y = EXTENTS.bottomRight.y;
    }

    // Add extents to the real base BB and return
    wlr_box finalBox = {m_vRealPosition.vec().x - maxExtents.topLeft.x, m_vRealPosition.vec().y - maxExtents.topLeft.y,
                        m_vRealSize.vec().x + maxExtents.topLeft.x + maxExtents.bottomRight.x, m_vRealSize.vec().y + maxExtents.topLeft.y + maxExtents.bottomRight.y};

    return finalBox;
}

SWindowDecorationExtents CWindow::getFullWindowReservedArea() {
    SWindowDecorationExtents extents;

    for (auto& wd : m_dWindowDecorations) {
        const auto RESERVED = wd->getWindowDecorationReservedArea();

        if (RESERVED.bottomRight == Vector2D{} && RESERVED.topLeft == Vector2D{})
            continue;

        extents.topLeft     = extents.topLeft + RESERVED.topLeft;
        extents.bottomRight = extents.bottomRight + RESERVED.bottomRight;
    }

    return extents;
}

void CWindow::updateWindowDecos() {
    for (auto& wd : m_dWindowDecorations)
        wd->updateWindow(this);

    for (auto& wd : m_vDecosToRemove) {
        for (auto it = m_dWindowDecorations.begin(); it != m_dWindowDecorations.end(); it++) {
            if (it->get() == wd) {
                it = m_dWindowDecorations.erase(it);
                if (it == m_dWindowDecorations.end())
                    break;
            }
        }
    }

    m_vDecosToRemove.clear();
}

pid_t CWindow::getPID() {
    pid_t PID = -1;
    if (!m_bIsX11) {

        if (!m_bIsMapped)
            return -1;

        wl_client_get_credentials(wl_resource_get_client(m_uSurface.xdg->resource), &PID, nullptr, nullptr);
    } else {
        PID = m_uSurface.xwayland->pid;
    }

    return PID;
}

IHyprWindowDecoration* CWindow::getDecorationByType(eDecorationType type) {
    for (auto& wd : m_dWindowDecorations) {
        if (wd->getDecorationType() == type)
            return wd.get();
    }

    return nullptr;
}

void CWindow::createToplevelHandle() {
    if (m_bIsX11 && (m_bX11DoesntWantBorders || m_iX11Type == 2))
        return; // don't create a toplevel

    m_phForeignToplevel = wlr_foreign_toplevel_handle_v1_create(g_pCompositor->m_sWLRToplevelMgr);

    wlr_foreign_toplevel_handle_v1_set_app_id(m_phForeignToplevel, g_pXWaylandManager->getAppIDClass(this).c_str());
    wlr_foreign_toplevel_handle_v1_output_enter(m_phForeignToplevel, g_pCompositor->getMonitorFromID(m_iMonitorID)->output);
    wlr_foreign_toplevel_handle_v1_set_title(m_phForeignToplevel, m_szTitle.c_str());
    wlr_foreign_toplevel_handle_v1_set_maximized(m_phForeignToplevel, false);
    wlr_foreign_toplevel_handle_v1_set_minimized(m_phForeignToplevel, false);
    wlr_foreign_toplevel_handle_v1_set_fullscreen(m_phForeignToplevel, false);

    // handle events
    hyprListener_toplevelActivate.initCallback(
        &m_phForeignToplevel->events.request_activate, [&](void* owner, void* data) { g_pCompositor->focusWindow(this); }, this, "Toplevel");

    hyprListener_toplevelFullscreen.initCallback(
        &m_phForeignToplevel->events.request_fullscreen,
        [&](void* owner, void* data) {
            const auto EV = (wlr_foreign_toplevel_handle_v1_fullscreen_event*)data;

            g_pCompositor->setWindowFullscreen(this, EV->fullscreen, FULLSCREEN_FULL);
        },
        this, "Toplevel");

    hyprListener_toplevelClose.initCallback(
        &m_phForeignToplevel->events.request_close, [&](void* owner, void* data) { g_pCompositor->closeWindow(this); }, this, "Toplevel");

    m_iLastToplevelMonitorID = m_iMonitorID;
}

void CWindow::destroyToplevelHandle() {
    if (!m_phForeignToplevel)
        return;

    hyprListener_toplevelActivate.removeCallback();
    hyprListener_toplevelClose.removeCallback();
    hyprListener_toplevelFullscreen.removeCallback();

    wlr_foreign_toplevel_handle_v1_destroy(m_phForeignToplevel);
    m_phForeignToplevel = nullptr;
}

void CWindow::updateToplevel() {
    updateSurfaceOutputs();

    if (!m_phForeignToplevel)
        return;

    wlr_foreign_toplevel_handle_v1_set_title(m_phForeignToplevel, m_szTitle.c_str());
    wlr_foreign_toplevel_handle_v1_set_fullscreen(m_phForeignToplevel, m_bIsFullscreen);

    if (m_iLastToplevelMonitorID != m_iMonitorID) {
        if (const auto PMONITOR = g_pCompositor->getMonitorFromID(m_iLastToplevelMonitorID); PMONITOR && PMONITOR->m_bEnabled)
            wlr_foreign_toplevel_handle_v1_output_leave(m_phForeignToplevel, PMONITOR->output);
        wlr_foreign_toplevel_handle_v1_output_enter(m_phForeignToplevel, g_pCompositor->getMonitorFromID(m_iMonitorID)->output);

        m_iLastToplevelMonitorID = m_iMonitorID;
    }
}

void sendEnterIter(wlr_surface* pSurface, int x, int y, void* data) {
    const auto OUTPUT = (wlr_output*)data;
    wlr_surface_send_enter(pSurface, OUTPUT);
}

void sendLeaveIter(wlr_surface* pSurface, int x, int y, void* data) {
    const auto OUTPUT = (wlr_output*)data;
    wlr_surface_send_leave(pSurface, OUTPUT);
}

void CWindow::updateSurfaceOutputs() {
    if (m_iLastSurfaceMonitorID == m_iMonitorID || !m_bIsMapped || m_bHidden || !m_bMappedX11)
        return;

    const auto PLASTMONITOR = g_pCompositor->getMonitorFromID(m_iLastSurfaceMonitorID);

    m_iLastSurfaceMonitorID = m_iMonitorID;

    const auto PNEWMONITOR = g_pCompositor->getMonitorFromID(m_iMonitorID);

    if (PLASTMONITOR && PLASTMONITOR->m_bEnabled)
        wlr_surface_for_each_surface(m_pWLSurface.wlr(), sendLeaveIter, PLASTMONITOR->output);

    wlr_surface_for_each_surface(m_pWLSurface.wlr(), sendEnterIter, PNEWMONITOR->output);
}

void CWindow::moveToWorkspace(int workspaceID) {
    if (m_iWorkspaceID == workspaceID)
        return;

    m_iWorkspaceID = workspaceID;

    const auto PMONITOR   = g_pCompositor->getMonitorFromID(m_iMonitorID);
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(m_iWorkspaceID);

    if (PWORKSPACE) {
        g_pEventManager->postEvent(SHyprIPCEvent{"movewindow", getFormat("%x,%s", this, PWORKSPACE->m_szName.c_str())});
        EMIT_HOOK_EVENT("moveWindow", (std::vector<void*>{this, PWORKSPACE}));
    }

    if (m_pSwallowed) {
        m_pSwallowed->moveToWorkspace(workspaceID);
        m_pSwallowed->m_iMonitorID = m_iMonitorID;
    }

    if (PMONITOR)
        g_pProtocolManager->m_pFractionalScaleProtocolManager->setPreferredScaleForSurface(m_pWLSurface.wlr(), PMONITOR->scale);
}

CWindow* CWindow::X11TransientFor() {
    if (!m_bIsX11)
        return nullptr;

    if (!m_uSurface.xwayland->parent)
        return nullptr;

    auto PPARENT = g_pCompositor->getWindowFromSurface(m_uSurface.xwayland->parent->surface);

    while (g_pCompositor->windowValidMapped(PPARENT) && PPARENT->m_uSurface.xwayland->parent) {
        PPARENT = g_pCompositor->getWindowFromSurface(PPARENT->m_uSurface.xwayland->parent->surface);
    }

    if (!g_pCompositor->windowValidMapped(PPARENT))
        return nullptr;

    return PPARENT;
}

void CWindow::removeDecorationByType(eDecorationType type) {
    for (auto& wd : m_dWindowDecorations) {
        if (wd->getDecorationType() == type)
            m_vDecosToRemove.push_back(wd.get());
    }

    updateWindowDecos();
}

void unregisterVar(void* ptr) {
    ((CAnimatedVariable*)ptr)->unregister();
}

void CWindow::onUnmap() {
    if (g_pCompositor->m_pLastWindow == this)
        g_pCompositor->m_pLastWindow = nullptr;

    m_vRealPosition.setCallbackOnEnd(unregisterVar);
    m_vRealSize.setCallbackOnEnd(unregisterVar);
    m_fBorderFadeAnimationProgress.setCallbackOnEnd(unregisterVar);
    m_fBorderAngleAnimationProgress.setCallbackOnEnd(unregisterVar);
    m_fActiveInactiveAlpha.setCallbackOnEnd(unregisterVar);
    m_fAlpha.setCallbackOnEnd(unregisterVar);
    m_cRealShadowColor.setCallbackOnEnd(unregisterVar);
    m_fDimPercent.setCallbackOnEnd(unregisterVar);

    m_vRealSize.setCallbackOnBegin(nullptr);

    std::erase_if(g_pCompositor->m_vWindowFocusHistory, [&](const auto& other) { return other == this; });

    m_pWLSurface.unassign();

    hyprListener_unmapWindow.removeCallback();
}

void CWindow::onMap() {

    m_pWLSurface.assign(g_pXWaylandManager->getWindowSurface(this));

    // JIC, reset the callbacks. If any are set, we'll make sure they are cleared so we don't accidentally unset them. (In case a window got remapped)
    m_vRealPosition.resetAllCallbacks();
    m_vRealSize.resetAllCallbacks();
    m_fBorderFadeAnimationProgress.resetAllCallbacks();
    m_fBorderAngleAnimationProgress.resetAllCallbacks();
    m_fActiveInactiveAlpha.resetAllCallbacks();
    m_fAlpha.resetAllCallbacks();
    m_cRealShadowColor.resetAllCallbacks();
    m_fDimPercent.resetAllCallbacks();

    m_vRealPosition.registerVar();
    m_vRealSize.registerVar();
    m_fBorderFadeAnimationProgress.registerVar();
    m_fBorderAngleAnimationProgress.registerVar();
    m_fActiveInactiveAlpha.registerVar();
    m_fAlpha.registerVar();
    m_cRealShadowColor.registerVar();
    m_fDimPercent.registerVar();

    m_fBorderAngleAnimationProgress.setCallbackOnEnd([&](void* ptr) { onBorderAngleAnimEnd(ptr); }, false);

    m_fBorderAngleAnimationProgress.setValueAndWarp(0.f);
    m_fBorderAngleAnimationProgress = 1.f;

    g_pCompositor->m_vWindowFocusHistory.push_back(this);

    hyprListener_unmapWindow.initCallback(m_bIsX11 ? &m_uSurface.xwayland->events.unmap : &m_uSurface.xdg->events.unmap, &Events::listener_unmapWindow, this, "CWindow");
}

void CWindow::onBorderAngleAnimEnd(void* ptr) {
    const auto        PANIMVAR = (CAnimatedVariable*)ptr;

    const std::string STYLE = PANIMVAR->getConfig()->pValues->internalStyle;

    if (STYLE != "loop" || !PANIMVAR->getConfig()->pValues->internalEnabled)
        return;

    PANIMVAR->setCallbackOnEnd(nullptr); // we remove the callback here because otherwise setvalueandwarp will recurse this

    PANIMVAR->setValueAndWarp(0);
    *PANIMVAR = 1.f;

    PANIMVAR->setCallbackOnEnd([&](void* ptr) { onBorderAngleAnimEnd(ptr); }, false);
}

void CWindow::setHidden(bool hidden) {
    m_bHidden = hidden;

    if (hidden && g_pCompositor->m_pLastWindow == this) {
        g_pCompositor->m_pLastWindow = nullptr;
    }
}

bool CWindow::isHidden() {
    return m_bHidden;
}

void CWindow::applyDynamicRule(const SWindowRule& r) {
    if (r.szRule == "noblur") {
        m_sAdditionalConfigData.forceNoBlur = true;
    } else if (r.szRule == "noborder") {
        m_sAdditionalConfigData.forceNoBorder = true;
    } else if (r.szRule == "noshadow") {
        m_sAdditionalConfigData.forceNoShadow = true;
    } else if (r.szRule == "forcergbx") {
        m_sAdditionalConfigData.forceRGBX = true;
    } else if (r.szRule == "opaque") {
        if (!m_sAdditionalConfigData.forceOpaqueOverridden)
            m_sAdditionalConfigData.forceOpaque = true;
    } else if (r.szRule.find("rounding") == 0) {
        try {
            m_sAdditionalConfigData.rounding = std::stoi(r.szRule.substr(r.szRule.find_first_of(' ') + 1));
        } catch (std::exception& e) { Debug::log(ERR, "Rounding rule \"%s\" failed with: %s", r.szRule.c_str(), e.what()); }
    } else if (r.szRule.find("opacity") == 0) {
        try {
            CVarList vars(r.szRule, 0, ' ');

            for (size_t i = 1 /* first item is "opacity" */; i < vars.size(); ++i) {
                if (i == 1) {
                    // first arg, alpha
                    m_sSpecialRenderData.alpha = std::stof(vars[i]);
                } else {
                    if (vars[i] == "override") {
                        if (i == 2) {
                            m_sSpecialRenderData.alphaOverride = true;
                        } else {
                            m_sSpecialRenderData.alphaInactiveOverride = true;
                        }
                    } else {
                        m_sSpecialRenderData.alphaInactive = std::stof(vars[i]);
                    }
                }
            }
        } catch (std::exception& e) { Debug::log(ERR, "Opacity rule \"%s\" failed with: %s", r.szRule.c_str(), e.what()); }
    } else if (r.szRule == "noanim") {
        m_sAdditionalConfigData.forceNoAnims = true;
    } else if (r.szRule.find("animation") == 0) {
        auto STYLE                             = r.szRule.substr(r.szRule.find_first_of(' ') + 1);
        m_sAdditionalConfigData.animationStyle = STYLE;
    } else if (r.szRule.find("bordercolor") == 0) {
        try {
            std::string colorPart = removeBeginEndSpacesTabs(r.szRule.substr(r.szRule.find_first_of(' ') + 1));

            if (colorPart.contains(' ')) {
                // we have a space, 2 values
                m_sSpecialRenderData.activeBorderColor   = configStringToInt(colorPart.substr(0, colorPart.find_first_of(' ')));
                m_sSpecialRenderData.inactiveBorderColor = configStringToInt(colorPart.substr(colorPart.find_first_of(' ') + 1));
            } else {
                m_sSpecialRenderData.activeBorderColor = configStringToInt(colorPart);
            }
        } catch (std::exception& e) { Debug::log(ERR, "BorderColor rule \"%s\" failed with: %s", r.szRule.c_str(), e.what()); }
    } else if (r.szRule == "dimaround") {
        m_sAdditionalConfigData.dimAround = true;
    }
}

void CWindow::updateDynamicRules() {
    m_sSpecialRenderData.activeBorderColor   = -1;
    m_sSpecialRenderData.inactiveBorderColor = -1;
    m_sSpecialRenderData.alpha               = 1.f;
    m_sSpecialRenderData.alphaInactive       = -1.f;
    m_sAdditionalConfigData.forceNoBlur      = false;
    m_sAdditionalConfigData.forceNoBorder    = false;
    m_sAdditionalConfigData.forceNoShadow    = false;
    if (!m_sAdditionalConfigData.forceOpaqueOverridden)
        m_sAdditionalConfigData.forceOpaque = false;
    m_sAdditionalConfigData.forceNoAnims   = false;
    m_sAdditionalConfigData.animationStyle = std::string("");
    m_sAdditionalConfigData.rounding       = -1;
    m_sAdditionalConfigData.dimAround      = false;
    m_sAdditionalConfigData.forceRGBX      = false;

    const auto WINDOWRULES = g_pConfigManager->getMatchingRules(this);
    for (auto& r : WINDOWRULES) {
        applyDynamicRule(r);
    }
}

// check if the point is "hidden" under a rounded corner of the window
// it is assumed that the point is within the real window box (m_vRealPosition, m_vRealSize)
// otherwise behaviour is undefined
bool CWindow::isInCurvedCorner(double x, double y) {
    static auto* const ROUNDING   = &g_pConfigManager->getConfigValuePtr("decoration:rounding")->intValue;
    static auto* const BORDERSIZE = &g_pConfigManager->getConfigValuePtr("general:border_size")->intValue;

    if (BORDERSIZE >= ROUNDING || ROUNDING == 0)
        return false;

    // (x0, y0), (x0, y1), ... are the center point of rounding at each corner
    double x0 = m_vRealPosition.vec().x + *ROUNDING;
    double y0 = m_vRealPosition.vec().y + *ROUNDING;
    double x1 = m_vRealPosition.vec().x + m_vRealSize.vec().x - *ROUNDING;
    double y1 = m_vRealPosition.vec().y + m_vRealSize.vec().y - *ROUNDING;

    if (x < x0 && y < y0) {
        return Vector2D{x0, y0}.distance(Vector2D{x, y}) > (double)*ROUNDING;
    }
    if (x > x1 && y < y0) {
        return Vector2D{x1, y0}.distance(Vector2D{x, y}) > (double)*ROUNDING;
    }
    if (x < x0 && y > y1) {
        return Vector2D{x0, y1}.distance(Vector2D{x, y}) > (double)*ROUNDING;
    }
    if (x > x1 && y > y1) {
        return Vector2D{x1, y1}.distance(Vector2D{x, y}) > (double)*ROUNDING;
    }

    return false;
}

void findExtensionForVector2D(wlr_surface* surface, int x, int y, void* data) {
    const auto DATA = (SExtensionFindingData*)data;

    wlr_box    box = {DATA->origin.x + x, DATA->origin.y + y, surface->current.width, surface->current.height};

    if (wlr_box_contains_point(&box, DATA->vec.x, DATA->vec.y))
        *DATA->found = surface;
}

// checks if the wayland window has a popup at pos
bool CWindow::hasPopupAt(const Vector2D& pos) {
    if (m_bIsX11)
        return false;

    wlr_surface*          resultSurf = nullptr;
    Vector2D              origin     = m_vRealPosition.vec();
    SExtensionFindingData data       = {origin, pos, &resultSurf};
    wlr_xdg_surface_for_each_popup_surface(m_uSurface.xdg, findExtensionForVector2D, &data);

    return resultSurf;
}

CWindow* CWindow::getGroupHead() {
    CWindow* curr = this;
    while (!curr->m_sGroupData.head)
        curr = curr->m_sGroupData.pNextWindow;
    return curr;
}

CWindow* CWindow::getGroupTail() {
    CWindow* curr = this;
    while (!curr->m_sGroupData.pNextWindow->m_sGroupData.head)
        curr = curr->m_sGroupData.pNextWindow;
    return curr;
}

CWindow* CWindow::getGroupCurrent() {
    CWindow* curr = this;
    while (curr->isHidden())
        curr = curr->m_sGroupData.pNextWindow;
    return curr;
}

void CWindow::setGroupCurrent(CWindow* pWindow) {
    CWindow* curr     = this->m_sGroupData.pNextWindow;
    bool     isMember = false;
    while (curr != this) {
        if (curr == pWindow) {
            isMember = true;
            break;
        }
        curr = curr->m_sGroupData.pNextWindow;
    }

    if (!isMember && pWindow != this)
        return;

    const auto PCURRENT   = getGroupCurrent();
    const bool FULLSCREEN = PCURRENT->m_bIsFullscreen;
    const auto WORKSPACE  = g_pCompositor->getWorkspaceByID(PCURRENT->m_iWorkspaceID);

    const auto PWINDOWSIZE = PCURRENT->m_vRealSize.goalv();
    const auto PWINDOWPOS  = PCURRENT->m_vRealPosition.goalv();

    const auto CURRENTISFOCUS = PCURRENT == g_pCompositor->m_pLastWindow;

    if (FULLSCREEN)
        g_pCompositor->setWindowFullscreen(PCURRENT, false, WORKSPACE->m_efFullscreenMode);

    PCURRENT->setHidden(true);
    pWindow->setHidden(false);

    g_pLayoutManager->getCurrentLayout()->replaceWindowDataWith(PCURRENT, pWindow);

    if (PCURRENT->m_bIsFloating) {
        pWindow->m_vRealPosition.setValueAndWarp(PWINDOWPOS);
        pWindow->m_vRealSize.setValueAndWarp(PWINDOWSIZE);
    }

    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    if (CURRENTISFOCUS)
        g_pCompositor->focusWindow(pWindow);

    if (FULLSCREEN)
        g_pCompositor->setWindowFullscreen(pWindow, true, WORKSPACE->m_efFullscreenMode);
}

void CWindow::insertWindowToGroup(CWindow* pWindow) {
    const auto PHEAD = getGroupHead();
    const auto PTAIL = getGroupTail();

    if (pWindow->m_sGroupData.pNextWindow) {
        std::vector<CWindow*> members;
        CWindow*              curr = pWindow;
        do {
            const auto PLAST = curr;
            members.push_back(curr);
            curr                            = curr->m_sGroupData.pNextWindow;
            PLAST->m_sGroupData.pNextWindow = nullptr;
            PLAST->m_sGroupData.head        = false;
        } while (curr != pWindow);

        for (auto& w : members) {
            insertWindowToGroup(w);
        }

        return;
    }

    PTAIL->m_sGroupData.pNextWindow   = pWindow;
    pWindow->m_sGroupData.pNextWindow = PHEAD;

    setGroupCurrent(pWindow);
}

void CWindow::updateGroupOutputs() {
    if (!m_sGroupData.pNextWindow)
        return;

    CWindow* curr = m_sGroupData.pNextWindow;

    while (curr != this) {
        curr->m_iMonitorID = m_iMonitorID;
        curr->moveToWorkspace(m_iWorkspaceID);

        curr->m_vRealPosition = m_vRealPosition.goalv();
        curr->m_vRealSize     = m_vRealSize.goalv();

        curr = curr->m_sGroupData.pNextWindow;
    }
}