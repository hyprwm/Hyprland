#include "Window.hpp"
#include "Compositor.hpp"
#include "render/decorations/CHyprDropShadowDecoration.hpp"

CWindow::CWindow() {
    m_vRealPosition.create(AVARTYPE_VECTOR, g_pConfigManager->getAnimationPropertyConfig("windowsIn"), (void*) this, AVARDAMAGE_ENTIRE);
    m_vRealSize.create(AVARTYPE_VECTOR, g_pConfigManager->getAnimationPropertyConfig("windowsIn"), (void*)this, AVARDAMAGE_ENTIRE);
    m_cRealBorderColor.create(AVARTYPE_COLOR, g_pConfigManager->getAnimationPropertyConfig("border"), (void*)this, AVARDAMAGE_BORDER);
    m_fAlpha.create(AVARTYPE_FLOAT, g_pConfigManager->getAnimationPropertyConfig("fadeIn"), (void*)this, AVARDAMAGE_ENTIRE);
    m_fActiveInactiveAlpha.create(AVARTYPE_FLOAT, g_pConfigManager->getAnimationPropertyConfig("fadeSwitch"), (void*)this, AVARDAMAGE_ENTIRE);
    m_cRealShadowColor.create(AVARTYPE_COLOR, g_pConfigManager->getAnimationPropertyConfig("fadeShadow"), (void*)this, AVARDAMAGE_SHADOW);
    m_fDimPercent.create(AVARTYPE_FLOAT, g_pConfigManager->getAnimationPropertyConfig("fadeDim"), (void*)this, AVARDAMAGE_ENTIRE);

    m_dWindowDecorations.emplace_back(std::make_unique<CHyprDropShadowDecoration>(this)); // put the shadow so it's the first deco (has to be rendered first)
}

CWindow::~CWindow() {
    if (g_pCompositor->isWindowActive(this)) {
        g_pCompositor->m_pLastFocus = nullptr;
        g_pCompositor->m_pLastWindow = nullptr;
    }
}

wlr_box CWindow::getFullWindowBoundingBox() {
    static auto *const PBORDERSIZE = &g_pConfigManager->getConfigValuePtr("general:border_size")->intValue;

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
    wlr_box finalBox = {m_vRealPosition.vec().x - maxExtents.topLeft.x,
                        m_vRealPosition.vec().y - maxExtents.topLeft.y,
                        m_vRealSize.vec().x + maxExtents.topLeft.x + maxExtents.bottomRight.x,
                        m_vRealSize.vec().y + maxExtents.topLeft.y + maxExtents.bottomRight.y};
    
    return finalBox;
}

wlr_box CWindow::getWindowIdealBoundingBoxIgnoreReserved() {

    const auto PMONITOR = g_pCompositor->getMonitorFromID(m_iMonitorID);

    auto POS = m_vPosition;
    auto SIZE = m_vSize;

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
        const auto CLIENT = wl_resource_get_client(m_uSurface.xdg->resource);
        wl_client_get_credentials(CLIENT, &PID, nullptr, nullptr);
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
    hyprListener_toplevelActivate.initCallback(&m_phForeignToplevel->events.request_activate, [&](void* owner, void* data) {

        g_pCompositor->focusWindow(this);

    }, this, "Toplevel");

    hyprListener_toplevelFullscreen.initCallback(&m_phForeignToplevel->events.request_fullscreen, [&](void* owner, void* data) {

        const auto EV = (wlr_foreign_toplevel_handle_v1_fullscreen_event*)data;

        g_pCompositor->setWindowFullscreen(this, EV->fullscreen, FULLSCREEN_FULL);

    }, this, "Toplevel");

    hyprListener_toplevelClose.initCallback(&m_phForeignToplevel->events.request_close, [&](void* owner, void* data) {

        g_pCompositor->closeWindow(this);

    }, this, "Toplevel");

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
        wlr_surface_for_each_surface(g_pXWaylandManager->getWindowSurface(this), sendLeaveIter, PLASTMONITOR->output);

    wlr_surface_for_each_surface(g_pXWaylandManager->getWindowSurface(this), sendEnterIter, PNEWMONITOR->output);
}

void CWindow::moveToWorkspace(int workspaceID) {
    if (m_iWorkspaceID != workspaceID) {
        m_iWorkspaceID = workspaceID;

        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(m_iWorkspaceID);
        if (PWORKSPACE) {
            g_pEventManager->postEvent(SHyprIPCEvent{"movewindow", getFormat("%x,%s", this, PWORKSPACE->m_szName.c_str())});
        } 
    }
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
