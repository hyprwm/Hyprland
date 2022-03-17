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

    m_sWLROutputLayout = wlr_output_layout_create();

    wl_signal_add(&m_sWLRBackend->events.new_output, &Events::listen_newOutput);

    m_sWLRScene = wlr_scene_create();
    wlr_scene_attach_output_layout(m_sWLRScene, m_sWLROutputLayout);

    m_sWLRXDGShell = wlr_xdg_shell_create(m_sWLDisplay);
    wl_signal_add(&m_sWLRXDGShell->events.new_surface, &Events::listen_newXDGSurface);

    m_sWLRCursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(m_sWLRCursor, m_sWLROutputLayout);

    m_sWLRXCursorMgr = wlr_xcursor_manager_create(nullptr, 24);
    wlr_xcursor_manager_load(m_sWLRXCursorMgr, 1);

    wl_signal_add(&m_sWLRCursor->events.motion, &Events::listen_mouseMove);
    wl_signal_add(&m_sWLRCursor->events.motion_absolute, &Events::listen_mouseMoveAbsolute);
    wl_signal_add(&m_sWLRCursor->events.button, &Events::listen_mouseButton);
    wl_signal_add(&m_sWLRCursor->events.axis, &Events::listen_mouseAxis);
    wl_signal_add(&m_sWLRCursor->events.frame, &Events::listen_mouseFrame);

    m_sWLRSeat = wlr_seat_create(m_sWLDisplay, "seat0");

    wl_signal_add(&m_sWLRBackend->events.new_input, &Events::listen_newInput);
    wl_signal_add(&m_sWLRSeat->events.request_set_cursor, &Events::listen_requestMouse);
    wl_signal_add(&m_sWLRSeat->events.request_set_selection, &Events::listen_requestSetSel);

    // TODO: XWayland
}

CCompositor::~CCompositor() {

}

void CCompositor::startCompositor() {

    // Init all the managers BEFORE we start with the wayland server so that ALL of the stuff is initialized
    // properly and we dont get any bad mem reads.
    //
    Debug::log(LOG, "Creating the config manager!");
    g_pConfigManager = std::make_unique<CConfigManager>();

    Debug::log(LOG, "Creating the ManagerThread!");
    g_pManagerThread = std::make_unique<CManagerThread>();

    Debug::log(LOG, "Creating the InputManager!");
    g_pInputManager = std::make_unique<CInputManager>();
    //
    //

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