#include "Compositor.hpp"

CCompositor::CCompositor() {
    unlink("/tmp/hypr/hyprland.log");
    unlink("/tmp/hypr/hyprlandd.log");
    unlink("/tmp/hypr/.hyprlandrq");

    system("mkdir -p /tmp/hypr");

    m_sWLDisplay = wl_display_create();

    m_sWLRBackend = wlr_backend_autocreate(m_sWLDisplay);

    if (!m_sWLRBackend) {
        Debug::log(CRIT, "m_sWLRBackend was NULL!");
        RIP("m_sWLRBackend NULL!");
        return;
    }

    m_iDRMFD = wlr_backend_get_drm_fd(m_sWLRBackend);
    if (m_iDRMFD < 0) {
        Debug::log(CRIT, "Couldn't query the DRM FD!");
        RIP("DRMFD NULL!");
        return;
    }

    m_sWLRRenderer = wlr_gles2_renderer_create_with_drm_fd(m_iDRMFD);

    if (!m_sWLRRenderer) {
        Debug::log(CRIT, "m_sWLRRenderer was NULL!");
        RIP("m_sWLRRenderer NULL!");
        return;
    }

    wlr_renderer_init_wl_display(m_sWLRRenderer, m_sWLDisplay);

    m_sWLRAllocator = wlr_allocator_autocreate(m_sWLRBackend, m_sWLRRenderer);

    if (!m_sWLRAllocator) {
        Debug::log(CRIT, "m_sWLRAllocator was NULL!");
        RIP("m_sWLRAllocator NULL!");
        return;
    }

    m_sWLREGL = wlr_gles2_renderer_get_egl(m_sWLRRenderer);

    if (!m_sWLREGL) {
        Debug::log(CRIT, "m_sWLREGL was NULL!");
        RIP("m_sWLREGL NULL!");
        return;
    }

    m_sWLRCompositor = wlr_compositor_create(m_sWLDisplay, m_sWLRRenderer);
    m_sWLRSubCompositor = wlr_subcompositor_create(m_sWLDisplay);
    m_sWLRDataDevMgr = wlr_data_device_manager_create(m_sWLDisplay);

    m_sWLRDmabuf = wlr_linux_dmabuf_v1_create(m_sWLDisplay, m_sWLRRenderer);
    wlr_export_dmabuf_manager_v1_create(m_sWLDisplay);
    wlr_screencopy_manager_v1_create(m_sWLDisplay);
    wlr_data_control_manager_v1_create(m_sWLDisplay);
    wlr_gamma_control_manager_v1_create(m_sWLDisplay);
    wlr_primary_selection_v1_device_manager_create(m_sWLDisplay);
    wlr_viewporter_create(m_sWLDisplay);

    m_sWLROutputLayout = wlr_output_layout_create();

    m_sWLRScene = wlr_scene_create();
    wlr_scene_attach_output_layout(m_sWLRScene, m_sWLROutputLayout);

    m_sWLRXDGShell = wlr_xdg_shell_create(m_sWLDisplay);

    m_sWLRCursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(m_sWLRCursor, m_sWLROutputLayout);

    m_sWLRXCursorMgr = wlr_xcursor_manager_create(nullptr, 24);
    wlr_xcursor_manager_load(m_sWLRXCursorMgr, 1);

    m_sSeat.seat = wlr_seat_create(m_sWLDisplay, "seat0");

    m_sWLRPresentation = wlr_presentation_create(m_sWLDisplay, m_sWLRBackend);

    m_sWLRIdle = wlr_idle_create(m_sWLDisplay);

    m_sWLRLayerShell = wlr_layer_shell_v1_create(m_sWLDisplay);

    m_sWLRServerDecoMgr = wlr_server_decoration_manager_create(m_sWLDisplay);
    m_sWLRXDGDecoMgr = wlr_xdg_decoration_manager_v1_create(m_sWLDisplay);
    wlr_server_decoration_manager_set_default_mode(m_sWLRServerDecoMgr, WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);

    wlr_xdg_output_manager_v1_create(m_sWLDisplay, m_sWLROutputLayout);
    m_sWLROutputMgr = wlr_output_manager_v1_create(m_sWLDisplay);

    m_sWLRInhibitMgr = wlr_input_inhibit_manager_create(m_sWLDisplay);
    m_sWLRKbShInhibitMgr = wlr_keyboard_shortcuts_inhibit_v1_create(m_sWLDisplay);

    m_sWLREXTWorkspaceMgr = wlr_ext_workspace_manager_v1_create(m_sWLDisplay);

    m_sWLRPointerConstraints = wlr_pointer_constraints_v1_create(m_sWLDisplay);

    m_sWLRRelPointerMgr = wlr_relative_pointer_manager_v1_create(m_sWLDisplay);

    m_sWLRVKeyboardMgr = wlr_virtual_keyboard_manager_v1_create(m_sWLDisplay);

    m_sWLRVirtPtrMgr = wlr_virtual_pointer_manager_v1_create(m_sWLDisplay);
}

CCompositor::~CCompositor() {
    cleanupExit();
}

void handleCritSignal(int signo) {
    g_pCompositor->cleanupExit();
    exit(signo);
}

void CCompositor::initAllSignals() {
    addWLSignal(&m_sWLRBackend->events.new_output, &Events::listen_newOutput, m_sWLRBackend, "Backend");
    addWLSignal(&m_sWLRXDGShell->events.new_surface, &Events::listen_newXDGSurface, m_sWLRXDGShell, "XDG Shell");
    addWLSignal(&m_sWLRCursor->events.motion, &Events::listen_mouseMove, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.motion_absolute, &Events::listen_mouseMoveAbsolute, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.button, &Events::listen_mouseButton, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.axis, &Events::listen_mouseAxis, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.frame, &Events::listen_mouseFrame, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRBackend->events.new_input, &Events::listen_newInput, m_sWLRBackend, "Backend");
    addWLSignal(&m_sSeat.seat->events.request_set_cursor, &Events::listen_requestMouse, &m_sSeat, "Seat");
    addWLSignal(&m_sSeat.seat->events.request_set_selection, &Events::listen_requestSetSel, &m_sSeat, "Seat");
    addWLSignal(&m_sSeat.seat->events.request_start_drag, &Events::listen_requestDrag, &m_sSeat, "Seat");
    addWLSignal(&m_sSeat.seat->events.start_drag, &Events::listen_startDrag, &m_sSeat, "Seat");
    addWLSignal(&m_sWLRLayerShell->events.new_surface, &Events::listen_newLayerSurface, m_sWLRLayerShell, "LayerShell");
    addWLSignal(&m_sWLROutputLayout->events.change, &Events::listen_change, m_sWLROutputLayout, "OutputLayout");
    addWLSignal(&m_sWLROutputMgr->events.apply, &Events::listen_outputMgrApply, m_sWLROutputMgr, "OutputMgr");
    addWLSignal(&m_sWLROutputMgr->events.test, &Events::listen_outputMgrTest, m_sWLROutputMgr, "OutputMgr");
    addWLSignal(&m_sWLRInhibitMgr->events.activate, &Events::listen_InhibitActivate, m_sWLRInhibitMgr, "InhibitMgr");
    addWLSignal(&m_sWLRInhibitMgr->events.deactivate, &Events::listen_InhibitDeactivate, m_sWLRInhibitMgr, "InhibitMgr");
    addWLSignal(&m_sWLRPointerConstraints->events.new_constraint, &Events::listen_newConstraint, m_sWLRPointerConstraints, "PointerConstraints");
    addWLSignal(&m_sWLRXDGDecoMgr->events.new_toplevel_decoration, &Events::listen_NewXDGDeco, m_sWLRXDGDecoMgr, "XDGDecoMgr");
    addWLSignal(&m_sWLRVirtPtrMgr->events.new_virtual_pointer, &Events::listen_newVirtPtr, m_sWLRVirtPtrMgr, "VirtPtrMgr");

    signal(SIGINT, handleCritSignal);
    signal(SIGTERM, handleCritSignal);
}

void CCompositor::cleanupExit() {
    if (!m_sWLDisplay)
        return;

    if (g_pXWaylandManager->m_sWLRXWayland) {
        wlr_xwayland_destroy(g_pXWaylandManager->m_sWLRXWayland);
        g_pXWaylandManager->m_sWLRXWayland = nullptr;
    }

    wl_display_destroy_clients(m_sWLDisplay);
    wl_display_destroy(m_sWLDisplay);

    m_sWLDisplay = nullptr;
}

void CCompositor::startCompositor() {
    // Init all the managers BEFORE we start with the wayland server so that ALL of the stuff is initialized
    // properly and we dont get any bad mem reads.
    //
    Debug::log(LOG, "Creating the CHyprError!");
    g_pHyprError = std::make_unique<CHyprError>();
    
    Debug::log(LOG, "Creating the KeybindManager!");
    g_pKeybindManager = std::make_unique<CKeybindManager>();

    Debug::log(LOG, "Creating the AnimationManager!");
    g_pAnimationManager = std::make_unique<CAnimationManager>();

    Debug::log(LOG, "Creating the ConfigManager!");
    g_pConfigManager = std::make_unique<CConfigManager>();

    Debug::log(LOG, "Creating the ThreadManager!");
    g_pThreadManager = std::make_unique<CThreadManager>();

    Debug::log(LOG, "Creating the InputManager!");
    g_pInputManager = std::make_unique<CInputManager>();

    Debug::log(LOG, "Creating the CHyprOpenGLImpl!");
    g_pHyprOpenGL = std::make_unique<CHyprOpenGLImpl>();

    Debug::log(LOG, "Creating the HyprRenderer!");
    g_pHyprRenderer = std::make_unique<CHyprRenderer>();

    Debug::log(LOG, "Creating the XWaylandManager!");
    g_pXWaylandManager = std::make_unique<CHyprXWaylandManager>();

    Debug::log(LOG, "Creating the LayoutManager!");
    g_pLayoutManager = std::make_unique<CLayoutManager>();
    //
    //

    initAllSignals();

    // Set some env vars so that Firefox is automatically in Wayland mode
    // and QT apps too
    // electron needs -- flags so we can't really set them here
    setenv("QT_QPA_PLATFORM", "wayland", true);
    setenv("MOZ_ENABLE_WAYLAND", "1", true);

    // Set XDG_CURRENT_DESKTOP to our compositor's name
    setenv("XDG_CURRENT_DESKTOP", "hyprland", true);

    m_szWLDisplaySocket = wl_display_add_socket_auto(m_sWLDisplay);

    if (!m_szWLDisplaySocket) {
        Debug::log(CRIT, "m_szWLDisplaySocket NULL!");
        wlr_backend_destroy(m_sWLRBackend);
        RIP("m_szWLDisplaySocket NULL!");
    }

    setenv("WAYLAND_DISPLAY", m_szWLDisplaySocket, 1);

    signal(SIGPIPE, SIG_IGN);

    Debug::log(LOG, "Running on WAYLAND_DISPLAY: %s", m_szWLDisplaySocket);

    if (!wlr_backend_start(m_sWLRBackend)) {
        Debug::log(CRIT, "Backend did not start!");
        wlr_backend_destroy(m_sWLRBackend);
        wl_display_destroy(m_sWLDisplay);
        RIP("Backend did not start!");
    }

    wlr_xcursor_manager_set_cursor_image(m_sWLRXCursorMgr, "left_ptr", m_sWLRCursor);

    // This blocks until we are done.
    Debug::log(LOG, "Hyprland is ready, running the event loop!");
    wl_display_run(m_sWLDisplay);
}

SMonitor* CCompositor::getMonitorFromID(const int& id) {
    for (auto& m : m_lMonitors) {
        if (m.ID == (uint64_t)id) {
            return &m;
        }
    }

    return nullptr;
}

SMonitor* CCompositor::getMonitorFromCursor() {
    const auto COORDS = Vector2D(m_sWLRCursor->x, m_sWLRCursor->y);
    const auto OUTPUT = wlr_output_layout_output_at(m_sWLROutputLayout, COORDS.x, COORDS.y);

    if (!OUTPUT) {
        Debug::log(WARN, "getMonitorFromCursor: cursor outside monitors??");
        return &m_lMonitors.front();
    }

    for (auto& m : m_lMonitors) {
        if (m.output == OUTPUT)
            return &m;
    }

    Debug::log(LOG, "Monitor not in list??");

    return &m_lMonitors.front();
}

SMonitor* CCompositor::getMonitorFromVector(const Vector2D& point) {
    const auto OUTPUT = wlr_output_layout_output_at(m_sWLROutputLayout, point.x, point.y);

    if (!OUTPUT) {
        Debug::log(WARN, "getMonitorFromVector: vector outside monitors? Returning front");
        return &m_lMonitors.front();
    }

    return getMonitorFromOutput(OUTPUT);
}

void CCompositor::removeWindowFromVectorSafe(CWindow* pWindow) {
    if (windowExists(pWindow) && !pWindow->m_bFadingOut)
        m_lWindows.remove(*pWindow);
}

bool CCompositor::windowExists(CWindow* pWindow) {
    for (auto& w : m_lWindows) {
        if (&w == pWindow)
            return true;
    }

    return false;
}

CWindow* CCompositor::vectorToWindow(const Vector2D& pos) {
    const auto PMONITOR = getMonitorFromVector(pos);
    // first loop over floating cuz they're above, m_lWindows should be sorted bottom->top, for tiled it doesn't matter.
    for (auto w = m_lWindows.rbegin(); w != m_lWindows.rend(); w++) {
        wlr_box box = {w->m_vRealPosition.vec().x, w->m_vRealPosition.vec().y, w->m_vRealSize.vec().x, w->m_vRealSize.vec().y};
        if (wlr_box_contains_point(&box, pos.x, pos.y) && w->m_bIsMapped && w->m_bIsFloating && isWorkspaceVisible(w->m_iWorkspaceID) && !w->m_bHidden)
            return &(*w);
    }

    for (auto& w : m_lWindows) {
        wlr_box box = {w.m_vRealPosition.vec().x, w.m_vRealPosition.vec().y, w.m_vRealSize.vec().x, w.m_vRealSize.vec().y};
        if (wlr_box_contains_point(&box, pos.x, pos.y) && w.m_bIsMapped && !w.m_bIsFloating && PMONITOR->activeWorkspace == w.m_iWorkspaceID && !w.m_bHidden)
            return &w;
    }

    return nullptr;
}

CWindow* CCompositor::vectorToWindowTiled(const Vector2D& pos) {
    const auto PMONITOR = getMonitorFromVector(pos);
    for (auto& w : m_lWindows) {
        wlr_box box = {w.m_vPosition.x, w.m_vPosition.y, w.m_vSize.x, w.m_vSize.y};
        if (w.m_bIsMapped && wlr_box_contains_point(&box, pos.x, pos.y) && w.m_iWorkspaceID == PMONITOR->activeWorkspace && !w.m_bIsFloating && !w.m_bHidden)
            return &w;
    }

    return nullptr;
}

CWindow* CCompositor::vectorToWindowIdeal(const Vector2D& pos) {
    const auto PMONITOR = getMonitorFromVector(pos);
    // first loop over floating cuz they're above, m_lWindows should be sorted bottom->top, for tiled it doesn't matter.
    for (auto w = m_lWindows.rbegin(); w != m_lWindows.rend(); w++) {
        wlr_box box = {w->m_vRealPosition.vec().x, w->m_vRealPosition.vec().y, w->m_vRealSize.vec().x, w->m_vRealSize.vec().y};
        if (w->m_bIsFloating && w->m_bIsMapped && wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y) && isWorkspaceVisible(w->m_iWorkspaceID) && !w->m_bHidden)
            return &(*w);
    }

    for (auto& w : m_lWindows) {
        wlr_box box = {w.m_vPosition.x, w.m_vPosition.y, w.m_vSize.x, w.m_vSize.y};
        if (!w.m_bIsFloating && w.m_bIsMapped && wlr_box_contains_point(&box, pos.x, pos.y) && w.m_iWorkspaceID == PMONITOR->activeWorkspace && !w.m_bHidden)
            return &w;
    }

    return nullptr;
}

CWindow* CCompositor::windowFromCursor() {
    const auto PMONITOR = getMonitorFromCursor();

    // first loop over floating cuz they're above, m_lWindows should be sorted bottom->top, for tiled it doesn't matter.
    for (auto w = m_lWindows.rbegin(); w != m_lWindows.rend(); w++) {
        wlr_box box = {w->m_vRealPosition.vec().x, w->m_vRealPosition.vec().y, w->m_vRealSize.vec().x, w->m_vRealSize.vec().y};
        if (wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y) && w->m_bIsMapped && w->m_bIsFloating && isWorkspaceVisible(w->m_iWorkspaceID))
            return &(*w);
    }

    for (auto& w : m_lWindows) {
        wlr_box box = {w.m_vPosition.x, w.m_vPosition.y, w.m_vSize.x, w.m_vSize.y};
        if (wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y) && w.m_bIsMapped && w.m_iWorkspaceID == PMONITOR->activeWorkspace)
            return &w;
    }

    return nullptr;
}

CWindow* CCompositor::windowFloatingFromCursor() {
    for (auto w = m_lWindows.rbegin(); w != m_lWindows.rend(); w++) {
        wlr_box box = {w->m_vRealPosition.vec().x, w->m_vRealPosition.vec().y, w->m_vRealSize.vec().x, w->m_vRealSize.vec().y};
        if (wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y) && w->m_bIsMapped && w->m_bIsFloating && isWorkspaceVisible(w->m_iWorkspaceID) && !w->m_bHidden)
            return &(*w);
    }

    return nullptr;
}

wlr_surface* CCompositor::vectorWindowToSurface(const Vector2D& pos, CWindow* pWindow, Vector2D& sl) {

    if (!windowValidMapped(pWindow))
        return nullptr;

    RASSERT(!pWindow->m_bIsX11, "Cannot call vectorWindowToSurface on an X11 window!");

    const auto PSURFACE = pWindow->m_uSurface.xdg;
    
    double subx, suby;

    const auto PFOUND = wlr_xdg_surface_surface_at(PSURFACE, pos.x - pWindow->m_vRealPosition.vec().x, pos.y - pWindow->m_vRealPosition.vec().y, &subx, &suby);

    if (PFOUND) {
        sl.x = subx;
        sl.y = suby;
        return PFOUND;
    }

    sl.x = pos.x - pWindow->m_vRealPosition.vec().x;
    sl.y = pos.y - pWindow->m_vRealPosition.vec().y;

    return PSURFACE->surface;
}

SMonitor* CCompositor::getMonitorFromOutput(wlr_output* out) {
    for (auto& m : m_lMonitors) {
        if (m.output == out) {
            return &m;
        }
    }

    return nullptr;
}

void CCompositor::focusWindow(CWindow* pWindow, wlr_surface* pSurface) {

    if (g_pCompositor->m_sSeat.exclusiveClient) {
        Debug::log(LOG, "Disallowing setting focus to a window due to there being an active input inhibitor layer.");
        return;
    }

    if (!pWindow || !windowValidMapped(pWindow)) {
        wlr_seat_keyboard_notify_clear_focus(m_sSeat.seat);
        return;
    }

    if (pWindow->m_bNoFocus) {
        Debug::log(LOG, "Ignoring focus to nofocus window!");
        return;
    }

    if (m_pLastWindow == pWindow && m_sSeat.seat->keyboard_state.focused_surface == pSurface)
        return;

    const auto PLASTWINDOW = m_pLastWindow;
    m_pLastWindow = pWindow;

    // we need to make the PLASTWINDOW not equal to m_pLastWindow so that RENDERDATA is correct for an unfocused window
    if (windowValidMapped(PLASTWINDOW)) {
        const auto RENDERDATA = g_pLayoutManager->getCurrentLayout()->requestRenderHints(PLASTWINDOW);
        if (RENDERDATA.isBorderColor)
            PLASTWINDOW->m_cRealBorderColor = RENDERDATA.borderColor;
        else
            PLASTWINDOW->m_cRealBorderColor = CColor(g_pConfigManager->getInt("general:col.inactive_border"));

        if (PLASTWINDOW->m_bIsX11) {
            wlr_seat_keyboard_notify_clear_focus(m_sSeat.seat);
            wlr_seat_pointer_clear_focus(m_sSeat.seat);
        }
    }

    m_pLastWindow = PLASTWINDOW;

    const auto PWINDOWSURFACE = pSurface ? pSurface : g_pXWaylandManager->getWindowSurface(pWindow);

    focusSurface(PWINDOWSURFACE, pWindow);

    g_pXWaylandManager->activateWindow(pWindow, true); // sets the m_pLastWindow

    // do pointer focus too                                     
    const auto POINTERLOCAL = g_pInputManager->getMouseCoordsInternal() - pWindow->m_vRealPosition.goalv();
    wlr_seat_pointer_notify_enter(m_sSeat.seat, PWINDOWSURFACE, POINTERLOCAL.x, POINTERLOCAL.y);

    const auto RENDERDATA = g_pLayoutManager->getCurrentLayout()->requestRenderHints(pWindow);
    if (RENDERDATA.isBorderColor)
        pWindow->m_cRealBorderColor = RENDERDATA.borderColor;
    else
        pWindow->m_cRealBorderColor = CColor(g_pConfigManager->getInt("general:col.active_border"));
}

void CCompositor::focusSurface(wlr_surface* pSurface, CWindow* pWindowOwner) {

    if (m_sSeat.seat->keyboard_state.focused_surface == pSurface || (pWindowOwner && m_sSeat.seat->keyboard_state.focused_surface == g_pXWaylandManager->getWindowSurface(pWindowOwner)))
        return;  // Don't focus when already focused on this.

    if (!pSurface)
        return;

    // Unfocus last surface if should
    if (m_pLastFocus && m_sSeat.seat->keyboard_state.focused_surface && wlr_surface_is_xdg_surface(m_pLastFocus))
        wlr_xdg_toplevel_set_activated(wlr_xdg_surface_from_wlr_surface(m_pLastFocus)->toplevel, false);

    const auto KEYBOARD = wlr_seat_get_keyboard(m_sSeat.seat);

    if (!KEYBOARD)
        return;

    wlr_seat_keyboard_notify_enter(m_sSeat.seat, pSurface, KEYBOARD->keycodes, KEYBOARD->num_keycodes, &KEYBOARD->modifiers);

    wlr_seat_keyboard_focus_change_event event = {
        .seat = m_sSeat.seat,
        .old_surface = m_pLastFocus,
        .new_surface = pSurface,
    };
    wlr_signal_emit_safe(&m_sSeat.seat->keyboard_state.events.focus_change, &event);

    if (pWindowOwner)
        Debug::log(LOG, "Set keyboard focus to surface %x, with window name: %s", pSurface, pWindowOwner->m_szTitle.c_str());
    else
        Debug::log(LOG, "Set keyboard focus to surface %x", pSurface);
}

bool CCompositor::windowValidMapped(CWindow* pWindow) {
    if (!windowExists(pWindow))
        return false;

    if (pWindow->m_bIsX11 && !pWindow->m_bMappedX11)
        return false;

    if (!pWindow->m_bIsMapped)
        return false;

    if (pWindow->m_bHidden)
        return false;

    if (!g_pXWaylandManager->getWindowSurface(pWindow))
        return false;

    return true;
}

CWindow* CCompositor::getWindowForPopup(wlr_xdg_popup* popup) {
    for (auto& p : m_lXDGPopups) {
        if (p.popup == popup)
            return p.parentWindow;
    }

    return nullptr;
}

wlr_surface* CCompositor::vectorToLayerSurface(const Vector2D& pos, std::list<SLayerSurface*>* layerSurfaces, Vector2D* sCoords) {
    for (auto& l : *layerSurfaces) {
        if (l->fadingOut || (l->layerSurface && !l->layerSurface->mapped))
            continue;

        const auto SURFACEAT = wlr_layer_surface_v1_surface_at(l->layerSurface, pos.x - l->geometry.x, pos.y - l->geometry.y, &sCoords->x, &sCoords->y);

        if (SURFACEAT)
            return SURFACEAT;
    }

    return nullptr;
}

CWindow* CCompositor::getWindowFromSurface(wlr_surface* pSurface) {
    for (auto& w : m_lWindows) {
        if (g_pXWaylandManager->getWindowSurface(&w) == pSurface)
            return &w;
    }

    return nullptr;
}

CWindow* CCompositor::getFullscreenWindowOnWorkspace(const int& ID) {
    for (auto& w : m_lWindows) {
        if (w.m_iWorkspaceID == ID && w.m_bIsFullscreen)
            return &w;
    }

    return nullptr;
}

bool CCompositor::isWorkspaceVisible(const int& w) {
    for (auto& m : m_lMonitors) {
        if (m.activeWorkspace == w)
            return true;
    }

    return false;
}

CWorkspace* CCompositor::getWorkspaceByID(const int& id) {
    for (auto& w : m_lWorkspaces) {
        if (w.m_iID == id)
            return &w;
    }

    return nullptr;
}

void CCompositor::sanityCheckWorkspaces() {
    for (auto it = m_lWorkspaces.begin(); it != m_lWorkspaces.end(); ++it) {
        if (getWindowsOnWorkspace(it->m_iID) == 0 && !isWorkspaceVisible(it->m_iID)) {
            it = m_lWorkspaces.erase(it);
        }
    }
}

int CCompositor::getWindowsOnWorkspace(const int& id) {
    int no = 0;
    for (auto& w : m_lWindows) {
        if (w.m_iWorkspaceID == id && w.m_bIsMapped)
            no++;
    }

    return no;
}

CWindow* CCompositor::getFirstWindowOnWorkspace(const int& id) {
    for (auto& w : m_lWindows) {
        if (w.m_iWorkspaceID == id)
            return &w;
    }

    return nullptr;
}

void CCompositor::fixXWaylandWindowsOnWorkspace(const int& id) {
    const auto ISVISIBLE = isWorkspaceVisible(id);

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(id);

    for (auto& w : m_lWindows) {
        if (w.m_iWorkspaceID == id) {

            // moveXWaylandWindow only moves XWayland windows
            // so there is no need to check here
            // if the window is XWayland or not.
            if (ISVISIBLE && (!PWORKSPACE->m_bHasFullscreenWindow || w.m_bIsFullscreen))
                g_pXWaylandManager->moveXWaylandWindow(&w, w.m_vRealPosition.vec());
            else 
                g_pXWaylandManager->moveXWaylandWindow(&w, Vector2D(42069,42069));
        }
    }
}

bool CCompositor::doesSeatAcceptInput(wlr_surface* surface) {
    return !m_sSeat.exclusiveClient || (surface && m_sSeat.exclusiveClient == wl_resource_get_client(surface->resource));
}

bool CCompositor::isWindowActive(CWindow* pWindow) {
    if (!windowValidMapped(pWindow))
        return false;

    const auto PSURFACE = g_pXWaylandManager->getWindowSurface(pWindow);

    return PSURFACE == m_pLastFocus || pWindow == m_pLastWindow;
}

void CCompositor::moveWindowToTop(CWindow* pWindow) {
    if (!windowValidMapped(pWindow))
        return;

    for (auto it = m_lWindows.begin(); it != m_lWindows.end(); ++it) {
        if (&(*it) == pWindow) {
            m_lWindows.splice(m_lWindows.end(), m_lWindows, it);
            break;
        }
    }
}

void CCompositor::cleanupFadingOut() {
    for (auto& w : m_lWindowsFadingOut) {

        bool valid = windowExists(w);
        
        if (!valid || !w->m_bFadingOut || w->m_fAlpha.fl() == 0.f) {
            if (valid && !w->m_bReadyToDelete)
                continue;

            g_pHyprOpenGL->m_mWindowFramebuffers[w].release();
            g_pHyprOpenGL->m_mWindowFramebuffers.erase(w);
            m_lWindows.remove(*w);
            m_lWindowsFadingOut.remove(w);

            Debug::log(LOG, "Cleanup: destroyed a window");
            return;
        }
    }

    for (auto& ls : m_lSurfacesFadingOut) {
        if (ls->fadingOut && ls->readyToDelete && !ls->alpha.isBeingAnimated()) {
            for (auto& m : m_lMonitors) {
                for (auto& lsl : m.m_aLayerSurfaceLists) {
                    lsl.remove(ls);
                }
            }

            g_pHyprOpenGL->m_mLayerFramebuffers[ls].release();
            g_pHyprOpenGL->m_mLayerFramebuffers.erase(ls);
            
            m_lSurfacesFadingOut.remove(ls);
            delete ls;

            Debug::log(LOG, "Cleanup: destroyed a layersurface");

            return;
        }
    }
}

CWindow* CCompositor::getWindowInDirection(CWindow* pWindow, char dir) {
    const auto POSA = pWindow->m_vPosition;
    const auto SIZEA = pWindow->m_vSize;

    auto longestIntersect = -1;
    CWindow* longestIntersectWindow = nullptr;

    for (auto& w : m_lWindows) {
        if (&w == pWindow || !windowValidMapped(&w) || w.m_bIsFloating || w.m_iWorkspaceID != pWindow->m_iWorkspaceID)
            continue;

        const auto POSB = w.m_vPosition;
        const auto SIZEB = w.m_vSize;
        switch (dir) {
            case 'l':
                if (STICKS(POSA.x, POSB.x + SIZEB.x)) {
                    const auto INTERSECTLEN = std::max((double)0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectWindow = &w;
                    }
                }
                break;
            case 'r':
                if (STICKS(POSA.x + SIZEA.x, POSB.x)) {
                    const auto INTERSECTLEN = std::max((double)0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectWindow = &w;
                    }
                }
                break;
            case 't':
            case 'u':
                if (STICKS(POSA.y, POSB.y + SIZEB.y)) {
                    const auto INTERSECTLEN = std::max((double)0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectWindow = &w;
                    }
                }
                break;
            case 'b':
            case 'd':
                if (STICKS(POSA.y + SIZEA.y, POSB.y)) {
                    const auto INTERSECTLEN = std::max((double)0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectWindow = &w;
                    }
                }
                break;
        }
    }

    if (longestIntersect != -1)
        return longestIntersectWindow;

    return nullptr;
}

void CCompositor::deactivateAllWLRWorkspaces(wlr_ext_workspace_handle_v1* exclude) {
    for (auto& w : m_lWorkspaces) {
        if (w.m_pWlrHandle && w.m_pWlrHandle != exclude)
            wlr_ext_workspace_handle_v1_set_active(w.m_pWlrHandle, false);
    }
}

CWindow* CCompositor::getNextWindowOnWorkspace(CWindow* pWindow) {
    bool gotToWindow = false;
    for (auto& w : m_lWindows) {
        if (&w != pWindow && !gotToWindow)
            continue;

        if (&w == pWindow) {
            gotToWindow = true;
            continue;
        }

        if (w.m_iWorkspaceID == pWindow->m_iWorkspaceID && windowValidMapped(&w))
            return &w;
    }

    for (auto& w : m_lWindows) {
        if (&w != pWindow && w.m_iWorkspaceID == pWindow->m_iWorkspaceID && windowValidMapped(&w))
            return &w;
    }

    return nullptr;
}

int CCompositor::getNextAvailableNamedWorkspace() {
    int lowest = -1337 + 1;
    for (auto& w : m_lWorkspaces) {
        if (w.m_iID < -1 && w.m_iID < lowest)
            lowest = w.m_iID;
    }

    return lowest - 1;
}

CWorkspace* CCompositor::getWorkspaceByName(const std::string& name) {
    for (auto& w : m_lWorkspaces) {
        if (w.m_szName == name)
            return &w;
    }

    return nullptr;
}

CWorkspace* CCompositor::getWorkspaceByString(const std::string& str) {
    if (str.find("name:") == 0) {
        return getWorkspaceByName(str.substr(str.find_first_of(':') + 1));
    }

    try {
        int id = std::stoi(str);
        return getWorkspaceByID(id);
    } catch (std::exception& e) {
        Debug::log(ERR, "Error in getWorkspaceByString, invalid id");
    }

    return nullptr;
}

bool CCompositor::isPointOnAnyMonitor(const Vector2D& point) {
    for (auto& m : m_lMonitors) {
        if (VECINRECT(point, m.vecPosition.x, m.vecPosition.y, m.vecSize.x + m.vecPosition.x, m.vecSize.y + m.vecPosition.y))
            return true;
    }

    return false;
}

CWindow* CCompositor::getConstraintWindow(SMouse* pMouse) {
    if (!pMouse->currentConstraint)
        return nullptr;

    const auto PSURFACE = pMouse->currentConstraint->surface;

    for (auto& w : m_lWindows) {
        if (PSURFACE == g_pXWaylandManager->getWindowSurface(&w)) {
            if (!w.m_bIsX11 && !windowValidMapped(&w))
                continue;

            return &w;
        }
    }

    return nullptr;
}

SMonitor* CCompositor::getMonitorInDirection(const char& dir) {
    const auto POSA = m_pLastMonitor->vecPosition;
    const auto SIZEA = m_pLastMonitor->vecSize;

    auto longestIntersect = -1;
    SMonitor* longestIntersectMonitor = nullptr;

    for (auto& m : m_lMonitors) {
        if (&m == m_pLastMonitor)
            continue;

        const auto POSB = m.vecPosition;
        const auto SIZEB = m.vecSize;
        switch (dir) {
            case 'l':
                if (STICKS(POSA.x, POSB.x + SIZEB.x)) {
                    const auto INTERSECTLEN = std::max((double)0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectMonitor = &m;
                    }
                }
                break;
            case 'r':
                if (STICKS(POSA.x + SIZEA.x, POSB.x)) {
                    const auto INTERSECTLEN = std::max((double)0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectMonitor = &m;
                    }
                }
                break;
            case 't':
            case 'u':
                if (STICKS(POSA.y, POSB.y + SIZEB.y)) {
                    const auto INTERSECTLEN = std::max((double)0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectMonitor = &m;
                    }
                }
                break;
            case 'b':
            case 'd':
                if (STICKS(POSA.y + SIZEA.y, POSB.y)) {
                    const auto INTERSECTLEN = std::max((double)0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectMonitor = &m;
                    }
                }
                break;
        }
    }

    if (longestIntersect != -1)
        return longestIntersectMonitor;

    return nullptr;
}
