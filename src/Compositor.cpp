#include "Compositor.hpp"

CCompositor::CCompositor() {

    m_sWLDisplay = wl_display_create();

    m_sWLRBackend = wlr_backend_autocreate(m_sWLDisplay);

    if (!m_sWLRBackend) {
        Debug::log(CRIT, "m_sWLRBackend was NULL!");
        RIP("m_sWLRBackend NULL!");
        return;
    }

    m_sWLRRenderer = wlr_renderer_autocreate(m_sWLRBackend);

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

    m_sWLRSeat = wlr_seat_create(m_sWLDisplay, "seat0");

    m_sWLRPresentation = wlr_presentation_create(m_sWLDisplay, m_sWLRBackend);

    m_sWLRIdle = wlr_idle_create(m_sWLDisplay);

    m_sWLRLayerShell = wlr_layer_shell_v1_create(m_sWLDisplay);

    wlr_server_decoration_manager_set_default_mode(wlr_server_decoration_manager_create(m_sWLDisplay), WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
    wlr_xdg_decoration_manager_v1_create(m_sWLDisplay);

    wlr_xdg_output_manager_v1_create(m_sWLDisplay, m_sWLROutputLayout);
    m_sWLROutputMgr = wlr_output_manager_v1_create(m_sWLDisplay);
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
    wl_signal_add(&m_sWLRSeat->events.request_set_cursor, &Events::listen_requestMouse);
    wl_signal_add(&m_sWLRSeat->events.request_set_selection, &Events::listen_requestSetSel);
    wl_signal_add(&m_sWLRLayerShell->events.new_surface, &Events::listen_newLayerSurface);
    wl_signal_add(&m_sWLROutputLayout->events.change, &Events::listen_change);
    wl_signal_add(&m_sWLROutputMgr->events.apply, &Events::listen_outputMgrApply);
    wl_signal_add(&m_sWLROutputMgr->events.test, &Events::listen_outputMgrTest);
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
        if (m.ID == id) {
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
    const auto PMONITOR = getMonitorFromCursor();
    for (auto& w : m_lWindows) {
        wlr_box box = {w.m_vRealPosition.x, w.m_vRealPosition.y, w.m_vRealSize.x, w.m_vRealSize.y};
        if (w.m_iMonitorID == PMONITOR->ID && wlr_box_contains_point(&box, pos.x, pos.y))
            return &w;
    }

    return nullptr;
}

CWindow* CCompositor::vectorToWindowIdeal(const Vector2D& pos) {
    const auto PMONITOR = getMonitorFromCursor();
    for (auto& w : m_lWindows) {
        wlr_box box = {w.m_vPosition.x, w.m_vPosition.y, w.m_vSize.x, w.m_vSize.y};
        if (w.m_iMonitorID == PMONITOR->ID && wlr_box_contains_point(&box, pos.x, pos.y))
            return &w;
    }

    return nullptr;
}

CWindow* CCompositor::windowFromCursor() {
    const auto PMONITOR = getMonitorFromCursor();
    for (auto& w : m_lWindows) {
        wlr_box box = {w.m_vPosition.x, w.m_vPosition.y, w.m_vSize.x, w.m_vSize.y};
        if (w.m_iMonitorID == PMONITOR->ID && wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y))
            return &w;
    }

    return nullptr;
}

void CCompositor::focusWindow(CWindow* pWindow) {

    if (!pWindow) {
        wlr_seat_keyboard_notify_clear_focus(m_sWLRSeat);
        return;
    }

    const auto PWINDOWSURFACE = g_pXWaylandManager->getWindowSurface(pWindow);

    if (m_sWLRSeat->keyboard_state.focused_surface == PWINDOWSURFACE)
        return; // Don't focus when already focused on this.

    // Unfocus last window
    if (m_pLastFocus && windowValidMapped(m_pLastFocus))
        g_pXWaylandManager->activateSurface(g_pXWaylandManager->getWindowSurface(m_pLastFocus), false);

    const auto KEYBOARD = wlr_seat_get_keyboard(m_sWLRSeat);
    wlr_seat_keyboard_notify_enter(m_sWLRSeat, PWINDOWSURFACE, KEYBOARD->keycodes, KEYBOARD->num_keycodes, &KEYBOARD->modifiers);

    g_pXWaylandManager->activateSurface(PWINDOWSURFACE, true);
    
    m_pLastFocus = pWindow;

    Debug::log(LOG, "Set keyboard %x focus to %x, with name: %s", KEYBOARD, pWindow, pWindow->m_szTitle.c_str());
}

bool CCompositor::windowValidMapped(CWindow* pWindow) {
    if (!windowExists(pWindow))
        return false;

    if (pWindow->m_bIsX11 && !pWindow->m_bMappedX11)
        return false;

    if (!g_pXWaylandManager->getWindowSurface(pWindow))
        return false;

    return true;
}