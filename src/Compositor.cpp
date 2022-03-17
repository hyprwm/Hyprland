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

    m_sWLRAllocator = wlr_allocator_autocreate(m_sWLRBackend, m_sWLRRenderer);

    if (!m_sWLRAllocator) {
        Debug::log(CRIT, "m_sWLRAllocator was NULL!");
        RIP("m_sWLRAllocator NULL!");
        return;
    }


    m_sWLRCompositor = wlr_compositor_create(m_sWLDisplay, m_sWLRRenderer);
    wlr_export_dmabuf_manager_v1_create(m_sWLDisplay);
    wlr_screencopy_manager_v1_create(m_sWLDisplay);
    wlr_data_control_manager_v1_create(m_sWLDisplay);
    wlr_data_device_manager_create(m_sWLDisplay);
    wlr_gamma_control_manager_v1_create(m_sWLDisplay);
    wlr_primary_selection_v1_device_manager_create(m_sWLDisplay);
    wlr_viewporter_create(m_sWLDisplay);

    m_sWLRXDGActivation - wlr_xdg_activation_v1_create(m_sWLDisplay);
    m_sWLROutputLayout = wlr_output_layout_create();

    wl_signal_add(&m_sWLRXDGActivation->events.request_activate, &Events::listen_activate);
    wl_signal_add(&m_sWLROutputLayout->events.change, &Events::listen_change);
    wl_signal_add(&m_sWLRBackend->events.new_output, &Events::listen_newOutput);

    m_sWLRIdle = wlr_idle_create(m_sWLDisplay);
    m_sWLRLayerShell = wlr_layer_shell_v1_create(m_sWLDisplay);
    m_sWLRXDGShell = wlr_xdg_shell_create(m_sWLDisplay);

    wl_signal_add(&m_sWLRLayerShell->events.new_surface, &Events::listen_newLayerSurface);
    wl_signal_add(&m_sWLRXDGShell->events.new_surface, &Events::listen_newXDGSurface);

    wlr_server_decoration_manager_set_default_mode(wlr_server_decoration_manager_create(m_sWLDisplay), WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
    wlr_xdg_decoration_manager_v1_create(m_sWLDisplay);

    m_sWLRCursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(m_sWLRCursor, m_sWLROutputLayout);

    m_sWLRXCursorMgr = wlr_xcursor_manager_create(NULL, 24);

    m_sWLRVKeyboardMgr = wlr_virtual_keyboard_manager_v1_create(m_sWLDisplay);

    m_sWLRSeat = wlr_seat_create(m_sWLDisplay, "seat0");

    m_sWLROutputMgr = wlr_output_manager_v1_create(m_sWLDisplay);

    m_sWLRPresentation = wlr_presentation_create(m_sWLDisplay, m_sWLRBackend);

    wl_signal_add(&m_sWLRCursor->events.motion, &Events::listen_mouseMove);
    wl_signal_add(&m_sWLRCursor->events.motion_absolute, &Events::listen_mouseMoveAbsolute);
    wl_signal_add(&m_sWLRCursor->events.button, &Events::listen_mouseButton);
    wl_signal_add(&m_sWLRCursor->events.axis, &Events::listen_mouseAxis);
    wl_signal_add(&m_sWLRCursor->events.frame, &Events::listen_mouseFrame);
    wl_signal_add(&m_sWLRBackend->events.new_input, &Events::listen_newInput);
    wl_signal_add(&m_sWLRVKeyboardMgr->events.new_virtual_keyboard, &Events::listen_newKeyboard);
    wl_signal_add(&m_sWLRSeat->events.request_set_cursor, &Events::listen_requestMouse);
    wl_signal_add(&m_sWLRSeat->events.request_set_selection, &Events::listen_requestSetSel);
    wl_signal_add(&m_sWLRSeat->events.request_set_primary_selection, &Events::listen_requestSetPrimarySel);
    wl_signal_add(&m_sWLROutputMgr->events.apply, &Events::listen_outputMgrApply);
    wl_signal_add(&m_sWLROutputMgr->events.test, &Events::listen_outputMgrTest);

    // TODO: XWayland
}

CCompositor::~CCompositor() {

}

void CCompositor::startCompositor() {
    m_szWLDisplaySocket = wl_display_add_socket_auto(m_sWLDisplay);

    if (!m_szWLDisplaySocket) {
        Debug::log(CRIT, "m_szWLDisplaySocket NULL!");
        RIP("m_szWLDisplaySocket NULL!");
    }

    setenv("WAYLAND_DISPLAY", m_szWLDisplaySocket, 1);

    signal(SIGPIPE, SIG_IGN);

    Debug::log(LOG, "Running on WAYLAND_DISPLAY: %s", m_szWLDisplaySocket);

    if (!wlr_backend_start(m_sWLRBackend)) {
        Debug::log(CRIT, "Backend did not start!");
        RIP("Backend did not start!");
    }

    wlr_xcursor_manager_set_cursor_image(m_sWLRXCursorMgr, "left_ptr", m_sWLRCursor);

    Debug::log(LOG, "Creating the config manager!");
    g_pConfigManager = std::make_unique<CConfigManager>();

    Debug::log(LOG, "Creating the ManagerThread!");
    g_pManagerThread = std::make_unique<CManagerThread>();

    Debug::log(LOG, "Creating the InputManager!");
    g_pInputManager = std::make_unique<CInputManager>();

    // This blocks until we are done.
    Debug::log(LOG, "Hyprland is ready, running the event loop!");
    wl_display_run(m_sWLDisplay);
}