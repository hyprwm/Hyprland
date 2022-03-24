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

    const auto DRMFD = wlr_backend_get_drm_fd(m_sWLRBackend);
    if (DRMFD < 0) {
        Debug::log(CRIT, "Couldn't query the DRM FD!");
        RIP("DRMFD NULL!");
        return;
    }

    m_sWLRRenderer = wlr_gles2_renderer_create_with_drm_fd(DRMFD);

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

    wlr_server_decoration_manager_set_default_mode(wlr_server_decoration_manager_create(m_sWLDisplay), WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
    wlr_xdg_decoration_manager_v1_create(m_sWLDisplay);

    wlr_xdg_output_manager_v1_create(m_sWLDisplay, m_sWLROutputLayout);
    m_sWLROutputMgr = wlr_output_manager_v1_create(m_sWLDisplay);

    m_sWLRInhibitMgr = wlr_input_inhibit_manager_create(m_sWLDisplay);
    m_sWLRKbShInhibitMgr = wlr_keyboard_shortcuts_inhibit_v1_create(m_sWLDisplay);
}

CCompositor::~CCompositor() {

}

void CCompositor::initAllSignals() {
    wl_signal_add(&m_sWLRBackend->events.new_output, &Events::listen_newOutput);
    wl_signal_add(&m_sWLRXDGShell->events.new_surface, &Events::listen_newXDGSurface);
    wl_signal_add(&m_sWLRCursor->events.motion, &Events::listen_mouseMove);
    wl_signal_add(&m_sWLRCursor->events.motion_absolute, &Events::listen_mouseMoveAbsolute);
    wl_signal_add(&m_sWLRCursor->events.button, &Events::listen_mouseButton);
    wl_signal_add(&m_sWLRCursor->events.axis, &Events::listen_mouseAxis);
    wl_signal_add(&m_sWLRCursor->events.frame, &Events::listen_mouseFrame);
    wl_signal_add(&m_sWLRBackend->events.new_input, &Events::listen_newInput);
    wl_signal_add(&m_sSeat.seat->events.request_set_cursor, &Events::listen_requestMouse);
    wl_signal_add(&m_sSeat.seat->events.request_set_selection, &Events::listen_requestSetSel);
    wl_signal_add(&m_sSeat.seat->events.request_start_drag, &Events::listen_requestDrag);
    wl_signal_add(&m_sWLRLayerShell->events.new_surface, &Events::listen_newLayerSurface);
    wl_signal_add(&m_sWLROutputLayout->events.change, &Events::listen_change);
    wl_signal_add(&m_sWLROutputMgr->events.apply, &Events::listen_outputMgrApply);
    wl_signal_add(&m_sWLROutputMgr->events.test, &Events::listen_outputMgrTest);
    wl_signal_add(&m_sWLRInhibitMgr->events.activate, &Events::listen_InhibitActivate);
    wl_signal_add(&m_sWLRInhibitMgr->events.deactivate, &Events::listen_InhibitDeactivate);
}

void CCompositor::startCompositor() {
    // Init all the managers BEFORE we start with the wayland server so that ALL of the stuff is initialized
    // properly and we dont get any bad mem reads.
    //
    Debug::log(LOG, "Creating the KeybindManager!");
    g_pKeybindManager = std::make_unique<CKeybindManager>();

    Debug::log(LOG, "Creating the ConfigManager!");
    g_pConfigManager = std::make_unique<CConfigManager>();

    Debug::log(LOG, "Creating the ThreadManager!");
    g_pThreadManager = std::make_unique<CThreadManager>();

    Debug::log(LOG, "Creating the InputManager!");
    g_pInputManager = std::make_unique<CInputManager>();

    Debug::log(LOG, "Creating the HyprRenderer!");
    g_pHyprRenderer = std::make_unique<CHyprRenderer>();

    Debug::log(LOG, "Creating the XWaylandManager!");
    g_pXWaylandManager = std::make_unique<CHyprXWaylandManager>();

    Debug::log(LOG, "Creating the LayoutManager!");
    g_pLayoutManager = std::make_unique<CLayoutManager>();

    Debug::log(LOG, "Creating the AnimationManager!");
    g_pAnimationManager = std::make_unique<CAnimationManager>();
    //
    //

    initAllSignals();

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
        Debug::log(WARN, "getMonitorFromVector: vector outside monitors??");
        return nullptr;
    }

    return getMonitorFromOutput(OUTPUT);
}

void CCompositor::removeWindowFromVectorSafe(CWindow* pWindow) {
    if (windowExists(pWindow))
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
    // first loop over floating cuz they're above
    // TODO: make an actual Z-system
    for (auto& w : m_lWindows) {
        wlr_box box = {w.m_vRealPosition.x, w.m_vRealPosition.y, w.m_vRealSize.x, w.m_vRealSize.y};
        if (wlr_box_contains_point(&box, pos.x, pos.y) && w.m_bIsFloating && isWorkspaceVisible(w.m_iWorkspaceID))
            return &w;
    }

    for (auto& w : m_lWindows) {
        wlr_box box = {w.m_vRealPosition.x, w.m_vRealPosition.y, w.m_vRealSize.x, w.m_vRealSize.y};
        if (wlr_box_contains_point(&box, pos.x, pos.y) && !w.m_bIsFloating && PMONITOR->activeWorkspace == w.m_iWorkspaceID)
            return &w;
    }

    return nullptr;
}

CWindow* CCompositor::vectorToWindowTiled(const Vector2D& pos) {
    const auto PMONITOR = getMonitorFromVector(pos);
    for (auto& w : m_lWindows) {
        wlr_box box = {w.m_vPosition.x, w.m_vPosition.y, w.m_vSize.x, w.m_vSize.y};
        if (wlr_box_contains_point(&box, pos.x, pos.y) && w.m_iWorkspaceID == PMONITOR->activeWorkspace && !w.m_bIsFloating)
            return &w;
    }

    return nullptr;
}

CWindow* CCompositor::vectorToWindowIdeal(const Vector2D& pos) {
    const auto PMONITOR = getMonitorFromVector(pos);
    // first loop over floating cuz they're above
    // TODO: make an actual Z-system
    for (auto& w : m_lWindows) {
        wlr_box box = {w.m_vRealPosition.x, w.m_vRealPosition.y, w.m_vRealSize.x, w.m_vRealSize.y};
        if (w.m_bIsFloating && wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y) && isWorkspaceVisible(w.m_iWorkspaceID))
            return &w;
    }

    for (auto& w : m_lWindows) {
        wlr_box box = {w.m_vPosition.x, w.m_vPosition.y, w.m_vSize.x, w.m_vSize.y};
        if (!w.m_bIsFloating && wlr_box_contains_point(&box, pos.x, pos.y) && w.m_iWorkspaceID == PMONITOR->activeWorkspace)
            return &w;
    }

    return nullptr;
}

CWindow* CCompositor::windowFromCursor() {
    const auto PMONITOR = getMonitorFromCursor();

    // first loop over floating cuz they're above
    // TODO: make an actual Z-system
    for (auto& w : m_lWindows) {
        wlr_box box = {w.m_vRealPosition.x, w.m_vRealPosition.y, w.m_vRealSize.x, w.m_vRealSize.y};
        if (wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y) && w.m_bIsFloating && isWorkspaceVisible(w.m_iWorkspaceID))
            return &w;
    }

    for (auto& w : m_lWindows) {
        wlr_box box = {w.m_vPosition.x, w.m_vPosition.y, w.m_vSize.x, w.m_vSize.y};
        if (wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y) && w.m_iWorkspaceID == PMONITOR->activeWorkspace)
            return &w;
    }

    return nullptr;
}

CWindow* CCompositor::windowFloatingFromCursor() {
    for (auto& w : m_lWindows) {
        wlr_box box = {w.m_vRealPosition.x, w.m_vRealPosition.y, w.m_vRealSize.x, w.m_vRealSize.y};
        if (wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y) && w.m_bIsFloating && isWorkspaceVisible(w.m_iWorkspaceID))
            return &w;
    }

    return nullptr;
}

SMonitor* CCompositor::getMonitorFromOutput(wlr_output* out) {
    for (auto& m : m_lMonitors) {
        if (m.output == out) {
            return &m;
        }
    }

    return nullptr;
}

void CCompositor::focusWindow(CWindow* pWindow) {

    if (!pWindow) {
        wlr_seat_keyboard_notify_clear_focus(m_sSeat.seat);
        return;
    }

    const auto PWINDOWSURFACE = g_pXWaylandManager->getWindowSurface(pWindow);

    focusSurface(PWINDOWSURFACE);

    Debug::log(LOG, "Set keyboard focus to %x, with name: %s", pWindow, pWindow->m_szTitle.c_str());
}

void CCompositor::focusSurface(wlr_surface* pSurface) {
    if (m_sSeat.seat->keyboard_state.focused_surface == pSurface)
        return;  // Don't focus when already focused on this.

    // Unfocus last surface
    if (m_pLastFocus && !wlr_surface_is_xwayland_surface(m_pLastFocus))
        g_pXWaylandManager->activateSurface(m_pLastFocus, false);

    const auto KEYBOARD = wlr_seat_get_keyboard(m_sSeat.seat);
    wlr_seat_keyboard_notify_enter(m_sSeat.seat, pSurface, KEYBOARD->keycodes, KEYBOARD->num_keycodes, &KEYBOARD->modifiers);

    m_pLastFocus = pSurface;

    g_pXWaylandManager->activateSurface(pSurface, true);
}

bool CCompositor::windowValidMapped(CWindow* pWindow) {
    if (!windowExists(pWindow))
        return false;

    if (pWindow->m_bIsX11 && !pWindow->m_bMappedX11)
        return false;

    if (!pWindow->m_bIsMapped)
        return false;

    if (!g_pXWaylandManager->getWindowSurface(pWindow))
        return false;

    return true;
}

SLayerSurface* CCompositor::getLayerForPopup(SLayerPopup* pPopup) {
    auto CurrentPopup = pPopup;
    while (CurrentPopup->parentPopup != nullptr) {
        for (auto& p : g_pCompositor->m_lLayerPopups) {
            if (p.popup == CurrentPopup->parentPopup) {
                CurrentPopup = &p;
                break;
            }
        }
    }

    return CurrentPopup->parentSurface;
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
        if (!l->layerSurface->mapped)
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

SWorkspace* CCompositor::getWorkspaceByID(const int& id) {
    for (auto& w : m_lWorkspaces) {
        if (w.ID == id)
            return &w;
    }

    return nullptr;
}

void CCompositor::sanityCheckWorkspaces() {
    for (auto it = m_lWorkspaces.begin(); it != m_lWorkspaces.end(); ++it) {
        if (getWindowsOnWorkspace(it->ID) == 0 && !isWorkspaceVisible(it->ID))
            it = m_lWorkspaces.erase(it);
    }
}

int CCompositor::getWindowsOnWorkspace(const int& id) {
    int no = 0;
    for (auto& w : m_lWindows) {
        if (w.m_iWorkspaceID == id)
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
            if (ISVISIBLE && (!PWORKSPACE->hasFullscreenWindow || w.m_bIsFullscreen))
                g_pXWaylandManager->moveXWaylandWindow(&w, w.m_vRealPosition);
            else 
                g_pXWaylandManager->moveXWaylandWindow(&w, Vector2D(42069,42069));
        }
    }
}

bool CCompositor::doesSeatAcceptInput(wlr_surface* surface) {
    return !m_sSeat.exclusiveClient || (surface && m_sSeat.exclusiveClient == wl_resource_get_client(surface->resource));
}