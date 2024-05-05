#include "Window.hpp"
#include "../Compositor.hpp"
#include "../render/decorations/CHyprDropShadowDecoration.hpp"
#include "../render/decorations/CHyprGroupBarDecoration.hpp"
#include "../render/decorations/CHyprBorderDecoration.hpp"
#include "../config/ConfigValue.hpp"
#include <any>
#include "../managers/TokenManager.hpp"

PHLWINDOW CWindow::create() {
    PHLWINDOW pWindow = SP<CWindow>(new CWindow);

    pWindow->m_pSelf = pWindow;

    pWindow->m_vRealPosition.create(g_pConfigManager->getAnimationPropertyConfig("windowsIn"), pWindow, AVARDAMAGE_ENTIRE);
    pWindow->m_vRealSize.create(g_pConfigManager->getAnimationPropertyConfig("windowsIn"), pWindow, AVARDAMAGE_ENTIRE);
    pWindow->m_fBorderFadeAnimationProgress.create(g_pConfigManager->getAnimationPropertyConfig("border"), pWindow, AVARDAMAGE_BORDER);
    pWindow->m_fBorderAngleAnimationProgress.create(g_pConfigManager->getAnimationPropertyConfig("borderangle"), pWindow, AVARDAMAGE_BORDER);
    pWindow->m_fAlpha.create(g_pConfigManager->getAnimationPropertyConfig("fadeIn"), pWindow, AVARDAMAGE_ENTIRE);
    pWindow->m_fActiveInactiveAlpha.create(g_pConfigManager->getAnimationPropertyConfig("fadeSwitch"), pWindow, AVARDAMAGE_ENTIRE);
    pWindow->m_cRealShadowColor.create(g_pConfigManager->getAnimationPropertyConfig("fadeShadow"), pWindow, AVARDAMAGE_SHADOW);
    pWindow->m_fDimPercent.create(g_pConfigManager->getAnimationPropertyConfig("fadeDim"), pWindow, AVARDAMAGE_ENTIRE);

    pWindow->addWindowDeco(std::make_unique<CHyprDropShadowDecoration>(pWindow));
    pWindow->addWindowDeco(std::make_unique<CHyprBorderDecoration>(pWindow));

    return pWindow;
}

CWindow::CWindow() {
    ;
}

CWindow::~CWindow() {
    if (g_pCompositor->m_pLastWindow.lock().get() == this) {
        g_pCompositor->m_pLastFocus = nullptr;
        g_pCompositor->m_pLastWindow.reset();
    }

    events.destroy.emit();

    if (!g_pHyprOpenGL)
        return;

    g_pHyprRenderer->makeEGLCurrent();
    std::erase_if(g_pHyprOpenGL->m_mWindowFramebuffers, [&](const auto& other) { return !other.first.lock() || other.first.lock().get() == this; });
}

SWindowDecorationExtents CWindow::getFullWindowExtents() {
    if (m_bFadingOut)
        return m_eOriginalClosedExtents;

    const int BORDERSIZE = getRealBorderSize();

    if (m_sAdditionalConfigData.dimAround) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(m_iMonitorID);
        return {{m_vRealPosition.value().x - PMONITOR->vecPosition.x, m_vRealPosition.value().y - PMONITOR->vecPosition.y},
                {PMONITOR->vecSize.x - (m_vRealPosition.value().x - PMONITOR->vecPosition.x), PMONITOR->vecSize.y - (m_vRealPosition.value().y - PMONITOR->vecPosition.y)}};
    }

    SWindowDecorationExtents maxExtents = {{BORDERSIZE + 2, BORDERSIZE + 2}, {BORDERSIZE + 2, BORDERSIZE + 2}};

    const auto               EXTENTS = g_pDecorationPositioner->getWindowDecorationExtents(m_pSelf.lock());

    if (EXTENTS.topLeft.x > maxExtents.topLeft.x)
        maxExtents.topLeft.x = EXTENTS.topLeft.x;

    if (EXTENTS.topLeft.y > maxExtents.topLeft.y)
        maxExtents.topLeft.y = EXTENTS.topLeft.y;

    if (EXTENTS.bottomRight.x > maxExtents.bottomRight.x)
        maxExtents.bottomRight.x = EXTENTS.bottomRight.x;

    if (EXTENTS.bottomRight.y > maxExtents.bottomRight.y)
        maxExtents.bottomRight.y = EXTENTS.bottomRight.y;

    if (m_pWLSurface.exists() && !m_bIsX11) {
        CBox surfaceExtents = {0, 0, 0, 0};
        // TODO: this could be better, perhaps make a getFullWindowRegion?
        wlr_xdg_surface_for_each_popup_surface(
            m_uSurface.xdg,
            [](wlr_surface* surf, int sx, int sy, void* data) {
                CBox* pSurfaceExtents = (CBox*)data;
                if (sx < pSurfaceExtents->x)
                    pSurfaceExtents->x = sx;
                if (sy < pSurfaceExtents->y)
                    pSurfaceExtents->y = sy;
                if (sx + surf->current.width > pSurfaceExtents->width)
                    pSurfaceExtents->width = sx + surf->current.width - pSurfaceExtents->x;
                if (sy + surf->current.height > pSurfaceExtents->height)
                    pSurfaceExtents->height = sy + surf->current.height - pSurfaceExtents->y;
            },
            &surfaceExtents);

        if (-surfaceExtents.x > maxExtents.topLeft.x)
            maxExtents.topLeft.x = -surfaceExtents.x;

        if (-surfaceExtents.y > maxExtents.topLeft.y)
            maxExtents.topLeft.y = -surfaceExtents.y;

        if (surfaceExtents.x + surfaceExtents.width > m_pWLSurface.wlr()->current.width + maxExtents.bottomRight.x)
            maxExtents.bottomRight.x = surfaceExtents.x + surfaceExtents.width - m_pWLSurface.wlr()->current.width;

        if (surfaceExtents.y + surfaceExtents.height > m_pWLSurface.wlr()->current.height + maxExtents.bottomRight.y)
            maxExtents.bottomRight.y = surfaceExtents.y + surfaceExtents.height - m_pWLSurface.wlr()->current.height;
    }

    return maxExtents;
}

CBox CWindow::getFullWindowBoundingBox() {
    if (m_sAdditionalConfigData.dimAround) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(m_iMonitorID);
        return {PMONITOR->vecPosition.x, PMONITOR->vecPosition.y, PMONITOR->vecSize.x, PMONITOR->vecSize.y};
    }

    auto maxExtents = getFullWindowExtents();

    CBox finalBox = {m_vRealPosition.value().x - maxExtents.topLeft.x, m_vRealPosition.value().y - maxExtents.topLeft.y,
                     m_vRealSize.value().x + maxExtents.topLeft.x + maxExtents.bottomRight.x, m_vRealSize.value().y + maxExtents.topLeft.y + maxExtents.bottomRight.y};

    return finalBox;
}

CBox CWindow::getWindowIdealBoundingBoxIgnoreReserved() {

    const auto PMONITOR = g_pCompositor->getMonitorFromID(m_iMonitorID);

    auto       POS  = m_vPosition;
    auto       SIZE = m_vSize;

    if (m_bIsFullscreen) {
        POS  = PMONITOR->vecPosition;
        SIZE = PMONITOR->vecSize;

        return CBox{(int)POS.x, (int)POS.y, (int)SIZE.x, (int)SIZE.y};
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

    return CBox{(int)POS.x, (int)POS.y, (int)SIZE.x, (int)SIZE.y};
}

CBox CWindow::getWindowBoxUnified(uint64_t properties) {

    if (m_sAdditionalConfigData.dimAround) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(m_iMonitorID);
        return {PMONITOR->vecPosition.x, PMONITOR->vecPosition.y, PMONITOR->vecSize.x, PMONITOR->vecSize.y};
    }

    SWindowDecorationExtents EXTENTS = {{0, 0}, {0, 0}};
    if (properties & RESERVED_EXTENTS)
        EXTENTS.addExtents(g_pDecorationPositioner->getWindowDecorationReserved(m_pSelf.lock()));
    if (properties & INPUT_EXTENTS)
        EXTENTS.addExtents(g_pDecorationPositioner->getWindowDecorationExtents(m_pSelf.lock(), true));
    if (properties & FULL_EXTENTS)
        EXTENTS.addExtents(g_pDecorationPositioner->getWindowDecorationExtents(m_pSelf.lock(), false));

    CBox box = {m_vRealPosition.value().x, m_vRealPosition.value().y, m_vRealSize.value().x, m_vRealSize.value().y};
    box.addExtents(EXTENTS);

    return box;
}

CBox CWindow::getWindowMainSurfaceBox() {
    return {m_vRealPosition.value().x, m_vRealPosition.value().y, m_vRealSize.value().x, m_vRealSize.value().y};
}

SWindowDecorationExtents CWindow::getFullWindowReservedArea() {
    return g_pDecorationPositioner->getWindowDecorationReserved(m_pSelf.lock());
}

void CWindow::updateWindowDecos() {

    if (!m_bIsMapped || isHidden())
        return;

    for (auto& wd : m_vDecosToRemove) {
        for (auto it = m_dWindowDecorations.begin(); it != m_dWindowDecorations.end(); it++) {
            if (it->get() == wd) {
                g_pDecorationPositioner->uncacheDecoration(it->get());
                it = m_dWindowDecorations.erase(it);
                if (it == m_dWindowDecorations.end())
                    break;
            }
        }
    }

    g_pDecorationPositioner->onWindowUpdate(m_pSelf.lock());

    m_vDecosToRemove.clear();

    // make a copy because updateWindow can remove decos.
    std::vector<IHyprWindowDecoration*> decos;

    for (auto& wd : m_dWindowDecorations) {
        decos.push_back(wd.get());
    }

    for (auto& wd : decos) {
        if (std::find_if(m_dWindowDecorations.begin(), m_dWindowDecorations.end(), [wd](const auto& other) { return other.get() == wd; }) == m_dWindowDecorations.end())
            continue;
        wd->updateWindow(m_pSelf.lock());
    }
}

void CWindow::addWindowDeco(std::unique_ptr<IHyprWindowDecoration> deco) {
    m_dWindowDecorations.emplace_back(std::move(deco));
    g_pDecorationPositioner->forceRecalcFor(m_pSelf.lock());
    updateWindowDecos();
    g_pLayoutManager->getCurrentLayout()->recalculateWindow(m_pSelf.lock());
}

void CWindow::removeWindowDeco(IHyprWindowDecoration* deco) {
    m_vDecosToRemove.push_back(deco);
    g_pDecorationPositioner->forceRecalcFor(m_pSelf.lock());
    updateWindowDecos();
    g_pLayoutManager->getCurrentLayout()->recalculateWindow(m_pSelf.lock());
}

void CWindow::uncacheWindowDecos() {
    for (auto& wd : m_dWindowDecorations) {
        g_pDecorationPositioner->uncacheDecoration(wd.get());
    }
}

bool CWindow::checkInputOnDecos(const eInputType type, const Vector2D& mouseCoords, std::any data) {
    if (type != INPUT_TYPE_DRAG_END && hasPopupAt(mouseCoords))
        return false;

    for (auto& wd : m_dWindowDecorations) {
        if (!(wd->getDecorationFlags() & DECORATION_ALLOWS_MOUSE_INPUT))
            continue;

        if (!g_pDecorationPositioner->getWindowDecorationBox(wd.get()).containsPoint(mouseCoords))
            continue;

        if (wd->onInputOnDeco(type, mouseCoords, data))
            return true;
    }

    return false;
}

pid_t CWindow::getPID() {
    pid_t PID = -1;
    if (!m_bIsX11) {
        if (!m_uSurface.xdg)
            return -1;

        wl_client_get_credentials(wl_resource_get_client(m_uSurface.xdg->resource), &PID, nullptr, nullptr);
    } else {
        if (!m_uSurface.xwayland)
            return -1;

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

void CWindow::updateToplevel() {
    updateSurfaceScaleTransformDetails();
}

void sendEnterIter(wlr_surface* pSurface, int x, int y, void* data) {
    const auto OUTPUT = (wlr_output*)data;
    wlr_surface_send_enter(pSurface, OUTPUT);
}

void sendLeaveIter(wlr_surface* pSurface, int x, int y, void* data) {
    const auto OUTPUT = (wlr_output*)data;
    wlr_surface_send_leave(pSurface, OUTPUT);
}

void CWindow::updateSurfaceScaleTransformDetails() {
    if (!m_bIsMapped || m_bHidden || g_pCompositor->m_bUnsafeState)
        return;

    const auto PLASTMONITOR = g_pCompositor->getMonitorFromID(m_iLastSurfaceMonitorID);

    m_iLastSurfaceMonitorID = m_iMonitorID;

    const auto PNEWMONITOR = g_pCompositor->getMonitorFromID(m_iMonitorID);

    if (!PNEWMONITOR)
        return;

    if (PNEWMONITOR != PLASTMONITOR) {
        if (PLASTMONITOR && PLASTMONITOR->m_bEnabled)
            wlr_surface_for_each_surface(m_pWLSurface.wlr(), sendLeaveIter, PLASTMONITOR->output);

        wlr_surface_for_each_surface(m_pWLSurface.wlr(), sendEnterIter, PNEWMONITOR->output);
    }

    wlr_surface_for_each_surface(
        m_pWLSurface.wlr(),
        [](wlr_surface* surf, int x, int y, void* data) {
            const auto PMONITOR = g_pCompositor->getMonitorFromID(((CWindow*)data)->m_iMonitorID);

            const auto PSURFACE = CWLSurface::surfaceFromWlr(surf);
            if (PSURFACE && PSURFACE->m_fLastScale == PMONITOR->scale)
                return;

            g_pCompositor->setPreferredScaleForSurface(surf, PMONITOR->scale);
            g_pCompositor->setPreferredTransformForSurface(surf, PMONITOR->transform);
        },
        this);
}

void CWindow::moveToWorkspace(PHLWORKSPACE pWorkspace) {
    if (m_pWorkspace == pWorkspace)
        return;

    static auto PINITIALWSTRACKING = CConfigValue<Hyprlang::INT>("misc:initial_workspace_tracking");

    if (!m_szInitialWorkspaceToken.empty()) {
        const auto TOKEN = g_pTokenManager->getToken(m_szInitialWorkspaceToken);
        if (TOKEN) {
            if (*PINITIALWSTRACKING == 2) {
                // persistent
                SInitialWorkspaceToken token = std::any_cast<SInitialWorkspaceToken>(TOKEN->data);
                if (token.primaryOwner.lock().get() == this) {
                    token.workspace = pWorkspace->getConfigName();
                    TOKEN->data     = token;
                }
            }
        }
    }

    static auto PCLOSEONLASTSPECIAL = CConfigValue<Hyprlang::INT>("misc:close_special_on_empty");

    const auto  OLDWORKSPACE = m_pWorkspace;

    m_pWorkspace = pWorkspace;

    setAnimationsToMove();

    g_pCompositor->updateWorkspaceWindows(OLDWORKSPACE->m_iID);
    g_pCompositor->updateWorkspaceSpecialRenderData(OLDWORKSPACE->m_iID);
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(OLDWORKSPACE->m_iMonitorID);

    g_pCompositor->updateWorkspaceWindows(workspaceID());
    g_pCompositor->updateWorkspaceSpecialRenderData(workspaceID());
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m_iMonitorID);

    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    if (valid(pWorkspace)) {
        g_pEventManager->postEvent(SHyprIPCEvent{"movewindow", std::format("{:x},{}", (uintptr_t)this, pWorkspace->m_szName)});
        g_pEventManager->postEvent(SHyprIPCEvent{"movewindowv2", std::format("{:x},{},{}", (uintptr_t)this, pWorkspace->m_iID, pWorkspace->m_szName)});
        EMIT_HOOK_EVENT("moveWindow", (std::vector<std::any>{m_pSelf.lock(), pWorkspace}));
    }

    if (const auto SWALLOWED = m_pSwallowed.lock()) {
        SWALLOWED->moveToWorkspace(pWorkspace);
        SWALLOWED->m_iMonitorID = m_iMonitorID;
    }

    // update xwayland coords
    g_pXWaylandManager->setWindowSize(m_pSelf.lock(), m_vRealSize.value());

    if (OLDWORKSPACE && g_pCompositor->isWorkspaceSpecial(OLDWORKSPACE->m_iID) && g_pCompositor->getWindowsOnWorkspace(OLDWORKSPACE->m_iID) == 0 && *PCLOSEONLASTSPECIAL) {
        if (const auto PMONITOR = g_pCompositor->getMonitorFromID(OLDWORKSPACE->m_iMonitorID); PMONITOR)
            PMONITOR->setSpecialWorkspace(nullptr);
    }
}

PHLWINDOW CWindow::X11TransientFor() {
    if (!m_bIsX11)
        return nullptr;

    if (!m_uSurface.xwayland->parent)
        return nullptr;

    auto PPARENT = g_pCompositor->getWindowFromSurface(m_uSurface.xwayland->parent->surface);

    while (validMapped(PPARENT) && PPARENT->m_uSurface.xwayland->parent) {
        PPARENT = g_pCompositor->getWindowFromSurface(PPARENT->m_uSurface.xwayland->parent->surface);
    }

    if (!validMapped(PPARENT))
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
    ((CBaseAnimatedVariable*)ptr)->unregister();
}

void CWindow::onUnmap() {
    static auto PCLOSEONLASTSPECIAL = CConfigValue<Hyprlang::INT>("misc:close_special_on_empty");

    if (g_pCompositor->m_pLastWindow.lock().get() == this)
        g_pCompositor->m_pLastWindow.reset();
    if (g_pInputManager->currentlyDraggedWindow.lock().get() == this)
        g_pInputManager->currentlyDraggedWindow.reset();

    static auto PINITIALWSTRACKING = CConfigValue<Hyprlang::INT>("misc:initial_workspace_tracking");

    if (!m_szInitialWorkspaceToken.empty()) {
        const auto TOKEN = g_pTokenManager->getToken(m_szInitialWorkspaceToken);
        if (TOKEN) {
            if (*PINITIALWSTRACKING == 2) {
                // persistent token, but the first window got removed so the token is gone
                SInitialWorkspaceToken token = std::any_cast<SInitialWorkspaceToken>(TOKEN->data);
                if (token.primaryOwner.lock().get() == this)
                    g_pTokenManager->removeToken(TOKEN);
            }
        }
    }

    m_iLastWorkspace = m_pWorkspace->m_iID;

    m_vRealPosition.setCallbackOnEnd(unregisterVar);
    m_vRealSize.setCallbackOnEnd(unregisterVar);
    m_fBorderFadeAnimationProgress.setCallbackOnEnd(unregisterVar);
    m_fBorderAngleAnimationProgress.setCallbackOnEnd(unregisterVar);
    m_fActiveInactiveAlpha.setCallbackOnEnd(unregisterVar);
    m_fAlpha.setCallbackOnEnd(unregisterVar);
    m_cRealShadowColor.setCallbackOnEnd(unregisterVar);
    m_fDimPercent.setCallbackOnEnd(unregisterVar);

    m_vRealSize.setCallbackOnBegin(nullptr);

    std::erase_if(g_pCompositor->m_vWindowFocusHistory, [&](const auto& other) { return other.expired() || other.lock().get() == this; });

    hyprListener_unmapWindow.removeCallback();

    if (*PCLOSEONLASTSPECIAL && g_pCompositor->getWindowsOnWorkspace(workspaceID()) == 0 && onSpecialWorkspace()) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(m_iMonitorID);
        if (PMONITOR && PMONITOR->activeSpecialWorkspace && PMONITOR->activeSpecialWorkspace == m_pWorkspace)
            PMONITOR->setSpecialWorkspace(nullptr);
    }

    const auto PMONITOR = g_pCompositor->getMonitorFromID(m_iMonitorID);

    if (PMONITOR && PMONITOR->solitaryClient.lock().get() == this)
        PMONITOR->solitaryClient.reset();

    g_pCompositor->updateWorkspaceWindows(workspaceID());
    g_pCompositor->updateWorkspaceSpecialRenderData(workspaceID());
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m_iMonitorID);
    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    m_pWorkspace.reset();

    if (m_bIsX11)
        return;

    m_pSubsurfaceHead.reset();
    m_pPopupHead.reset();
}

void CWindow::onMap() {
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

    g_pCompositor->m_vWindowFocusHistory.push_back(m_pSelf);

    hyprListener_unmapWindow.initCallback(m_bIsX11 ? &m_uSurface.xwayland->surface->events.unmap : &m_uSurface.xdg->surface->events.unmap, &Events::listener_unmapWindow, this,
                                          "CWindow");

    m_vReportedSize = m_vPendingReportedSize;
    m_bAnimatingIn  = true;

    if (m_bIsX11)
        return;

    m_pSubsurfaceHead = std::make_unique<CSubsurface>(m_pSelf.lock());
    m_pPopupHead      = std::make_unique<CPopup>(m_pSelf.lock());
}

void CWindow::onBorderAngleAnimEnd(void* ptr) {
    const auto        PANIMVAR = (CAnimatedVariable<float>*)ptr;

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

    if (hidden && g_pCompositor->m_pLastWindow.lock().get() == this) {
        g_pCompositor->m_pLastWindow.reset();
    }

    setSuspended(hidden);
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
    } else if (r.szRule == "nodim") {
        m_sAdditionalConfigData.forceNoDim = true;
    } else if (r.szRule == "forcergbx") {
        m_sAdditionalConfigData.forceRGBX = true;
    } else if (r.szRule == "opaque") {
        if (!m_sAdditionalConfigData.forceOpaqueOverridden)
            m_sAdditionalConfigData.forceOpaque = true;
    } else if (r.szRule == "immediate") {
        m_sAdditionalConfigData.forceTearing = true;
    } else if (r.szRule == "nearestneighbor") {
        m_sAdditionalConfigData.nearestNeighbor = true;
    } else if (r.szRule.starts_with("rounding")) {
        try {
            m_sAdditionalConfigData.rounding = std::stoi(r.szRule.substr(r.szRule.find_first_of(' ') + 1));
        } catch (std::exception& e) { Debug::log(ERR, "Rounding rule \"{}\" failed with: {}", r.szRule, e.what()); }
    } else if (r.szRule.starts_with("bordersize")) {
        try {
            m_sAdditionalConfigData.borderSize = std::stoi(r.szRule.substr(r.szRule.find_first_of(' ') + 1));
        } catch (std::exception& e) { Debug::log(ERR, "Bordersize rule \"{}\" failed with: {}", r.szRule, e.what()); }
    } else if (r.szRule.starts_with("opacity")) {
        try {
            CVarList vars(r.szRule, 0, ' ');

            int      opacityIDX = 0;

            for (auto& r : vars) {
                if (r == "opacity")
                    continue;

                if (r == "override") {
                    if (opacityIDX == 1)
                        m_sSpecialRenderData.alphaOverride = true;
                    else if (opacityIDX == 2)
                        m_sSpecialRenderData.alphaInactiveOverride = true;
                    else if (opacityIDX == 3)
                        m_sSpecialRenderData.alphaFullscreenOverride = true;
                } else {
                    if (opacityIDX == 0) {
                        m_sSpecialRenderData.alpha         = std::stof(r);
                        m_sSpecialRenderData.alphaOverride = false;
                    } else if (opacityIDX == 1) {
                        m_sSpecialRenderData.alphaInactive         = std::stof(r);
                        m_sSpecialRenderData.alphaInactiveOverride = false;
                    } else if (opacityIDX == 2) {
                        m_sSpecialRenderData.alphaFullscreen         = std::stof(r);
                        m_sSpecialRenderData.alphaFullscreenOverride = false;
                    } else {
                        throw std::runtime_error("more than 3 alpha values");
                    }

                    opacityIDX++;
                }
            }

            if (opacityIDX == 1) {
                m_sSpecialRenderData.alphaInactiveOverride   = m_sSpecialRenderData.alphaOverride;
                m_sSpecialRenderData.alphaInactive           = m_sSpecialRenderData.alpha;
                m_sSpecialRenderData.alphaFullscreenOverride = m_sSpecialRenderData.alphaOverride;
                m_sSpecialRenderData.alphaFullscreen         = m_sSpecialRenderData.alpha;
            }
        } catch (std::exception& e) { Debug::log(ERR, "Opacity rule \"{}\" failed with: {}", r.szRule, e.what()); }
    } else if (r.szRule == "noanim") {
        m_sAdditionalConfigData.forceNoAnims = true;
    } else if (r.szRule.starts_with("animation")) {
        auto STYLE                             = r.szRule.substr(r.szRule.find_first_of(' ') + 1);
        m_sAdditionalConfigData.animationStyle = STYLE;
    } else if (r.szRule.starts_with("bordercolor")) {
        try {
            // Each vector will only get used if it has at least one color
            CGradientValueData activeBorderGradient   = {};
            CGradientValueData inactiveBorderGradient = {};
            bool               active                 = true;
            CVarList           colorsAndAngles        = CVarList(removeBeginEndSpacesTabs(r.szRule.substr(r.szRule.find_first_of(' ') + 1)), 0, 's', true);

            // Basic form has only two colors, everything else can be parsed as a gradient
            if (colorsAndAngles.size() == 2 && !colorsAndAngles[1].contains("deg")) {
                m_sSpecialRenderData.activeBorderColor   = CGradientValueData(CColor(configStringToInt(colorsAndAngles[0])));
                m_sSpecialRenderData.inactiveBorderColor = CGradientValueData(CColor(configStringToInt(colorsAndAngles[1])));
                return;
            }

            for (auto& token : colorsAndAngles) {
                // The first angle, or an explicit "0deg", splits the two gradients
                if (active && token.contains("deg")) {
                    activeBorderGradient.m_fAngle = std::stoi(token.substr(0, token.size() - 3)) * (PI / 180.0);
                    active                        = false;
                } else if (token.contains("deg"))
                    inactiveBorderGradient.m_fAngle = std::stoi(token.substr(0, token.size() - 3)) * (PI / 180.0);
                else if (active)
                    activeBorderGradient.m_vColors.push_back(configStringToInt(token));
                else
                    inactiveBorderGradient.m_vColors.push_back(configStringToInt(token));
            }

            // Includes sanity checks for the number of colors in each gradient
            if (activeBorderGradient.m_vColors.size() > 10 || inactiveBorderGradient.m_vColors.size() > 10)
                Debug::log(WARN, "Bordercolor rule \"{}\" has more than 10 colors in one gradient, ignoring", r.szRule);
            else if (activeBorderGradient.m_vColors.empty())
                Debug::log(WARN, "Bordercolor rule \"{}\" has no colors, ignoring", r.szRule);
            else if (inactiveBorderGradient.m_vColors.empty())
                m_sSpecialRenderData.activeBorderColor = activeBorderGradient;
            else {
                m_sSpecialRenderData.activeBorderColor   = activeBorderGradient;
                m_sSpecialRenderData.inactiveBorderColor = inactiveBorderGradient;
            }
        } catch (std::exception& e) { Debug::log(ERR, "BorderColor rule \"{}\" failed with: {}", r.szRule, e.what()); }
    } else if (r.szRule == "dimaround") {
        m_sAdditionalConfigData.dimAround = true;
    } else if (r.szRule == "keepaspectratio") {
        m_sAdditionalConfigData.keepAspectRatio = true;
    } else if (r.szRule.starts_with("xray")) {
        CVarList vars(r.szRule, 0, ' ');

        try {
            m_sAdditionalConfigData.xray = configStringToInt(vars[1]);
        } catch (...) {}
    } else if (r.szRule.starts_with("idleinhibit")) {
        auto IDLERULE = r.szRule.substr(r.szRule.find_first_of(' ') + 1);

        if (IDLERULE == "none")
            m_eIdleInhibitMode = IDLEINHIBIT_NONE;
        else if (IDLERULE == "always")
            m_eIdleInhibitMode = IDLEINHIBIT_ALWAYS;
        else if (IDLERULE == "focus")
            m_eIdleInhibitMode = IDLEINHIBIT_FOCUS;
        else if (IDLERULE == "fullscreen")
            m_eIdleInhibitMode = IDLEINHIBIT_FULLSCREEN;
        else
            Debug::log(ERR, "Rule idleinhibit: unknown mode {}", IDLERULE);
    } else if (r.szRule.starts_with("maxsize")) {
        try {
            if (!m_bIsFloating)
                return;
            const auto VEC = configStringToVector2D(r.szRule.substr(8));
            if (VEC.x < 1 || VEC.y < 1) {
                Debug::log(ERR, "Invalid size for maxsize");
                return;
            }

            m_sAdditionalConfigData.maxSize = VEC;
            m_vRealSize                     = Vector2D(std::min((double)m_sAdditionalConfigData.maxSize.toUnderlying().x, m_vRealSize.goal().x),
                                                       std::min((double)m_sAdditionalConfigData.maxSize.toUnderlying().y, m_vRealSize.goal().y));
            g_pXWaylandManager->setWindowSize(m_pSelf.lock(), m_vRealSize.goal());
        } catch (std::exception& e) { Debug::log(ERR, "maxsize rule \"{}\" failed with: {}", r.szRule, e.what()); }
    } else if (r.szRule.starts_with("minsize")) {
        try {
            if (!m_bIsFloating)
                return;
            const auto VEC = configStringToVector2D(r.szRule.substr(8));
            if (VEC.x < 1 || VEC.y < 1) {
                Debug::log(ERR, "Invalid size for minsize");
                return;
            }

            m_sAdditionalConfigData.minSize = VEC;
            m_vRealSize                     = Vector2D(std::max((double)m_sAdditionalConfigData.minSize.toUnderlying().x, m_vRealSize.goal().x),
                                                       std::max((double)m_sAdditionalConfigData.minSize.toUnderlying().y, m_vRealSize.goal().y));
            g_pXWaylandManager->setWindowSize(m_pSelf.lock(), m_vRealSize.goal());
            if (m_sGroupData.pNextWindow.expired())
                setHidden(false);
        } catch (std::exception& e) { Debug::log(ERR, "minsize rule \"{}\" failed with: {}", r.szRule, e.what()); }
    }
}

void CWindow::updateDynamicRules() {
    m_sSpecialRenderData.activeBorderColor   = CGradientValueData();
    m_sSpecialRenderData.inactiveBorderColor = CGradientValueData();
    m_sSpecialRenderData.alpha               = 1.f;
    m_sSpecialRenderData.alphaInactive       = -1.f;
    m_sAdditionalConfigData.forceNoBlur      = false;
    m_sAdditionalConfigData.forceNoBorder    = false;
    m_sAdditionalConfigData.forceNoShadow    = false;
    m_sAdditionalConfigData.forceNoDim       = false;
    if (!m_sAdditionalConfigData.forceOpaqueOverridden)
        m_sAdditionalConfigData.forceOpaque = false;
    m_sAdditionalConfigData.maxSize         = Vector2D(std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
    m_sAdditionalConfigData.minSize         = Vector2D(20, 20);
    m_sAdditionalConfigData.forceNoAnims    = false;
    m_sAdditionalConfigData.animationStyle  = std::string("");
    m_sAdditionalConfigData.rounding        = -1;
    m_sAdditionalConfigData.dimAround       = false;
    m_sAdditionalConfigData.forceRGBX       = false;
    m_sAdditionalConfigData.borderSize      = -1;
    m_sAdditionalConfigData.keepAspectRatio = false;
    m_sAdditionalConfigData.xray            = -1;
    m_sAdditionalConfigData.forceTearing    = false;
    m_sAdditionalConfigData.nearestNeighbor = false;
    m_eIdleInhibitMode                      = IDLEINHIBIT_NONE;

    m_vMatchedRules = g_pConfigManager->getMatchingRules(m_pSelf.lock());
    for (auto& r : m_vMatchedRules) {
        applyDynamicRule(r);
    }

    EMIT_HOOK_EVENT("windowUpdateRules", m_pSelf.lock());

    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m_iMonitorID);
}

// check if the point is "hidden" under a rounded corner of the window
// it is assumed that the point is within the real window box (m_vRealPosition, m_vRealSize)
// otherwise behaviour is undefined
bool CWindow::isInCurvedCorner(double x, double y) {
    const int ROUNDING = rounding();
    if (getRealBorderSize() >= ROUNDING)
        return false;

    // (x0, y0), (x0, y1), ... are the center point of rounding at each corner
    double x0 = m_vRealPosition.value().x + ROUNDING;
    double y0 = m_vRealPosition.value().y + ROUNDING;
    double x1 = m_vRealPosition.value().x + m_vRealSize.value().x - ROUNDING;
    double y1 = m_vRealPosition.value().y + m_vRealSize.value().y - ROUNDING;

    if (x < x0 && y < y0) {
        return Vector2D{x0, y0}.distance(Vector2D{x, y}) > (double)ROUNDING;
    }
    if (x > x1 && y < y0) {
        return Vector2D{x1, y0}.distance(Vector2D{x, y}) > (double)ROUNDING;
    }
    if (x < x0 && y > y1) {
        return Vector2D{x0, y1}.distance(Vector2D{x, y}) > (double)ROUNDING;
    }
    if (x > x1 && y > y1) {
        return Vector2D{x1, y1}.distance(Vector2D{x, y}) > (double)ROUNDING;
    }

    return false;
}

void findExtensionForVector2D(wlr_surface* surface, int x, int y, void* data) {
    const auto DATA = (SExtensionFindingData*)data;

    CBox       box = {DATA->origin.x + x, DATA->origin.y + y, surface->current.width, surface->current.height};

    if (box.containsPoint(DATA->vec))
        *DATA->found = surface;
}

// checks if the wayland window has a popup at pos
bool CWindow::hasPopupAt(const Vector2D& pos) {
    if (m_bIsX11)
        return false;

    wlr_surface*          resultSurf = nullptr;
    Vector2D              origin     = m_vRealPosition.value();
    SExtensionFindingData data       = {origin, pos, &resultSurf};
    wlr_xdg_surface_for_each_popup_surface(m_uSurface.xdg, findExtensionForVector2D, &data);

    return resultSurf;
}

void CWindow::applyGroupRules() {
    if ((m_eGroupRules & GROUP_SET && m_bFirstMap) || m_eGroupRules & GROUP_SET_ALWAYS)
        createGroup();

    if (m_sGroupData.pNextWindow.lock() && ((m_eGroupRules & GROUP_LOCK && m_bFirstMap) || m_eGroupRules & GROUP_LOCK_ALWAYS))
        getGroupHead()->m_sGroupData.locked = true;
}

void CWindow::createGroup() {
    if (m_sGroupData.deny) {
        Debug::log(LOG, "createGroup: window:{:x},title:{} is denied as a group, ignored", (uintptr_t)this, this->m_szTitle);
        return;
    }

    if (m_sGroupData.pNextWindow.expired()) {
        m_sGroupData.pNextWindow = m_pSelf;
        m_sGroupData.head        = true;
        m_sGroupData.locked      = false;
        m_sGroupData.deny        = false;

        addWindowDeco(std::make_unique<CHyprGroupBarDecoration>(m_pSelf.lock()));

        g_pCompositor->updateWorkspaceWindows(workspaceID());
        g_pCompositor->updateWorkspaceSpecialRenderData(workspaceID());
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m_iMonitorID);
        g_pCompositor->updateAllWindowsAnimatedDecorationValues();

        g_pEventManager->postEvent(SHyprIPCEvent{"togglegroup", std::format("1,{:x}", (uintptr_t)this)});
    }
}

void CWindow::destroyGroup() {
    if (m_sGroupData.pNextWindow.lock().get() == this) {
        if (m_eGroupRules & GROUP_SET_ALWAYS) {
            Debug::log(LOG, "destoryGroup: window:{:x},title:{} has rule [group set always], ignored", (uintptr_t)this, this->m_szTitle);
            return;
        }
        m_sGroupData.pNextWindow.reset();
        m_sGroupData.head = false;
        updateWindowDecos();
        g_pCompositor->updateWorkspaceWindows(workspaceID());
        g_pCompositor->updateWorkspaceSpecialRenderData(workspaceID());
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m_iMonitorID);
        g_pCompositor->updateAllWindowsAnimatedDecorationValues();

        g_pEventManager->postEvent(SHyprIPCEvent{"togglegroup", std::format("0,{:x}", (uintptr_t)this)});
        return;
    }

    std::string            addresses;
    PHLWINDOW              curr = m_pSelf.lock();
    std::vector<PHLWINDOW> members;
    do {
        const auto PLASTWIN = curr;
        curr                = curr->m_sGroupData.pNextWindow.lock();
        PLASTWIN->m_sGroupData.pNextWindow.reset();
        curr->setHidden(false);
        members.push_back(curr);

        addresses += std::format("{:x},", (uintptr_t)curr.get());
    } while (curr.get() != this);

    for (auto& w : members) {
        if (w->m_sGroupData.head)
            g_pLayoutManager->getCurrentLayout()->onWindowRemoved(curr);
        w->m_sGroupData.head = false;
    }

    const bool GROUPSLOCKEDPREV        = g_pKeybindManager->m_bGroupsLocked;
    g_pKeybindManager->m_bGroupsLocked = true;
    for (auto& w : members) {
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(w);
        w->updateWindowDecos();
    }
    g_pKeybindManager->m_bGroupsLocked = GROUPSLOCKEDPREV;

    g_pCompositor->updateWorkspaceWindows(workspaceID());
    g_pCompositor->updateWorkspaceSpecialRenderData(workspaceID());
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m_iMonitorID);
    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    if (!addresses.empty())
        addresses.pop_back();
    g_pEventManager->postEvent(SHyprIPCEvent{"togglegroup", std::format("0,{}", addresses)});
}

PHLWINDOW CWindow::getGroupHead() {
    PHLWINDOW curr = m_pSelf.lock();
    while (!curr->m_sGroupData.head)
        curr = curr->m_sGroupData.pNextWindow.lock();
    return curr;
}

PHLWINDOW CWindow::getGroupTail() {
    PHLWINDOW curr = m_pSelf.lock();
    while (!curr->m_sGroupData.pNextWindow->m_sGroupData.head)
        curr = curr->m_sGroupData.pNextWindow.lock();
    return curr;
}

PHLWINDOW CWindow::getGroupCurrent() {
    PHLWINDOW curr = m_pSelf.lock();
    while (curr->isHidden())
        curr = curr->m_sGroupData.pNextWindow.lock();
    return curr;
}

int CWindow::getGroupSize() {
    int       size = 1;
    PHLWINDOW curr = m_pSelf.lock();
    while (curr->m_sGroupData.pNextWindow.lock().get() != this) {
        curr = curr->m_sGroupData.pNextWindow.lock();
        size++;
    }
    return size;
}

bool CWindow::canBeGroupedInto(PHLWINDOW pWindow) {
    return !g_pKeybindManager->m_bGroupsLocked                                                 // global group lock disengaged
        && ((m_eGroupRules & GROUP_INVADE && m_bFirstMap)                                      // window ignore local group locks, or
            || (!pWindow->getGroupHead()->m_sGroupData.locked                                  //      target unlocked
                && !(m_sGroupData.pNextWindow.lock() && getGroupHead()->m_sGroupData.locked))) //      source unlocked or isn't group
        && !m_sGroupData.deny                                                                  // source is not denied entry
        && !(m_eGroupRules & GROUP_BARRED && m_bFirstMap);                                     // group rule doesn't prevent adding window
}

PHLWINDOW CWindow::getGroupWindowByIndex(int index) {
    const int SIZE = getGroupSize();
    index          = ((index % SIZE) + SIZE) % SIZE;
    PHLWINDOW curr = getGroupHead();
    while (index > 0) {
        curr = curr->m_sGroupData.pNextWindow.lock();
        index--;
    }
    return curr;
}

void CWindow::setGroupCurrent(PHLWINDOW pWindow) {
    PHLWINDOW curr     = m_sGroupData.pNextWindow.lock();
    bool      isMember = false;
    while (curr.get() != this) {
        if (curr == pWindow) {
            isMember = true;
            break;
        }
        curr = curr->m_sGroupData.pNextWindow.lock();
    }

    if (!isMember && pWindow.get() != this)
        return;

    const auto PCURRENT   = getGroupCurrent();
    const bool FULLSCREEN = PCURRENT->m_bIsFullscreen;
    const auto WORKSPACE  = PCURRENT->m_pWorkspace;

    const auto PWINDOWSIZE = PCURRENT->m_vRealSize.goal();
    const auto PWINDOWPOS  = PCURRENT->m_vRealPosition.goal();

    const auto CURRENTISFOCUS = PCURRENT == g_pCompositor->m_pLastWindow.lock();

    if (FULLSCREEN)
        g_pCompositor->setWindowFullscreen(PCURRENT, false, WORKSPACE->m_efFullscreenMode);

    PCURRENT->setHidden(true);
    pWindow->setHidden(false); // can remove m_pLastWindow

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

    g_pHyprRenderer->damageWindow(pWindow);

    pWindow->updateWindowDecos();
}

void CWindow::insertWindowToGroup(PHLWINDOW pWindow) {
    const auto BEGINAT = m_pSelf.lock();
    const auto ENDAT   = m_sGroupData.pNextWindow.lock();

    if (!pWindow->getDecorationByType(DECORATION_GROUPBAR))
        pWindow->addWindowDeco(std::make_unique<CHyprGroupBarDecoration>(pWindow));

    if (!pWindow->m_sGroupData.pNextWindow.lock()) {
        BEGINAT->m_sGroupData.pNextWindow = pWindow;
        pWindow->m_sGroupData.pNextWindow = ENDAT;
        pWindow->m_sGroupData.head        = false;
        return;
    }

    const auto SHEAD = pWindow->getGroupHead();
    const auto STAIL = pWindow->getGroupTail();

    SHEAD->m_sGroupData.head          = false;
    BEGINAT->m_sGroupData.pNextWindow = SHEAD;
    STAIL->m_sGroupData.pNextWindow   = ENDAT;
}

PHLWINDOW CWindow::getGroupPrevious() {
    PHLWINDOW curr = m_sGroupData.pNextWindow.lock();

    while (curr != m_pSelf.lock() && curr->m_sGroupData.pNextWindow.lock().get() != this)
        curr = curr->m_sGroupData.pNextWindow.lock();

    return curr;
}

void CWindow::switchWithWindowInGroup(PHLWINDOW pWindow) {
    if (!m_sGroupData.pNextWindow.lock() || !pWindow->m_sGroupData.pNextWindow.lock())
        return;

    if (m_sGroupData.pNextWindow.lock() == pWindow) { // A -> this -> pWindow -> B >> A -> pWindow -> this -> B
        getGroupPrevious()->m_sGroupData.pNextWindow = pWindow;
        m_sGroupData.pNextWindow                     = pWindow->m_sGroupData.pNextWindow;
        pWindow->m_sGroupData.pNextWindow            = m_pSelf;

    } else if (pWindow->m_sGroupData.pNextWindow.lock().get() == this) { // A -> pWindow -> this -> B >> A -> this -> pWindow -> B
        pWindow->getGroupPrevious()->m_sGroupData.pNextWindow = m_pSelf;
        pWindow->m_sGroupData.pNextWindow                     = m_sGroupData.pNextWindow;
        m_sGroupData.pNextWindow                              = pWindow;

    } else { // A -> this -> B | C -> pWindow -> D >> A -> pWindow -> B | C -> this -> D
        std::swap(m_sGroupData.pNextWindow, pWindow->m_sGroupData.pNextWindow);
        std::swap(getGroupPrevious()->m_sGroupData.pNextWindow, pWindow->getGroupPrevious()->m_sGroupData.pNextWindow);
    }

    std::swap(m_sGroupData.head, pWindow->m_sGroupData.head);
    std::swap(m_sGroupData.locked, pWindow->m_sGroupData.locked);
}

void CWindow::updateGroupOutputs() {
    if (m_sGroupData.pNextWindow.expired())
        return;

    PHLWINDOW  curr = m_sGroupData.pNextWindow.lock();

    const auto WS = m_pWorkspace;

    while (curr.get() != this) {
        curr->m_iMonitorID = m_iMonitorID;
        curr->moveToWorkspace(WS);

        curr->m_vRealPosition = m_vRealPosition.goal();
        curr->m_vRealSize     = m_vRealSize.goal();

        curr = curr->m_sGroupData.pNextWindow.lock();
    }
}

Vector2D CWindow::middle() {
    return m_vRealPosition.goal() + m_vRealSize.goal() / 2.f;
}

bool CWindow::opaque() {
    if (m_fAlpha.value() != 1.f || m_fActiveInactiveAlpha.value() != 1.f)
        return false;

    if (m_vRealSize.goal().floor() != m_vReportedSize)
        return false;

    const auto PWORKSPACE = m_pWorkspace;

    if (m_pWLSurface.small() && !m_pWLSurface.m_bFillIgnoreSmall)
        return false;

    if (PWORKSPACE->m_fAlpha.value() != 1.f)
        return false;

    if (m_bIsX11)
        return !m_uSurface.xwayland->has_alpha;

    if (m_uSurface.xdg->surface->opaque)
        return true;

    const auto EXTENTS = pixman_region32_extents(&m_uSurface.xdg->surface->opaque_region);
    if (EXTENTS->x2 - EXTENTS->x1 >= m_uSurface.xdg->surface->current.buffer_width && EXTENTS->y2 - EXTENTS->y1 >= m_uSurface.xdg->surface->current.buffer_height)
        return true;

    return false;
}

float CWindow::rounding() {
    static auto PROUNDING = CConfigValue<Hyprlang::INT>("decoration:rounding");

    float       rounding = m_sAdditionalConfigData.rounding.toUnderlying() == -1 ? *PROUNDING : m_sAdditionalConfigData.rounding.toUnderlying();

    return m_sSpecialRenderData.rounding ? rounding : 0;
}

void CWindow::updateSpecialRenderData() {
    const auto PWORKSPACE    = m_pWorkspace;
    const auto WORKSPACERULE = PWORKSPACE ? g_pConfigManager->getWorkspaceRuleFor(PWORKSPACE) : SWorkspaceRule{};
    updateSpecialRenderData(WORKSPACERULE);
}

void CWindow::updateSpecialRenderData(const SWorkspaceRule& workspaceRule) {
    static auto PNOBORDERONFLOATING = CConfigValue<Hyprlang::INT>("general:no_border_on_floating");

    bool        border = true;
    if (m_bIsFloating && *PNOBORDERONFLOATING == 1)
        border = false;

    m_sSpecialRenderData.border     = workspaceRule.border.value_or(border);
    m_sSpecialRenderData.borderSize = workspaceRule.borderSize.value_or(-1);
    m_sSpecialRenderData.decorate   = workspaceRule.decorate.value_or(true);
    m_sSpecialRenderData.rounding   = workspaceRule.rounding.value_or(true);
    m_sSpecialRenderData.shadow     = workspaceRule.shadow.value_or(true);
}

int CWindow::getRealBorderSize() {
    if (!m_sSpecialRenderData.border || m_sAdditionalConfigData.forceNoBorder || (m_pWorkspace && m_bIsFullscreen && (m_pWorkspace->m_efFullscreenMode == FULLSCREEN_FULL)))
        return 0;

    if (m_sAdditionalConfigData.borderSize.toUnderlying() != -1)
        return m_sAdditionalConfigData.borderSize.toUnderlying();

    if (m_sSpecialRenderData.borderSize.toUnderlying() != -1)
        return m_sSpecialRenderData.borderSize.toUnderlying();

    static auto PBORDERSIZE = CConfigValue<Hyprlang::INT>("general:border_size");

    return *PBORDERSIZE;
}

bool CWindow::canBeTorn() {
    return (m_sAdditionalConfigData.forceTearing.toUnderlying() || m_bTearingHint);
}

bool CWindow::shouldSendFullscreenState() {
    const auto MODE = m_pWorkspace->m_efFullscreenMode;
    return m_bDontSendFullscreen ? false : (m_bFakeFullscreenState || (m_bIsFullscreen && (MODE == FULLSCREEN_FULL)));
}

void CWindow::setSuspended(bool suspend) {
    if (suspend == m_bSuspended)
        return;

    if (m_bIsX11)
        return;

    wlr_xdg_toplevel_set_suspended(m_uSurface.xdg->toplevel, suspend);
    m_bSuspended = suspend;
}

bool CWindow::visibleOnMonitor(CMonitor* pMonitor) {
    CBox wbox = {m_vRealPosition.value(), m_vRealSize.value()};

    return !wbox.intersection({pMonitor->vecPosition, pMonitor->vecSize}).empty();
}

void CWindow::setAnimationsToMove() {
    auto* const PANIMCFG = g_pConfigManager->getAnimationPropertyConfig("windowsMove");
    m_vRealPosition.setConfig(PANIMCFG);
    m_vRealSize.setConfig(PANIMCFG);
    m_bAnimatingIn = false;
}

void CWindow::onWorkspaceAnimUpdate() {
    // clip box for animated offsets
    if (!m_bIsFloating || m_bPinned || m_bIsFullscreen) {
        m_vFloatingOffset = Vector2D(0, 0);
        return;
    }

    Vector2D   offset;
    const auto PWORKSPACE = m_pWorkspace;
    if (!PWORKSPACE)
        return;

    const auto PWSMON = g_pCompositor->getMonitorFromID(PWORKSPACE->m_iMonitorID);
    if (!PWSMON)
        return;

    const auto WINBB = getFullWindowBoundingBox();
    if (PWORKSPACE->m_vRenderOffset.value().x != 0) {
        const auto PROGRESS = PWORKSPACE->m_vRenderOffset.value().x / PWSMON->vecSize.x;

        if (WINBB.x < PWSMON->vecPosition.x)
            offset.x += (PWSMON->vecPosition.x - WINBB.x) * PROGRESS;

        if (WINBB.x + WINBB.width > PWSMON->vecPosition.x + PWSMON->vecSize.x)
            offset.x += (WINBB.x + WINBB.width - PWSMON->vecPosition.x - PWSMON->vecSize.x) * PROGRESS;
    } else if (PWORKSPACE->m_vRenderOffset.value().y != 0) {
        const auto PROGRESS = PWORKSPACE->m_vRenderOffset.value().y / PWSMON->vecSize.y;

        if (WINBB.y < PWSMON->vecPosition.y)
            offset.y += (PWSMON->vecPosition.y - WINBB.y) * PROGRESS;

        if (WINBB.y + WINBB.height > PWSMON->vecPosition.y + PWSMON->vecSize.y)
            offset.y += (WINBB.y + WINBB.height - PWSMON->vecPosition.y - PWSMON->vecSize.y) * PROGRESS;
    }

    m_vFloatingOffset = offset;
}

int CWindow::popupsCount() {
    if (m_bIsX11)
        return 1;

    int no = 0;
    wlr_xdg_surface_for_each_popup_surface(
        m_uSurface.xdg, [](wlr_surface* s, int x, int y, void* data) { *(int*)data += 1; }, &no);
    return no;
}

int CWindow::workspaceID() {
    return m_pWorkspace ? m_pWorkspace->m_iID : m_iLastWorkspace;
}

bool CWindow::onSpecialWorkspace() {
    return m_pWorkspace ? m_pWorkspace->m_bIsSpecialWorkspace : g_pCompositor->isWorkspaceSpecial(m_iLastWorkspace);
}

std::unordered_map<std::string, std::string> CWindow::getEnv() {

    const auto PID = getPID();

    if (PID <= 1)
        return {};

    std::unordered_map<std::string, std::string> results;

    //
    std::string   environFile = "/proc/" + std::to_string(PID) + "/environ";
    std::ifstream ifs(environFile, std::ios::binary);

    if (!ifs.good())
        return {};

    std::vector<char> buffer;
    size_t            needle = 0;
    buffer.resize(512, '\0');
    while (ifs.read(buffer.data() + needle, 512)) {
        buffer.resize(buffer.size() + 512, '\0');
        needle += 512;
    }

    std::replace(buffer.begin(), buffer.end() - 1, '\0', '\n');

    CVarList envs(std::string{buffer.data(), needle - 1}, 0, '\n', true);

    for (auto& e : envs) {
        if (!e.contains('='))
            continue;

        const auto EQ            = e.find_first_of('=');
        results[e.substr(0, EQ)] = e.substr(EQ + 1);
    }

    return results;
}

void CWindow::activate() {
    static auto PFOCUSONACTIVATE = CConfigValue<Hyprlang::INT>("misc:focus_on_activate");

    g_pEventManager->postEvent(SHyprIPCEvent{"urgent", std::format("{:x}", (uintptr_t)this)});
    EMIT_HOOK_EVENT("urgent", m_pSelf.lock());

    m_bIsUrgent = true;

    if (!*PFOCUSONACTIVATE || (m_eSuppressedEvents & SUPPRESS_ACTIVATE_FOCUSONLY) || (m_eSuppressedEvents & SUPPRESS_ACTIVATE))
        return;

    if (m_bIsFloating)
        g_pCompositor->changeWindowZOrder(m_pSelf.lock(), true);

    g_pCompositor->focusWindow(m_pSelf.lock());
    g_pCompositor->warpCursorTo(middle());
}
