#include "Compositor.hpp"
#include "helpers/Splashes.hpp"
#include <random>
#include "debug/HyprCtl.hpp"

int handleCritSignal(int signo, void* data) {
    Debug::log(LOG, "Hyprland received signal %d", signo);

    if (signo == SIGTERM || signo == SIGINT || signo == SIGKILL) {
        g_pCompositor->cleanup();
    }

    return 0; // everything went fine
}

CCompositor::CCompositor() {
    wlr_log_init(WLR_INFO, NULL);

    m_iHyprlandPID = getpid();

    m_szInstanceSignature = GIT_COMMIT_HASH + std::string("_") + std::to_string(time(NULL));

    setenv("HYPRLAND_INSTANCE_SIGNATURE", m_szInstanceSignature.c_str(), true);

    const auto INSTANCEPATH = "/tmp/hypr/" + m_szInstanceSignature;
    mkdir(INSTANCEPATH.c_str(), S_IRWXU | S_IRWXG);

    Debug::init(m_szInstanceSignature);

    Debug::log(LOG, "Instance Signature: %s", m_szInstanceSignature.c_str());

    Debug::log(LOG, "Hyprland PID: %i", m_iHyprlandPID);

    Debug::log(LOG, "===== SYSTEM INFO: =====");

    logSystemInfo();

    Debug::log(LOG, "========================");

    Debug::log(NONE, "\n\n"); // pad

    Debug::log(INFO, "If you are crashing, or encounter any bugs, please consult https://github.com/hyprwm/Hyprland/wiki/Crashing-and-bugs\n\n");

    setRandomSplash();

    Debug::log(LOG, "\nCurrent splash: %s\n\n", m_szCurrentSplash.c_str());

    m_sWLDisplay = wl_display_create();

    m_sWLEventLoop = wl_display_get_event_loop(m_sWLDisplay);

    // register crit signal handler
    wl_event_loop_add_signal(m_sWLEventLoop, SIGTERM, handleCritSignal, nullptr);
    //wl_event_loop_add_signal(m_sWLEventLoop, SIGINT, handleCritSignal, nullptr);

    m_sWLRBackend = wlr_backend_autocreate(m_sWLDisplay);

    if (!m_sWLRBackend) {
        Debug::log(CRIT, "m_sWLRBackend was NULL!");
        throw std::runtime_error("wlr_backend_autocreate() failed!");
    }

    m_iDRMFD = wlr_backend_get_drm_fd(m_sWLRBackend);
    if (m_iDRMFD < 0) {
        Debug::log(CRIT, "Couldn't query the DRM FD!");
        throw std::runtime_error("wlr_backend_get_drm_fd() failed!");
    }

    m_sWLRRenderer = wlr_gles2_renderer_create_with_drm_fd(m_iDRMFD);

    if (!m_sWLRRenderer) {
        Debug::log(CRIT, "m_sWLRRenderer was NULL!");
        throw std::runtime_error("wlr_gles2_renderer_create_with_drm_fd() failed!");
    }

    wlr_renderer_init_wl_display(m_sWLRRenderer, m_sWLDisplay);

    m_sWLRAllocator = wlr_allocator_autocreate(m_sWLRBackend, m_sWLRRenderer);

    if (!m_sWLRAllocator) {
        Debug::log(CRIT, "m_sWLRAllocator was NULL!");
        throw std::runtime_error("wlr_allocator_autocreate() failed!");
    }

    m_sWLREGL = wlr_gles2_renderer_get_egl(m_sWLRRenderer);

    if (!m_sWLREGL) {
        Debug::log(CRIT, "m_sWLREGL was NULL!");
        throw std::runtime_error("wlr_gles2_renderer_get_egl() failed!");
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

    m_sWLROutputPowerMgr = wlr_output_power_manager_v1_create(m_sWLDisplay);

    m_sWLRScene = wlr_scene_create();
    wlr_scene_attach_output_layout(m_sWLRScene, m_sWLROutputLayout);

    m_sWLRXDGShell = wlr_xdg_shell_create(m_sWLDisplay, 5);

    m_sWLRCursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(m_sWLRCursor, m_sWLROutputLayout);

    m_sWLRXCursorMgr = wlr_xcursor_manager_create(nullptr, 24);
    wlr_xcursor_manager_load(m_sWLRXCursorMgr, 1);

    if (const auto XCURSORENV = getenv("XCURSOR_SIZE"); !XCURSORENV || std::string(XCURSORENV).empty())
        setenv("XCURSOR_SIZE", "24", true);

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

    m_sWLRToplevelMgr = wlr_foreign_toplevel_manager_v1_create(m_sWLDisplay);

    m_sWRLDRMLeaseMgr = wlr_drm_lease_v1_manager_create(m_sWLDisplay, m_sWLRBackend);
    if (!m_sWRLDRMLeaseMgr) {
        Debug::log(INFO, "Failed to create wlr_drm_lease_v1_manager");
        Debug::log(INFO, "VR will not be available");
    }

    m_sWLRTabletManager = wlr_tablet_v2_create(m_sWLDisplay);

    m_sWLRForeignRegistry = wlr_xdg_foreign_registry_create(m_sWLDisplay);

    m_sWLRIdleInhibitMgr = wlr_idle_inhibit_v1_create(m_sWLDisplay);

    wlr_xdg_foreign_v1_create(m_sWLDisplay, m_sWLRForeignRegistry);
    wlr_xdg_foreign_v2_create(m_sWLDisplay, m_sWLRForeignRegistry);

    m_sWLRPointerGestures = wlr_pointer_gestures_v1_create(m_sWLDisplay);

    m_sWLRSession = wlr_backend_get_session(m_sWLRBackend);

    m_sWLRTextInputMgr = wlr_text_input_manager_v3_create(m_sWLDisplay);

    m_sWLRIMEMgr = wlr_input_method_manager_v2_create(m_sWLDisplay);

    m_sWLRActivation = wlr_xdg_activation_v1_create(m_sWLDisplay);
}

CCompositor::~CCompositor() {
    cleanup();
}

void CCompositor::setRandomSplash() {
    std::random_device dev;
    std::mt19937 engine(dev());
    std::uniform_int_distribution<> distribution(0, SPLASHES.size() - 1);

    m_szCurrentSplash = SPLASHES[distribution(engine)];
}

void CCompositor::initAllSignals() {
    addWLSignal(&m_sWLRBackend->events.new_output, &Events::listen_newOutput, m_sWLRBackend, "Backend");
    addWLSignal(&m_sWLRXDGShell->events.new_surface, &Events::listen_newXDGSurface, m_sWLRXDGShell, "XDG Shell");
    addWLSignal(&m_sWLRCursor->events.motion, &Events::listen_mouseMove, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.motion_absolute, &Events::listen_mouseMoveAbsolute, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.button, &Events::listen_mouseButton, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.axis, &Events::listen_mouseAxis, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.frame, &Events::listen_mouseFrame, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.swipe_begin, &Events::listen_swipeBegin, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.swipe_update, &Events::listen_swipeUpdate, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.swipe_end, &Events::listen_swipeEnd, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.pinch_begin, &Events::listen_pinchBegin, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.pinch_update, &Events::listen_pinchUpdate, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.pinch_end, &Events::listen_pinchEnd, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.touch_down, &Events::listen_touchBegin, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.touch_up, &Events::listen_touchEnd, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.touch_motion, &Events::listen_touchUpdate, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.touch_frame, &Events::listen_touchFrame, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.hold_begin, &Events::listen_holdBegin, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRCursor->events.hold_end, &Events::listen_holdEnd, m_sWLRCursor, "WLRCursor");
    addWLSignal(&m_sWLRBackend->events.new_input, &Events::listen_newInput, m_sWLRBackend, "Backend");
    addWLSignal(&m_sSeat.seat->events.request_set_cursor, &Events::listen_requestMouse, &m_sSeat, "Seat");
    addWLSignal(&m_sSeat.seat->events.request_set_selection, &Events::listen_requestSetSel, &m_sSeat, "Seat");
    addWLSignal(&m_sSeat.seat->events.request_start_drag, &Events::listen_requestDrag, &m_sSeat, "Seat");
    addWLSignal(&m_sSeat.seat->events.start_drag, &Events::listen_startDrag, &m_sSeat, "Seat");
    addWLSignal(&m_sSeat.seat->events.request_set_selection, &Events::listen_requestSetSel, &m_sSeat, "Seat");
    addWLSignal(&m_sSeat.seat->events.request_set_primary_selection, &Events::listen_requestSetPrimarySel, &m_sSeat, "Seat");
    addWLSignal(&m_sWLRLayerShell->events.new_surface, &Events::listen_newLayerSurface, m_sWLRLayerShell, "LayerShell");
    addWLSignal(&m_sWLROutputLayout->events.change, &Events::listen_change, m_sWLROutputLayout, "OutputLayout");
    addWLSignal(&m_sWLROutputMgr->events.apply, &Events::listen_outputMgrApply, m_sWLROutputMgr, "OutputMgr");
    addWLSignal(&m_sWLROutputMgr->events.test, &Events::listen_outputMgrTest, m_sWLROutputMgr, "OutputMgr");
    addWLSignal(&m_sWLRInhibitMgr->events.activate, &Events::listen_InhibitActivate, m_sWLRInhibitMgr, "InhibitMgr");
    addWLSignal(&m_sWLRInhibitMgr->events.deactivate, &Events::listen_InhibitDeactivate, m_sWLRInhibitMgr, "InhibitMgr");
    addWLSignal(&m_sWLRPointerConstraints->events.new_constraint, &Events::listen_newConstraint, m_sWLRPointerConstraints, "PointerConstraints");
    addWLSignal(&m_sWLRXDGDecoMgr->events.new_toplevel_decoration, &Events::listen_NewXDGDeco, m_sWLRXDGDecoMgr, "XDGDecoMgr");
    addWLSignal(&m_sWLRVirtPtrMgr->events.new_virtual_pointer, &Events::listen_newVirtPtr, m_sWLRVirtPtrMgr, "VirtPtrMgr");
    addWLSignal(&m_sWLRVKeyboardMgr->events.new_virtual_keyboard, &Events::listen_newVirtualKeyboard, m_sWLRVKeyboardMgr, "VKeyboardMgr");
    addWLSignal(&m_sWLRRenderer->events.destroy, &Events::listen_RendererDestroy, m_sWLRRenderer, "WLRRenderer");
    addWLSignal(&m_sWLRIdleInhibitMgr->events.new_inhibitor, &Events::listen_newIdleInhibitor, m_sWLRIdleInhibitMgr, "WLRIdleInhibitMgr");
    addWLSignal(&m_sWLROutputPowerMgr->events.set_mode, &Events::listen_powerMgrSetMode, m_sWLROutputPowerMgr, "PowerMgr");
    addWLSignal(&m_sWLRIMEMgr->events.input_method, &Events::listen_newIME, m_sWLRIMEMgr, "IMEMgr");
    addWLSignal(&m_sWLRTextInputMgr->events.text_input, &Events::listen_newTextInput, m_sWLRTextInputMgr, "TextInputMgr");
    addWLSignal(&m_sWLRActivation->events.request_activate, &Events::listen_activateXDG, m_sWLRActivation, "ActivationV1");

    if(m_sWRLDRMLeaseMgr)
        addWLSignal(&m_sWRLDRMLeaseMgr->events.request, &Events::listen_leaseRequest, &m_sWRLDRMLeaseMgr, "DRM");

    if (m_sWLRSession)
        addWLSignal(&m_sWLRSession->events.active, &Events::listen_sessionActive, m_sWLRSession, "Session");
}

void CCompositor::cleanup() {
    if (!m_sWLDisplay || m_bIsShuttingDown)
        return;

    m_bIsShuttingDown = true;

    m_pLastFocus = nullptr;
    m_pLastWindow = nullptr;

    // accumulate all PIDs for killing, also request closing.
    for (auto& w : m_vWindows) {
        if (w->m_bIsMapped && !w->isHidden())
            m_dProcessPIDsOnShutdown.push_back(w->getPID());
    }

    // end threads
    g_pEventManager->m_tThread = std::thread();

    m_vWorkspaces.clear();
    m_vWindows.clear();

    for (auto& m : m_vMonitors) {
        g_pHyprOpenGL->destroyMonitorResources(m.get());

        wlr_output_enable(m->output, false);
        wlr_output_commit(m->output);
    }

    m_vMonitors.clear();

    if (g_pXWaylandManager->m_sWLRXWayland) {
        wlr_xwayland_destroy(g_pXWaylandManager->m_sWLRXWayland);
        g_pXWaylandManager->m_sWLRXWayland = nullptr;
    }

    wl_display_terminate(m_sWLDisplay);

    g_pKeybindManager->spawn("sleep 5 && kill -9 " + std::to_string(m_iHyprlandPID));  // this is to prevent that random "freezing"
                                                                                       // the PID should not be reused.
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

    Debug::log(LOG, "Creating the LayoutManager!");
    g_pLayoutManager = std::make_unique<CLayoutManager>();

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

    Debug::log(LOG, "Creating the EventManager!");
    g_pEventManager = std::make_unique<CEventManager>();
    g_pEventManager->startThread();

    Debug::log(LOG, "Creating the HyprDebugOverlay!");
    g_pDebugOverlay = std::make_unique<CHyprDebugOverlay>();
    //
    //

    // firefox wont detect wl
    setenv("MOZ_ENABLE_WAYLAND", "1", 1);

    setenv("XDG_CURRENT_DESKTOP", "Hyprland", 1);

    initAllSignals();

    m_szWLDisplaySocket = wl_display_add_socket_auto(m_sWLDisplay);

    if (!m_szWLDisplaySocket) {
        Debug::log(CRIT, "m_szWLDisplaySocket NULL!");
        wlr_backend_destroy(m_sWLRBackend);
        throw std::runtime_error("m_szWLDisplaySocket was null! (wl_display_add_socket_auto failed)");
    }

    setenv("WAYLAND_DISPLAY", m_szWLDisplaySocket, 1);

    signal(SIGPIPE, SIG_IGN);

    Debug::log(LOG, "Running on WAYLAND_DISPLAY: %s", m_szWLDisplaySocket);

    if (!wlr_backend_start(m_sWLRBackend)) {
        Debug::log(CRIT, "Backend did not start!");
        wlr_backend_destroy(m_sWLRBackend);
        wl_display_destroy(m_sWLDisplay);
        throw std::runtime_error("The backend could not start!");
    }

    wlr_xcursor_manager_set_cursor_image(m_sWLRXCursorMgr, "left_ptr", m_sWLRCursor);

    // This blocks until we are done.
    Debug::log(LOG, "Hyprland is ready, running the event loop!");
    wl_display_run(m_sWLDisplay);
}

CMonitor* CCompositor::getMonitorFromID(const int& id) {
    for (auto& m : m_vMonitors) {
        if (m->ID == (uint64_t)id) {
            return m.get();
        }
    }

    return nullptr;
}

CMonitor* CCompositor::getMonitorFromName(const std::string& name) {
    for (auto& m : m_vMonitors) {
        if (m->szName == name) {
            return m.get();
        }
    }

    return nullptr;
}

CMonitor* CCompositor::getMonitorFromCursor() {
    const auto COORDS = Vector2D(m_sWLRCursor->x, m_sWLRCursor->y);

    return getMonitorFromVector(COORDS);
}

CMonitor* CCompositor::getMonitorFromVector(const Vector2D& point) {
    const auto OUTPUT = wlr_output_layout_output_at(m_sWLROutputLayout, point.x, point.y);

    if (!OUTPUT) {
        float bestDistance = 0.f;
        CMonitor* pBestMon = nullptr;

        for (auto& m : m_vMonitors) {
            float dist = vecToRectDistanceSquared(point, m->vecPosition, m->vecPosition + m->vecSize);

            if (dist < bestDistance || !pBestMon) {
                bestDistance = dist;
                pBestMon = m.get();
            }
        }

        if (!pBestMon) { // ?????
            Debug::log(WARN, "getMonitorFromVector no close mon???");
            return m_vMonitors.front().get();
        }

        return pBestMon;
    }

    return getMonitorFromOutput(OUTPUT);
}

void CCompositor::removeWindowFromVectorSafe(CWindow* pWindow) {
    if (windowExists(pWindow) && !pWindow->m_bFadingOut){
        if (pWindow->m_bIsX11 && pWindow->m_iX11Type == 2) {
            m_dUnmanagedX11Windows.erase(std::remove_if(m_dUnmanagedX11Windows.begin(), m_dUnmanagedX11Windows.end(), [&](std::unique_ptr<CWindow>& el) { return el.get() == pWindow; }));
        }

        // if X11, also check its children
        // and delete any needed
        if (pWindow->m_bIsX11) {
            for (auto& w : m_vWindows) {
                if (!w->m_bIsX11)
                    continue;

                if (w->m_pX11Parent == pWindow)
                    m_vWindows.erase(std::remove_if(m_vWindows.begin(), m_vWindows.end(), [&](std::unique_ptr<CWindow>& el) { return el.get() == w.get(); }));
            }

            for (auto& w : m_dUnmanagedX11Windows) {
                if (w->m_pX11Parent == pWindow)
                    m_dUnmanagedX11Windows.erase(std::remove_if(m_dUnmanagedX11Windows.begin(), m_dUnmanagedX11Windows.end(), [&](std::unique_ptr<CWindow>& el) { return el.get() == w.get(); }));
            }
        }

        m_vWindows.erase(std::remove_if(m_vWindows.begin(), m_vWindows.end(), [&](std::unique_ptr<CWindow>& el) { return el.get() == pWindow; }));
    }
}

bool CCompositor::windowExists(CWindow* pWindow) {
    for (auto& w : m_vWindows) {
        if (w.get() == pWindow)
            return true;
    }

    return false;
}

CWindow* CCompositor::vectorToWindow(const Vector2D& pos) {
    const auto PMONITOR = getMonitorFromVector(pos);

    if (PMONITOR->specialWorkspaceOpen) {
        for (auto w = m_vWindows.rbegin(); w != m_vWindows.rend(); w++) {
            wlr_box box = {(*w)->m_vRealPosition.vec().x, (*w)->m_vRealPosition.vec().y, (*w)->m_vRealSize.vec().x, (*w)->m_vRealSize.vec().y};
            if ((*w)->m_bIsFloating && (*w)->m_iWorkspaceID == SPECIAL_WORKSPACE_ID && (*w)->m_bIsMapped && wlr_box_contains_point(&box, pos.x, pos.y) && !(*w)->isHidden())
                return (*w).get();
        }

        for (auto& w : m_vWindows) {
            wlr_box box = {w->m_vRealPosition.vec().x, w->m_vRealPosition.vec().y, w->m_vRealSize.vec().x, w->m_vRealSize.vec().y};
            if (w->m_iWorkspaceID == SPECIAL_WORKSPACE_ID && wlr_box_contains_point(&box, pos.x, pos.y) && w->m_bIsMapped && !w->m_bIsFloating && !w->isHidden())
                return w.get();
        }
    }

    // pinned
    for (auto w = m_vWindows.rbegin(); w != m_vWindows.rend(); w++) {
        wlr_box box = {(*w)->m_vRealPosition.vec().x, (*w)->m_vRealPosition.vec().y, (*w)->m_vRealSize.vec().x, (*w)->m_vRealSize.vec().y};
        if (wlr_box_contains_point(&box, pos.x, pos.y) && (*w)->m_bIsMapped && (*w)->m_bIsFloating && !(*w)->isHidden() && (*w)->m_bPinned)
            return w->get();
    }

    // first loop over floating cuz they're above, m_vWindows should be sorted bottom->top, for tiled it doesn't matter.
    for (auto w = m_vWindows.rbegin(); w != m_vWindows.rend(); w++) {
        wlr_box box = {(*w)->m_vRealPosition.vec().x, (*w)->m_vRealPosition.vec().y, (*w)->m_vRealSize.vec().x, (*w)->m_vRealSize.vec().y};
        if (wlr_box_contains_point(&box, pos.x, pos.y) && (*w)->m_bIsMapped && (*w)->m_bIsFloating && isWorkspaceVisible((*w)->m_iWorkspaceID) && !(*w)->isHidden() && !(*w)->m_bPinned)
            return w->get();
    }

    for (auto& w : m_vWindows) {
        wlr_box box = {w->m_vRealPosition.vec().x, w->m_vRealPosition.vec().y, w->m_vRealSize.vec().x, w->m_vRealSize.vec().y};
        if (wlr_box_contains_point(&box, pos.x, pos.y) && w->m_bIsMapped && !w->m_bIsFloating && PMONITOR->activeWorkspace == w->m_iWorkspaceID && !w->isHidden())
            return w.get();
    }

    return nullptr;
}

CWindow* CCompositor::vectorToWindowTiled(const Vector2D& pos) {
    const auto PMONITOR = getMonitorFromVector(pos);

    if (PMONITOR->specialWorkspaceOpen) {
        for (auto& w : m_vWindows) {
            wlr_box box = {w->m_vPosition.x, w->m_vPosition.y, w->m_vSize.x, w->m_vSize.y};
            if (w->m_iWorkspaceID == SPECIAL_WORKSPACE_ID && wlr_box_contains_point(&box, pos.x, pos.y) && !w->m_bIsFloating && !w->isHidden())
                return w.get();
        }
    }

    for (auto& w : m_vWindows) {
        wlr_box box = {w->m_vPosition.x, w->m_vPosition.y, w->m_vSize.x, w->m_vSize.y};
        if (w->m_bIsMapped && wlr_box_contains_point(&box, pos.x, pos.y) && w->m_iWorkspaceID == PMONITOR->activeWorkspace && !w->m_bIsFloating && !w->isHidden())
            return w.get();
    }

    return nullptr;
}

void findExtensionForVector2D(wlr_surface* surface, int x, int y, void* data) {
    const auto DATA = (SExtensionFindingData*)data;

    wlr_box box = {DATA->origin.x + x, DATA->origin.y + y, surface->current.width, surface->current.height};

    if (wlr_box_contains_point(&box, DATA->vec.x, DATA->vec.y))
        *DATA->found = surface;
}

CWindow* CCompositor::vectorToWindowIdeal(const Vector2D& pos) {
    const auto PMONITOR = getMonitorFromVector(pos);

    // special workspace
    if (PMONITOR->specialWorkspaceOpen) {
        for (auto w = m_vWindows.rbegin(); w != m_vWindows.rend(); w++) {
            wlr_box box = {(*w)->m_vRealPosition.vec().x, (*w)->m_vRealPosition.vec().y, (*w)->m_vRealSize.vec().x, (*w)->m_vRealSize.vec().y};
            if ((*w)->m_bIsFloating && (*w)->m_iWorkspaceID == SPECIAL_WORKSPACE_ID && (*w)->m_bIsMapped && wlr_box_contains_point(&box, pos.x, pos.y) && !(*w)->isHidden() && !(*w)->m_bX11ShouldntFocus)
                return (*w).get();
        }

        for (auto& w : m_vWindows) {
            wlr_box box = {w->m_vPosition.x, w->m_vPosition.y, w->m_vSize.x, w->m_vSize.y};
            if (!w->m_bIsFloating && w->m_iWorkspaceID == SPECIAL_WORKSPACE_ID && w->m_bIsMapped && wlr_box_contains_point(&box, pos.x, pos.y) && !w->isHidden() && !w->m_bX11ShouldntFocus)
                return w.get();
        }
    }

    // pinned windows on top of floating regardless
    for (auto w = m_vWindows.rbegin(); w != m_vWindows.rend(); w++) {
        wlr_box box = {(*w)->m_vRealPosition.vec().x, (*w)->m_vRealPosition.vec().y, (*w)->m_vRealSize.vec().x, (*w)->m_vRealSize.vec().y};
        if ((*w)->m_bIsFloating && (*w)->m_bIsMapped && !(*w)->isHidden() && !(*w)->m_bX11ShouldntFocus && (*w)->m_bPinned) {
            if (wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y))
                return w->get();

            if (!(*w)->m_bIsX11) {
                wlr_surface* resultSurf = nullptr;
                Vector2D origin = (*w)->m_vRealPosition.vec();
                SExtensionFindingData data = {origin, pos, &resultSurf};
                wlr_xdg_surface_for_each_popup_surface((*w)->m_uSurface.xdg, findExtensionForVector2D, &data);

                if (resultSurf)
                    return w->get();
            }
        }
    }

    // first loop over floating cuz they're above, m_lWindows should be sorted bottom->top, for tiled it doesn't matter.
    for (auto w = m_vWindows.rbegin(); w != m_vWindows.rend(); w++) {
        wlr_box box = {(*w)->m_vRealPosition.vec().x, (*w)->m_vRealPosition.vec().y, (*w)->m_vRealSize.vec().x, (*w)->m_vRealSize.vec().y};
        if ((*w)->m_bIsFloating && (*w)->m_bIsMapped && isWorkspaceVisible((*w)->m_iWorkspaceID) && !(*w)->isHidden() && !(*w)->m_bPinned) {
            // OR windows should add focus to parent
            if ((*w)->m_bX11ShouldntFocus && (*w)->m_iX11Type != 2)
                continue;

            if (wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y)) {

                if ((*w)->m_iX11Type == 2) {
                    // Override Redirect
                    return g_pCompositor->m_pLastWindow; // we kinda trick everything here. 
                                                         // TODO: this is wrong, we should focus the parent, but idk how to get it considering it's nullptr in most cases.
                }

                return w->get();
            }

            if (!(*w)->m_bIsX11) {
                wlr_surface* resultSurf = nullptr;
                Vector2D origin =(*w)->m_vRealPosition.vec();
                SExtensionFindingData data = {origin, pos, &resultSurf};
                wlr_xdg_surface_for_each_popup_surface((*w)->m_uSurface.xdg, findExtensionForVector2D, &data);

                if (resultSurf)
                    return w->get();
            }
        }
    }

    // for windows, we need to check their extensions too, first.
    for (auto& w : m_vWindows) {
        if (!w->m_bIsX11 && !w->m_bIsFloating && w->m_bIsMapped && w->m_iWorkspaceID == PMONITOR->activeWorkspace && !w->isHidden() && !w->m_bX11ShouldntFocus) {
            wlr_surface* resultSurf = nullptr;
            Vector2D origin = w->m_vRealPosition.vec();
            SExtensionFindingData data = {origin, pos, &resultSurf};
            wlr_xdg_surface_for_each_popup_surface(w->m_uSurface.xdg, findExtensionForVector2D, &data);

            if (resultSurf)
                return w.get();
        }
    }
    for (auto& w : m_vWindows) {
        wlr_box box = {w->m_vPosition.x, w->m_vPosition.y, w->m_vSize.x, w->m_vSize.y};
        if (!w->m_bIsFloating && w->m_bIsMapped && wlr_box_contains_point(&box, pos.x, pos.y) && w->m_iWorkspaceID == PMONITOR->activeWorkspace && !w->isHidden() && !w->m_bX11ShouldntFocus)
            return w.get();
    }

    return nullptr;
}

CWindow* CCompositor::windowFromCursor() {
    const auto PMONITOR = getMonitorFromCursor();

    if (PMONITOR->specialWorkspaceOpen) {
        for (auto w = m_vWindows.rbegin(); w != m_vWindows.rend(); w++) {
            wlr_box box = {(*w)->m_vRealPosition.vec().x, (*w)->m_vRealPosition.vec().y, (*w)->m_vRealSize.vec().x, (*w)->m_vRealSize.vec().y};
            if ((*w)->m_bIsFloating && (*w)->m_iWorkspaceID == SPECIAL_WORKSPACE_ID && (*w)->m_bIsMapped && wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y) && !(*w)->isHidden())
                return (*w).get();
        }

        for (auto& w : m_vWindows) {
            wlr_box box = {w->m_vPosition.x, w->m_vPosition.y, w->m_vSize.x, w->m_vSize.y};
            if (w->m_iWorkspaceID == SPECIAL_WORKSPACE_ID && wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y) && w->m_bIsMapped)
                return w.get();
        }
    }

    // pinned
    for (auto w = m_vWindows.rbegin(); w != m_vWindows.rend(); w++) {
        wlr_box box = {(*w)->m_vRealPosition.vec().x, (*w)->m_vRealPosition.vec().y, (*w)->m_vRealSize.vec().x, (*w)->m_vRealSize.vec().y};
        if (wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y) && (*w)->m_bIsMapped && (*w)->m_bIsFloating && (*w)->m_bPinned)
            return w->get();
    }

    // first loop over floating cuz they're above, m_lWindows should be sorted bottom->top, for tiled it doesn't matter.
    for (auto w = m_vWindows.rbegin(); w != m_vWindows.rend(); w++) {
        wlr_box box = {(*w)->m_vRealPosition.vec().x, (*w)->m_vRealPosition.vec().y, (*w)->m_vRealSize.vec().x, (*w)->m_vRealSize.vec().y};
        if (wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y) && (*w)->m_bIsMapped && (*w)->m_bIsFloating && isWorkspaceVisible((*w)->m_iWorkspaceID) && !(*w)->m_bPinned)
            return w->get();
    }

    for (auto& w : m_vWindows) {
        wlr_box box = {w->m_vPosition.x, w->m_vPosition.y, w->m_vSize.x, w->m_vSize.y};
        if (wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y) && w->m_bIsMapped && w->m_iWorkspaceID == PMONITOR->activeWorkspace)
            return w.get();
    }

    return nullptr;
}

CWindow* CCompositor::windowFloatingFromCursor() {
    for (auto w = m_vWindows.rbegin(); w != m_vWindows.rend(); w++) {
        wlr_box box = {(*w)->m_vRealPosition.vec().x, (*w)->m_vRealPosition.vec().y, (*w)->m_vRealSize.vec().x, (*w)->m_vRealSize.vec().y};
        if (wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y) && (*w)->m_bIsMapped && (*w)->m_bIsFloating && !(*w)->isHidden() && (*w)->m_bPinned)
            return w->get();
    }

    for (auto w = m_vWindows.rbegin(); w != m_vWindows.rend(); w++) {
        wlr_box box = {(*w)->m_vRealPosition.vec().x, (*w)->m_vRealPosition.vec().y, (*w)->m_vRealSize.vec().x, (*w)->m_vRealSize.vec().y};
        if (wlr_box_contains_point(&box, m_sWLRCursor->x, m_sWLRCursor->y) && (*w)->m_bIsMapped && (*w)->m_bIsFloating && isWorkspaceVisible((*w)->m_iWorkspaceID) && !(*w)->isHidden() && !(*w)->m_bPinned)
            return w->get();
    }

    return nullptr;
}

wlr_surface* CCompositor::vectorWindowToSurface(const Vector2D& pos, CWindow* pWindow, Vector2D& sl) {

    if (!windowValidMapped(pWindow))
        return nullptr;

    RASSERT(!pWindow->m_bIsX11, "Cannot call vectorWindowToSurface on an X11 window!");

    const auto PSURFACE = pWindow->m_uSurface.xdg;

    double subx, suby;

    // calc for oversized windows... fucking bullshit, again.
    wlr_box geom;
    wlr_xdg_surface_get_geometry(pWindow->m_uSurface.xdg, &geom);

    const auto PFOUND = wlr_xdg_surface_surface_at(PSURFACE, pos.x - pWindow->m_vRealPosition.vec().x + geom.x, pos.y - pWindow->m_vRealPosition.vec().y + geom.y, &subx, &suby);

    if (PFOUND) {
        sl.x = subx;
        sl.y = suby;
        return PFOUND;
    }

    sl.x = pos.x - pWindow->m_vRealPosition.vec().x;
    sl.y = pos.y - pWindow->m_vRealPosition.vec().y;

    sl.x += geom.x;
    sl.y += geom.y;

    return PSURFACE->surface;
}

CMonitor* CCompositor::getMonitorFromOutput(wlr_output* out) {
    for (auto& m : m_vMonitors) {
        if (m->output == out) {
            return m.get();
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
        const auto PLASTWINDOW = m_pLastWindow;
        m_pLastWindow = nullptr;

        if (windowValidMapped(PLASTWINDOW)) {
            updateWindowAnimatedDecorationValues(PLASTWINDOW);

            g_pXWaylandManager->activateWindow(PLASTWINDOW, false);

            if (PLASTWINDOW->m_phForeignToplevel)
                wlr_foreign_toplevel_handle_v1_set_activated(PLASTWINDOW->m_phForeignToplevel, false);
        }

        wlr_seat_keyboard_notify_clear_focus(m_sSeat.seat);

        g_pEventManager->postEvent(SHyprIPCEvent{"activewindow", ","});

        g_pLayoutManager->getCurrentLayout()->onWindowFocusChange(nullptr);

        m_pLastFocus = nullptr;

        g_pInputManager->recheckIdleInhibitorStatus();
        return;
    }

    if (pWindow->m_bNoFocus) {
        Debug::log(LOG, "Ignoring focus to nofocus window!");
        return;
    }

    if (m_pLastWindow == pWindow && m_sSeat.seat->keyboard_state.focused_surface == pSurface)
        return;

    if (pWindow->m_bPinned)
        pWindow->m_iWorkspaceID = m_pLastMonitor->activeWorkspace;

    if (!isWorkspaceVisible(pWindow->m_iWorkspaceID))
        g_pKeybindManager->changeworkspace("[internal]" + std::to_string(pWindow->m_iWorkspaceID));

    const auto PLASTWINDOW = m_pLastWindow;
    m_pLastWindow = pWindow;

    // we need to make the PLASTWINDOW not equal to m_pLastWindow so that RENDERDATA is correct for an unfocused window
    if (windowValidMapped(PLASTWINDOW)) {
        updateWindowAnimatedDecorationValues(PLASTWINDOW);

        if (!pWindow->m_bIsX11 || pWindow->m_iX11Type == 1)
            g_pXWaylandManager->activateWindow(PLASTWINDOW, false);

        if (PLASTWINDOW->m_phForeignToplevel)
            wlr_foreign_toplevel_handle_v1_set_activated(PLASTWINDOW->m_phForeignToplevel, false);
    }

    m_pLastWindow = PLASTWINDOW;

    const auto PWINDOWSURFACE = pSurface ? pSurface : g_pXWaylandManager->getWindowSurface(pWindow);

    focusSurface(PWINDOWSURFACE, pWindow);

    g_pXWaylandManager->activateWindow(pWindow, true); // sets the m_pLastWindow

    // do pointer focus too
    const auto POINTERLOCAL = g_pInputManager->getMouseCoordsInternal() - pWindow->m_vRealPosition.goalv();
    wlr_seat_pointer_notify_enter(m_sSeat.seat, PWINDOWSURFACE, POINTERLOCAL.x, POINTERLOCAL.y);

    updateWindowAnimatedDecorationValues(pWindow);

    // Send an event
    g_pEventManager->postEvent(SHyprIPCEvent{"activewindow", g_pXWaylandManager->getAppIDClass(pWindow) + "," + pWindow->m_szTitle});

    g_pLayoutManager->getCurrentLayout()->onWindowFocusChange(pWindow);

    if (pWindow->m_phForeignToplevel)
        wlr_foreign_toplevel_handle_v1_set_activated(pWindow->m_phForeignToplevel, true);

    if (!pWindow->m_bIsX11) {
        const auto PCONSTRAINT = wlr_pointer_constraints_v1_constraint_for_surface(m_sWLRPointerConstraints, pWindow->m_uSurface.xdg->surface, m_sSeat.seat);

        if (PCONSTRAINT)
            g_pInputManager->constrainMouse(m_sSeat.mouse, PCONSTRAINT);
    }

    g_pInputManager->recheckIdleInhibitorStatus();
}

void CCompositor::focusSurface(wlr_surface* pSurface, CWindow* pWindowOwner) {

    if (m_sSeat.seat->keyboard_state.focused_surface == pSurface || (pWindowOwner && m_sSeat.seat->keyboard_state.focused_surface == g_pXWaylandManager->getWindowSurface(pWindowOwner)))
        return;  // Don't focus when already focused on this.

    // Unfocus last surface if should
    if (m_pLastFocus && !pWindowOwner)
        g_pXWaylandManager->activateSurface(m_pLastFocus, false);

    if (!pSurface) {
        wlr_seat_keyboard_clear_focus(m_sSeat.seat);
        g_pEventManager->postEvent(SHyprIPCEvent{"activewindow", ","}); // unfocused
        g_pInputManager->m_sIMERelay.onKeyboardFocus(nullptr);
        return;
    }


    const auto KEYBOARD = wlr_seat_get_keyboard(m_sSeat.seat);

    if (!KEYBOARD)
        return;

    wlr_seat_keyboard_notify_enter(m_sSeat.seat, pSurface, KEYBOARD->keycodes, KEYBOARD->num_keycodes, &KEYBOARD->modifiers);

    g_pInputManager->m_sIMERelay.onKeyboardFocus(pSurface);

    wlr_seat_keyboard_focus_change_event event = {
        .seat = m_sSeat.seat,
        .old_surface = m_pLastFocus,
        .new_surface = pSurface,
    };
    wl_signal_emit_mutable(&m_sSeat.seat->keyboard_state.events.focus_change, &event);

    if (pWindowOwner)
        Debug::log(LOG, "Set keyboard focus to surface %x, with window name: %s", pSurface, pWindowOwner->m_szTitle.c_str());
    else
        Debug::log(LOG, "Set keyboard focus to surface %x", pSurface);

    g_pXWaylandManager->activateSurface(pSurface, false);
    m_pLastFocus = pSurface;
}

bool CCompositor::windowValidMapped(CWindow* pWindow) {
    if (!pWindow)
        return false;

    if (!windowExists(pWindow))
        return false;

    if (pWindow->m_bIsX11 && !pWindow->m_bMappedX11)
        return false;

    if (!pWindow->m_bIsMapped)
        return false;

    if (pWindow->isHidden())
        return false;

    return true;
}

CWindow* CCompositor::getWindowForPopup(wlr_xdg_popup* popup) {
    for (auto& p : m_vXDGPopups) {
        if (p->popup == popup)
            return p->parentWindow;
    }

    return nullptr;
}

wlr_surface* CCompositor::vectorToLayerSurface(const Vector2D& pos, std::vector<std::unique_ptr<SLayerSurface>>* layerSurfaces, Vector2D* sCoords, SLayerSurface** ppLayerSurfaceFound) {
    for (auto it = layerSurfaces->rbegin(); it != layerSurfaces->rend(); it++) {
        if ((*it)->fadingOut || !(*it)->layerSurface || ((*it)->layerSurface && !(*it)->layerSurface->mapped) || (*it)->alpha.fl() == 0.f)
            continue;

        const auto SURFACEAT = wlr_layer_surface_v1_surface_at((*it)->layerSurface, pos.x - (*it)->geometry.x, pos.y - (*it)->geometry.y, &sCoords->x, &sCoords->y);

        if (SURFACEAT) {
            *ppLayerSurfaceFound = it->get();
            return SURFACEAT;
        }
    }

    return nullptr;
}

CWindow* CCompositor::getWindowFromSurface(wlr_surface* pSurface) {
    for (auto& w : m_vWindows) {
        if (g_pXWaylandManager->getWindowSurface(w.get()) == pSurface)
            return w.get();
    }

    return nullptr;
}

CWindow* CCompositor::getFullscreenWindowOnWorkspace(const int& ID) {
    for (auto& w : m_vWindows) {
        if (w->m_iWorkspaceID == ID && w->m_bIsFullscreen)
            return w.get();
    }

    return nullptr;
}

bool CCompositor::isWorkspaceVisible(const int& w) {
    for (auto& m : m_vMonitors) {
        if (m->activeWorkspace == w)
            return true;

        if (m->specialWorkspaceOpen && w == SPECIAL_WORKSPACE_ID)
            return true;
    }

    return false;
}

CWorkspace* CCompositor::getWorkspaceByID(const int& id) {
    for (auto& w : m_vWorkspaces) {
        if (w->m_iID == id)
            return w.get();
    }

    return nullptr;
}

void CCompositor::sanityCheckWorkspaces() {
    for (auto it = m_vWorkspaces.begin(); it != m_vWorkspaces.end(); ++it) {
        const auto WINDOWSONWORKSPACE = getWindowsOnWorkspace((*it)->m_iID);

        if ((WINDOWSONWORKSPACE == 0 && !isWorkspaceVisible((*it)->m_iID))) {
            it = m_vWorkspaces.erase(it);

            if (it == m_vWorkspaces.end())
                break;

            continue;
        }

        if ((*it)->m_iID == SPECIAL_WORKSPACE_ID && WINDOWSONWORKSPACE == 0) {
            for (auto& m : m_vMonitors) {
                m->specialWorkspaceOpen = false;
            }

            it = m_vWorkspaces.erase(it);

            if (it == m_vWorkspaces.end())
                break;

            continue;
        }
    }
}

int CCompositor::getWindowsOnWorkspace(const int& id) {
    int no = 0;
    for (auto& w : m_vWindows) {
        if (w->m_iWorkspaceID == id && w->m_bIsMapped)
            no++;
    }

    return no;
}

CWindow* CCompositor::getFirstWindowOnWorkspace(const int& id) {
    for (auto& w : m_vWindows) {
        if (w->m_iWorkspaceID == id && w->m_bIsMapped && !w->isHidden())
            return w.get();
    }

    return nullptr;
}

bool CCompositor::doesSeatAcceptInput(wlr_surface* surface) {
    return !m_sSeat.exclusiveClient || (surface && m_sSeat.exclusiveClient == wl_resource_get_client(surface->resource));
}

bool CCompositor::isWindowActive(CWindow* pWindow) {
    if (!m_pLastWindow && !m_pLastFocus)
        return false;

    if (!windowValidMapped(pWindow))
        return false;

    const auto PSURFACE = g_pXWaylandManager->getWindowSurface(pWindow);

    return PSURFACE == m_pLastFocus || pWindow == m_pLastWindow;
}

void CCompositor::moveWindowToTop(CWindow* pWindow) {
    if (!windowValidMapped(pWindow))
        return;

    auto moveToTop = [&](CWindow* pw) -> void {
        for (auto it = m_vWindows.begin(); it != m_vWindows.end(); ++it) {
            if (it->get() == pw) {
                std::rotate(it, it + 1, m_vWindows.end());
                break;
            }
        }
    };

    moveToTop(pWindow);

    if (!pWindow->m_bIsX11)
        return;

    // move all children

    std::deque<CWindow*> toMove;

    for (auto& w : m_vWindows) {
        if (w->m_bIsMapped && w->m_bMappedX11 && !w->isHidden() && w->m_bIsX11 && w->X11TransientFor() == pWindow) {
            toMove.emplace_back(w.get());
        }
    }

    for (auto& pw : toMove) {
        moveToTop(pw);

        moveWindowToTop(pw);
    }
}

void CCompositor::cleanupFadingOut(const int& monid) {
    for (auto& w : m_vWindowsFadingOut) {

        if (w->m_iMonitorID != (long unsigned int)monid)
            continue;

        bool valid = windowExists(w);

        if (!valid || !w->m_bFadingOut || w->m_fAlpha.fl() == 0.f) {
            if (valid && !w->m_bReadyToDelete)
                continue;

            g_pHyprOpenGL->m_mWindowFramebuffers[w].release();
            g_pHyprOpenGL->m_mWindowFramebuffers.erase(w);
            removeWindowFromVectorSafe(w);
            m_vWindowsFadingOut.erase(std::remove(m_vWindowsFadingOut.begin(), m_vWindowsFadingOut.end(), w));

            Debug::log(LOG, "Cleanup: destroyed a window");

            glFlush();  // to free mem NOW.
            return;
        }
    }

    for (auto& ls : m_vSurfacesFadingOut) {

        // sometimes somehow fucking happens wtf
        bool exists = false;
        for (auto& m : m_vMonitors) {
            for (auto& lsl : m->m_aLayerSurfaceLists) {
                for (auto& lsp : lsl) {
                    if (lsp.get() == ls) {
                        exists = true;
                        break;
                    }
                }

                if (exists)
                    break;
            }

            if (exists)
                break;
        }

        if (!exists) {
            m_vSurfacesFadingOut.erase(std::remove(m_vSurfacesFadingOut.begin(), m_vSurfacesFadingOut.end(), ls));

            Debug::log(LOG, "Fading out a non-existent LS??");

            return;
        }

        if (ls->monitorID != monid)
            continue;

        // mark blur for recalc
        if (ls->layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND || ls->layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM)
            g_pHyprOpenGL->markBlurDirtyForMonitor(getMonitorFromID(monid));

        if (ls->fadingOut && ls->readyToDelete && !ls->alpha.isBeingAnimated()) {
            g_pHyprOpenGL->m_mLayerFramebuffers[ls].release();
            g_pHyprOpenGL->m_mLayerFramebuffers.erase(ls);

            for (auto& m : m_vMonitors) {
                for (auto& lsl : m->m_aLayerSurfaceLists) {
                    if (!lsl.empty() && std::find_if(lsl.begin(), lsl.end(), [&](std::unique_ptr<SLayerSurface>& other) { return other.get() == ls; }) != lsl.end()) {
                        lsl.erase(std::remove_if(lsl.begin(), lsl.end(), [&](std::unique_ptr<SLayerSurface>& other) { return other.get() == ls; }));
                    }
                }
            }

            m_vSurfacesFadingOut.erase(std::remove(m_vSurfacesFadingOut.begin(), m_vSurfacesFadingOut.end(), ls));

            Debug::log(LOG, "Cleanup: destroyed a layersurface");

            glFlush();  // to free mem NOW.
            return;
        }
    }
}

void CCompositor::addToFadingOutSafe(SLayerSurface* pLS) {
    const auto FOUND = std::find_if(m_vSurfacesFadingOut.begin(), m_vSurfacesFadingOut.end(), [&](SLayerSurface* other) { return other == pLS; });

    if (FOUND != m_vSurfacesFadingOut.end())
        return;  // if it's already added, don't add it.

    m_vSurfacesFadingOut.emplace_back(pLS);
}

void CCompositor::addToFadingOutSafe(CWindow* pWindow) {
    const auto FOUND = std::find_if(m_vWindowsFadingOut.begin(), m_vWindowsFadingOut.end(), [&](CWindow* other) { return other == pWindow; });

    if (FOUND != m_vWindowsFadingOut.end())
        return;  // if it's already added, don't add it.

    m_vWindowsFadingOut.emplace_back(pWindow);
}

CWindow* CCompositor::getWindowInDirection(CWindow* pWindow, char dir) {

    const auto WINDOWIDEALBB = pWindow->getWindowIdealBoundingBoxIgnoreReserved();

    const auto POSA = Vector2D(WINDOWIDEALBB.x, WINDOWIDEALBB.y);
    const auto SIZEA = Vector2D(WINDOWIDEALBB.width, WINDOWIDEALBB.height);

    auto longestIntersect = -1;
    CWindow* longestIntersectWindow = nullptr;

    for (auto& w : m_vWindows) {
        if (w.get() == pWindow || !w->m_bIsMapped || w->isHidden() || w->m_bIsFloating || !isWorkspaceVisible(w->m_iWorkspaceID))
            continue;

        const auto BWINDOWIDEALBB = w->getWindowIdealBoundingBoxIgnoreReserved();

        const auto POSB = Vector2D(BWINDOWIDEALBB.x, BWINDOWIDEALBB.y);
        const auto SIZEB = Vector2D(BWINDOWIDEALBB.width, BWINDOWIDEALBB.height);

        switch (dir) {
            case 'l':
                if (STICKS(POSA.x, POSB.x + SIZEB.x)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectWindow = w.get();
                    }
                }
                break;
            case 'r':
                if (STICKS(POSA.x + SIZEA.x, POSB.x)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectWindow = w.get();
                    }
                }
                break;
            case 't':
            case 'u':
                if (STICKS(POSA.y, POSB.y + SIZEB.y)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectWindow = w.get();
                    }
                }
                break;
            case 'b':
            case 'd':
                if (STICKS(POSA.y + SIZEA.y, POSB.y)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectWindow = w.get();
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
    for (auto& w : m_vWorkspaces) {
        if (w->m_pWlrHandle && w->m_pWlrHandle != exclude)
            w->setActive(false);
    }
}

CWindow* CCompositor::getNextWindowOnWorkspace(CWindow* pWindow, bool focusableOnly) {
    bool gotToWindow = false;
    for (auto& w : m_vWindows) {
        if (w.get() != pWindow && !gotToWindow)
            continue;

        if (w.get() == pWindow) {
            gotToWindow = true;
            continue;
        }

        if (w->m_iWorkspaceID == pWindow->m_iWorkspaceID && w->m_bIsMapped && !w->isHidden() && (!focusableOnly || !w->m_bNoFocus))
            return w.get();
    }

    for (auto& w : m_vWindows) {
        if (w.get() != pWindow && w->m_iWorkspaceID == pWindow->m_iWorkspaceID && w->m_bIsMapped && !w->isHidden() && (!focusableOnly || !w->m_bNoFocus))
            return w.get();
    }

    return nullptr;
}

CWindow* CCompositor::getPrevWindowOnWorkspace(CWindow* pWindow, bool focusableOnly) {
    bool gotToWindow = false;
    for (auto it = m_vWindows.rbegin(); it != m_vWindows.rend(); it++) {
        if (it->get() != pWindow && !gotToWindow)
            continue;

        if (it->get() == pWindow) {
            gotToWindow = true;
            continue;
        }

        if ((*it)->m_iWorkspaceID == pWindow->m_iWorkspaceID && (*it)->m_bIsMapped && !(*it)->isHidden() && (!focusableOnly || !(*it)->m_bNoFocus))
            return it->get();
    }

    for (auto it = m_vWindows.rbegin(); it != m_vWindows.rend(); it++) {
        if (it->get() != pWindow && (*it)->m_iWorkspaceID == pWindow->m_iWorkspaceID && (*it)->m_bIsMapped && !(*it)->isHidden() && (!focusableOnly || !(*it)->m_bNoFocus))
            return it->get();
    }

    return nullptr;
}

int CCompositor::getNextAvailableNamedWorkspace() {
    int lowest = -1337 + 1;
    for (auto& w : m_vWorkspaces) {
        if (w->m_iID < -1 && w->m_iID < lowest)
            lowest = w->m_iID;
    }

    return lowest - 1;
}

CWorkspace* CCompositor::getWorkspaceByName(const std::string& name) {
    for (auto& w : m_vWorkspaces) {
        if (w->m_szName == name)
            return w.get();
    }

    return nullptr;
}

CWorkspace* CCompositor::getWorkspaceByString(const std::string& str) {
    if (str.find("name:") == 0) {
        return getWorkspaceByName(str.substr(str.find_first_of(':') + 1));
    }

    try {
        std::string name = "";
        return getWorkspaceByID(getWorkspaceIDFromString(str, name));
    } catch (std::exception& e) {
        Debug::log(ERR, "Error in getWorkspaceByString, invalid id");
    }

    return nullptr;
}

bool CCompositor::isPointOnAnyMonitor(const Vector2D& point) {
    for (auto& m : m_vMonitors) {
        if (VECINRECT(point, m->vecPosition.x, m->vecPosition.y, m->vecSize.x + m->vecPosition.x, m->vecSize.y + m->vecPosition.y))
            return true;
    }

    return false;
}

void checkFocusSurfaceIter(wlr_surface* pSurface, int x, int y, void* data) {
    auto pair = (std::pair<wlr_surface*, bool>*)data;
    pair->second = pair->second || pSurface == pair->first;
}

CWindow* CCompositor::getConstraintWindow(SMouse* pMouse) {
    if (!pMouse->currentConstraint)
        return nullptr;

    const auto PSURFACE = pMouse->currentConstraint->surface;

    for (auto& w : m_vWindows) {
        if (w->isHidden() || !w->m_bMappedX11 || !w->m_bIsMapped || !g_pXWaylandManager->getWindowSurface(w.get()))
            continue;

        if (w->m_bIsX11) {
            if (PSURFACE == g_pXWaylandManager->getWindowSurface(w.get()))
                return w.get();
        } else {
            std::pair<wlr_surface*, bool> check = {PSURFACE, false};
            wlr_surface_for_each_surface(w->m_uSurface.xdg->surface, checkFocusSurfaceIter, &check);

            if (check.second)
                return w.get();
        }
    }

    return nullptr;
}

CMonitor* CCompositor::getMonitorInDirection(const char& dir) {
    const auto POSA = m_pLastMonitor->vecPosition;
    const auto SIZEA = m_pLastMonitor->vecSize;

    auto longestIntersect = -1;
    CMonitor* longestIntersectMonitor = nullptr;

    for (auto& m : m_vMonitors) {
        if (m.get() == m_pLastMonitor)
            continue;

        const auto POSB = m->vecPosition;
        const auto SIZEB = m->vecSize;
        switch (dir) {
            case 'l':
                if (STICKS(POSA.x, POSB.x + SIZEB.x)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectMonitor = m.get();
                    }
                }
                break;
            case 'r':
                if (STICKS(POSA.x + SIZEA.x, POSB.x)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectMonitor = m.get();
                    }
                }
                break;
            case 't':
            case 'u':
                if (STICKS(POSA.y, POSB.y + SIZEB.y)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectMonitor = m.get();
                    }
                }
                break;
            case 'b':
            case 'd':
                if (STICKS(POSA.y + SIZEA.y, POSB.y)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectMonitor = m.get();
                    }
                }
                break;
        }
    }

    if (longestIntersect != -1)
        return longestIntersectMonitor;

    return nullptr;
}

void CCompositor::updateAllWindowsAnimatedDecorationValues() {
    for (auto& w : m_vWindows) {
        if (!w->m_bIsMapped)
            continue;

        updateWindowAnimatedDecorationValues(w.get());
    }
}

void CCompositor::updateWindowAnimatedDecorationValues(CWindow* pWindow) {
    // optimization
    static int64_t* ACTIVECOL = &g_pConfigManager->getConfigValuePtr("general:col.active_border")->intValue;
    static int64_t* INACTIVECOL = &g_pConfigManager->getConfigValuePtr("general:col.inactive_border")->intValue;
    static auto *const PINACTIVEALPHA = &g_pConfigManager->getConfigValuePtr("decoration:inactive_opacity")->floatValue;
    static auto *const PACTIVEALPHA = &g_pConfigManager->getConfigValuePtr("decoration:active_opacity")->floatValue;
    static auto *const PFULLSCREENALPHA = &g_pConfigManager->getConfigValuePtr("decoration:fullscreen_opacity")->floatValue;
    static auto *const PSHADOWCOL = &g_pConfigManager->getConfigValuePtr("decoration:col.shadow")->intValue;
    static auto *const PSHADOWCOLINACTIVE = &g_pConfigManager->getConfigValuePtr("decoration:col.shadow_inactive")->intValue;
    static auto *const PDIMSTRENGTH = &g_pConfigManager->getConfigValuePtr("decoration:dim_strength")->floatValue;

    // border
    const auto RENDERDATA = g_pLayoutManager->getCurrentLayout()->requestRenderHints(pWindow);
    if (RENDERDATA.isBorderColor)
        pWindow->m_cRealBorderColor = RENDERDATA.borderColor;
    else
        pWindow->m_cRealBorderColor = CColor(pWindow == m_pLastWindow ? *ACTIVECOL : *INACTIVECOL);


    // opacity
    if (pWindow->m_bIsFullscreen) {
        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

        if (PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL)
            pWindow->m_fActiveInactiveAlpha = *PFULLSCREENALPHA;
        else {
            if (pWindow == m_pLastWindow)
                pWindow->m_fActiveInactiveAlpha = pWindow->m_sSpecialRenderData.alpha * *PACTIVEALPHA;
            else
                pWindow->m_fActiveInactiveAlpha = pWindow->m_sSpecialRenderData.alphaInactive != -1 ? pWindow->m_sSpecialRenderData.alphaInactive * *PINACTIVEALPHA : *PINACTIVEALPHA;
        }
    } else {
        if (pWindow == m_pLastWindow)
            pWindow->m_fActiveInactiveAlpha = pWindow->m_sSpecialRenderData.alpha * *PACTIVEALPHA;
        else
            pWindow->m_fActiveInactiveAlpha = pWindow->m_sSpecialRenderData.alphaInactive != -1 ? pWindow->m_sSpecialRenderData.alphaInactive * *PINACTIVEALPHA : *PINACTIVEALPHA;
    }

    // dim
    if (pWindow == m_pLastWindow) {
        pWindow->m_fDimPercent = 0;
    } else {
        pWindow->m_fDimPercent = *PDIMSTRENGTH;
    }

    // shadow
    if (pWindow->m_iX11Type != 2 && !pWindow->m_bX11DoesntWantBorders) {
        if (pWindow == m_pLastWindow) {
            pWindow->m_cRealShadowColor = CColor(*PSHADOWCOL);
        } else {
            pWindow->m_cRealShadowColor = CColor(*PSHADOWCOLINACTIVE != INT_MAX ? *PSHADOWCOLINACTIVE : *PSHADOWCOL);
        }
    } else {
        pWindow->m_cRealShadowColor.setValueAndWarp(CColor(0, 0, 0, 0)); // no shadow
    }
}

int CCompositor::getNextAvailableMonitorID() {
    int64_t topID = -1;
    for (auto& m : m_vRealMonitors) {
        if ((int64_t)m->ID > topID)
            topID = m->ID;
    }

    return topID + 1;
}

void CCompositor::swapActiveWorkspaces(CMonitor* pMonitorA, CMonitor* pMonitorB) {

    const auto PWORKSPACEA = g_pCompositor->getWorkspaceByID(pMonitorA->activeWorkspace);
    const auto PWORKSPACEB = g_pCompositor->getWorkspaceByID(pMonitorB->activeWorkspace);

    PWORKSPACEA->m_iMonitorID = pMonitorB->ID;
    PWORKSPACEA->moveToMonitor(pMonitorB->ID);

    for (auto& w : m_vWindows) {
        if (w->m_iWorkspaceID == PWORKSPACEA->m_iID) {
            w->m_iMonitorID = pMonitorB->ID;

            // additionally, move floating and fs windows manually
            if (w->m_bIsFloating)
                w->m_vRealPosition = w->m_vRealPosition.vec() - pMonitorA->vecPosition + pMonitorB->vecPosition;

            if (w->m_bIsFullscreen) {
                w->m_vRealPosition = pMonitorB->vecPosition;
                w->m_vRealSize = pMonitorB->vecSize;
            }

            w->updateToplevel();
        }
    }

    PWORKSPACEB->m_iMonitorID = pMonitorA->ID;
    PWORKSPACEB->moveToMonitor(pMonitorA->ID);

    for (auto& w : m_vWindows) {
        if (w->m_iWorkspaceID == PWORKSPACEB->m_iID) {
            w->m_iMonitorID = pMonitorA->ID;

            // additionally, move floating and fs windows manually
            if (w->m_bIsFloating)
                w->m_vRealPosition = w->m_vRealPosition.vec() - pMonitorB->vecPosition + pMonitorA->vecPosition;

            if (w->m_bIsFullscreen) {
                w->m_vRealPosition = pMonitorA->vecPosition;
                w->m_vRealSize = pMonitorA->vecSize;
            }

            w->updateToplevel();
        }
    }

    // fix pinned windows
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_iWorkspaceID == pMonitorA->activeWorkspace && w->m_bPinned) {
            w->m_iWorkspaceID = PWORKSPACEB->m_iID;
        }

        if (w->m_iWorkspaceID == pMonitorB->activeWorkspace && w->m_bPinned) {
            w->m_iWorkspaceID = PWORKSPACEA->m_iID;
        }
    }

    pMonitorA->activeWorkspace = PWORKSPACEB->m_iID;
    pMonitorB->activeWorkspace = PWORKSPACEA->m_iID;

    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(pMonitorA->ID);
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(pMonitorB->ID);

    g_pInputManager->refocus();

    // event
    g_pEventManager->postEvent(SHyprIPCEvent{"moveworkspace", PWORKSPACEA->m_szName + "," + pMonitorB->szName});
    g_pEventManager->postEvent(SHyprIPCEvent{"moveworkspace", PWORKSPACEB->m_szName + "," + pMonitorA->szName});
}

CMonitor* CCompositor::getMonitorFromString(const std::string& name) {
    if (isNumber(name)) {
        // change by ID
        int monID = -1;
        try {
            monID = std::stoi(name);
        } catch (std::exception& e) {
            // shouldn't happen but jic
            Debug::log(ERR, "Error in getMonitorFromString: invalid num");
            return nullptr;
        }

        if (monID > -1 && monID < (int)g_pCompositor->m_vMonitors.size()) {
            return g_pCompositor->getMonitorFromID(monID);
        } else {
            Debug::log(ERR, "Error in getMonitorFromString: invalid arg 1");
            return nullptr;
        }
    } else {
        if (name == "current")
            return g_pCompositor->m_pLastMonitor;

        if (isDirection(name)) {
            const auto PMONITOR = g_pCompositor->getMonitorInDirection(name[0]);
            return PMONITOR;
        } else {
            for (auto& m : g_pCompositor->m_vMonitors) {
                if (m->szName == name) {
                    return m.get();
                }
            }
        }

        Debug::log(ERR, "Error in getMonitorFromString: no such monitor");
    }

    return nullptr;
}

void CCompositor::moveWorkspaceToMonitor(CWorkspace* pWorkspace, CMonitor* pMonitor) {

    // We trust the workspace and monitor to be correct.

    if (pWorkspace->m_iMonitorID == pMonitor->ID)
        return;

    Debug::log(LOG, "moveWorkspaceToMonitor: Moving %d to monitor %d", pWorkspace->m_iID, pMonitor->ID);

    const auto POLDMON = getMonitorFromID(pWorkspace->m_iMonitorID);

    const bool SWITCHINGISACTIVE = POLDMON->activeWorkspace == pWorkspace->m_iID;

    // fix old mon
    int nextWorkspaceOnMonitorID = -1;
    for (auto& w : m_vWorkspaces) {
        if (w->m_iMonitorID == POLDMON->ID && w->m_iID != pWorkspace->m_iID) {
            nextWorkspaceOnMonitorID = w->m_iID;
            break;
        }
    }

    if (nextWorkspaceOnMonitorID == -1) {
        nextWorkspaceOnMonitorID = 1;

        while (getWorkspaceByID(nextWorkspaceOnMonitorID) || [&]() -> bool { const auto B = g_pConfigManager->getBoundMonitorForWS(std::to_string(nextWorkspaceOnMonitorID)); return B && B != POLDMON; }())
            nextWorkspaceOnMonitorID++;

        Debug::log(LOG, "moveWorkspaceToMonitor: Plugging gap with new %d", nextWorkspaceOnMonitorID);
    }

    Debug::log(LOG, "moveWorkspaceToMonitor: Plugging gap with existing %d", nextWorkspaceOnMonitorID);

    g_pKeybindManager->focusMonitor(std::to_string(POLDMON->ID));
    g_pKeybindManager->changeworkspace(std::to_string(nextWorkspaceOnMonitorID));

    // move the workspace

    pWorkspace->m_iMonitorID = pMonitor->ID;
    pWorkspace->moveToMonitor(pMonitor->ID);

    for (auto& w : m_vWindows) {
        if (w->m_iWorkspaceID == pWorkspace->m_iID) {
            w->m_iMonitorID = pMonitor->ID;

            // additionally, move floating and fs windows manually
            if (w->m_bIsMapped && !w->isHidden()) {
                if (w->m_bIsFloating)
                    w->m_vRealPosition = w->m_vRealPosition.vec() - POLDMON->vecPosition + pMonitor->vecPosition;

                if (w->m_bIsFullscreen) {
                    w->m_vRealPosition = pMonitor->vecPosition;
                    w->m_vRealSize = pMonitor->vecSize;
                }
            }

            w->updateToplevel();
        }
    }

    if (SWITCHINGISACTIVE) { // if it was active, preserve its' status. If it wasn't, don't.
        Debug::log(LOG, "moveWorkspaceToMonitor: SWITCHINGISACTIVE, active %d -> %d", pMonitor->activeWorkspace, pWorkspace->m_iID);

        if (const auto PWORKSPACE = getWorkspaceByID(pMonitor->activeWorkspace); PWORKSPACE)
            getWorkspaceByID(pMonitor->activeWorkspace)->startAnim(false, false);

        pMonitor->activeWorkspace = pWorkspace->m_iID;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(pMonitor->ID);

        pWorkspace->startAnim(true, true, true);

        wlr_cursor_warp(m_sWLRCursor, m_sSeat.mouse->mouse, pMonitor->vecPosition.x + pMonitor->vecTransformedSize.x / 2, pMonitor->vecPosition.y + pMonitor->vecTransformedSize.y / 2);
    }

    // finalize
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(POLDMON->ID);

    g_pInputManager->refocus();

    // event
    g_pEventManager->postEvent(SHyprIPCEvent{"moveworkspace", pWorkspace->m_szName + "," + pMonitor->szName});
}

bool CCompositor::workspaceIDOutOfBounds(const int& id) {
    int lowestID = 99999;
    int highestID = -99999;

    for (auto& w : m_vWorkspaces) {
        if (w->m_iID == SPECIAL_WORKSPACE_ID)
            continue;

        if (w->m_iID < lowestID)
            lowestID = w->m_iID;

        if (w->m_iID > highestID)
            highestID = w->m_iID;
    }

    return std::clamp(id, lowestID, highestID) != id;
}

void CCompositor::setWindowFullscreen(CWindow* pWindow, bool on, eFullscreenMode mode) {
    if (!windowValidMapped(pWindow))
        return;

    if (pWindow->m_bPinned) {
        Debug::log(LOG, "Pinned windows cannot be fullscreen'd");
        return;
    }

    const auto PMONITOR = getMonitorFromID(pWindow->m_iMonitorID);

    const auto PWORKSPACE = getWorkspaceByID(pWindow->m_iWorkspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow && on) {
        Debug::log(LOG, "Rejecting fullscreen ON on a fullscreen workspace");
        return;
    }

    g_pLayoutManager->getCurrentLayout()->fullscreenRequestForWindow(pWindow, mode, on);

    g_pXWaylandManager->setWindowFullscreen(pWindow, pWindow->m_bIsFullscreen && mode == FULLSCREEN_FULL);

    // make all windows on the same workspace under the fullscreen window
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_iWorkspaceID == pWindow->m_iWorkspaceID) {
            w->m_bCreatedOverFullscreen = false;
            if (w.get() != pWindow && !w->m_bFadingOut && !w->m_bPinned)
                w->m_fAlpha = pWindow->m_bIsFullscreen ? 0.f : 255.f;
        }
    }

    for (auto& ls : PMONITOR->m_aLayerSurfaceLists[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        if (!ls->fadingOut)
            ls->alpha = pWindow->m_bIsFullscreen && mode == FULLSCREEN_FULL ? 0.f : 255.f;
    }

    g_pXWaylandManager->setWindowSize(pWindow, pWindow->m_vRealSize.goalv(), true);

    forceReportSizesToWindowsOnWorkspace(pWindow->m_iWorkspaceID);

    g_pInputManager->recheckIdleInhibitorStatus();
}

void CCompositor::moveUnmanagedX11ToWindows(CWindow* pWindow) {
    for (auto it = m_dUnmanagedX11Windows.begin(); it != m_dUnmanagedX11Windows.end(); it++) {
        if (it->get() == pWindow) {
            m_vWindows.emplace_back(std::move(*it));
            m_dUnmanagedX11Windows.erase(it);
            return;
        }
    }
}

CWindow* CCompositor::getX11Parent(CWindow* pWindow) {
    if (!pWindow->m_bIsX11)
        return nullptr;

    for (auto& w : m_vWindows) {
        if (!w->m_bIsX11)
            continue;

        if (w->m_uSurface.xwayland == pWindow->m_uSurface.xwayland->parent)
            return w.get();
    }

    return nullptr;
}

void CCompositor::updateWorkspaceWindowDecos(const int& id) {
    for (auto& w : m_vWindows) {
        if (w->m_iWorkspaceID != id)
            continue;

        w->updateWindowDecos();
    }
}

void CCompositor::scheduleFrameForMonitor(CMonitor* pMonitor) {
    if ((m_sWLRSession && !m_sWLRSession->active) || !m_bSessionActive)
        return;

    wlr_output_schedule_frame(pMonitor->output);
}

CWindow* CCompositor::getWindowByRegex(const std::string& regexp) {
    eFocusWindowMode mode = MODE_CLASS_REGEX;

    std::regex regexCheck(regexp);
    std::string matchCheck;
    if (regexp.find("title:") == 0) {
        mode = MODE_TITLE_REGEX;
        regexCheck = std::regex(regexp.substr(6));
    } else if (regexp.find("address:") == 0) {
        mode = MODE_ADDRESS;
        matchCheck = regexp.substr(8);
    } else if (regexp.find("pid:") == 0) {
        mode = MODE_PID;
        matchCheck = regexp.substr(4);
    }

    for (auto& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped || w->isHidden())
            continue;

        switch (mode) {
            case MODE_CLASS_REGEX: {
                const auto windowClass = g_pXWaylandManager->getAppIDClass(w.get());
                if (!std::regex_search(g_pXWaylandManager->getAppIDClass(w.get()), regexCheck))
                    continue;
                break;
            }
            case MODE_TITLE_REGEX: {
                const auto windowTitle = g_pXWaylandManager->getTitle(w.get());
                if (!std::regex_search(windowTitle, regexCheck))
                    continue;
                break;
            }
            case MODE_ADDRESS: {
                std::string addr = getFormat("0x%x", w.get());
                if (matchCheck != addr)
                    continue;
                break;
            }
            case MODE_PID: {
                std::string pid = getFormat("%d", w->getPID());
                if (matchCheck != pid)
                    continue;
                break;
            }
            default:
                break;
        }

        return w.get();
    }

    return nullptr;
}

void CCompositor::warpCursorTo(const Vector2D& pos) {

    // warpCursorTo should only be used for warps that
    // should be disabled with no_cursor_warps

    static auto *const PNOWARPS = &g_pConfigManager->getConfigValuePtr("general:no_cursor_warps")->intValue;

    if (*PNOWARPS)
        return;

    wlr_cursor_warp(m_sWLRCursor, m_sSeat.mouse->mouse, pos.x, pos.y);

    const auto PMONITORNEW = getMonitorFromVector(pos);
    if (PMONITORNEW != m_pLastMonitor)
        m_pLastMonitor = PMONITORNEW;
}

SLayerSurface* CCompositor::getLayerSurfaceFromWlr(wlr_layer_surface_v1* pLS) {
    for (auto& m : m_vMonitors) {
        for (auto& lsl : m->m_aLayerSurfaceLists) {
            for (auto& ls : lsl) {
                if (ls->layerSurface == pLS)
                    return ls.get();
            }
        }
    }

    return nullptr;
}

void CCompositor::closeWindow(CWindow* pWindow) {
    if (pWindow && windowValidMapped(pWindow)) {
        g_pXWaylandManager->sendCloseWindow(pWindow);
    }
}

SLayerSurface* CCompositor::getLayerSurfaceFromSurface(wlr_surface* pSurface) {
    for (auto& m : m_vMonitors) {
        for (auto& lsl : m->m_aLayerSurfaceLists) {
            for (auto& ls : lsl) {
                if (ls->layerSurface && ls->layerSurface->surface == pSurface)
                    return ls.get();
            }
        }
    }

    return nullptr;
}

// returns a delta
Vector2D CCompositor::parseWindowVectorArgsRelative(const std::string& args, const Vector2D& relativeTo) {
    if (!args.contains(' '))
        return relativeTo;

    std::string x = args.substr(0, args.find_first_of(' '));
    std::string y = args.substr(args.find_first_of(' ') + 1);

    if (x == "exact") {
        std::string newX = y.substr(0, y.find_first_of(' '));
        std::string newY = y.substr(y.find_first_of(' ') + 1);

        if (!isNumber(newX) || !isNumber(newY)) {
            Debug::log(ERR, "parseWindowVectorArgsRelative: exact args not numbers");
            return relativeTo;
        }

        const int X = std::stoi(newX);
        const int Y = std::stoi(newY);

        if (X < 0 || Y < 0) {
            Debug::log(ERR, "parseWindowVectorArgsRelative: exact args cannot be < 0");
            return relativeTo;
        }

        return Vector2D(X, Y);
    }

    if (!isNumber(x) || !isNumber(y)) {
        Debug::log(ERR, "parseWindowVectorArgsRelative: args not numbers");
        return relativeTo;
    }

    const int X = std::stoi(x);
    const int Y = std::stoi(y);

    return Vector2D(X + relativeTo.x, Y + relativeTo.y);
}

void CCompositor::forceReportSizesToWindowsOnWorkspace(const int& wid) {
    for (auto& w : m_vWindows) {
        if (w->m_iWorkspaceID == wid && w->m_bIsMapped && !w->isHidden()) {
            g_pXWaylandManager->setWindowSize(w.get(), w->m_vRealSize.vec(), true);
        }
    }
}

bool CCompositor::cursorOnReservedArea() {
    const auto PMONITOR = getMonitorFromCursor();

    const auto XY1 = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
    const auto XY2 = PMONITOR->vecPosition + PMONITOR->vecSize - PMONITOR->vecReservedBottomRight;

    const auto CURSORPOS = g_pInputManager->getMouseCoordsInternal();

    return !VECINRECT(CURSORPOS, XY1.x, XY1.y, XY2.x, XY2.y);
}

CWorkspace* CCompositor::createNewWorkspace(const int& id, const int& monid, const std::string& name) {
    const auto NAME = name == "" ? std::to_string(id) : name;
    auto monID = monid;

    // check if bound
    if (const auto PMONITOR = g_pConfigManager->getBoundMonitorForWS(NAME); PMONITOR) {
        monID = PMONITOR->ID;
    }

    const auto PWORKSPACE = m_vWorkspaces.emplace_back(std::make_unique<CWorkspace>(monID, NAME, id == SPECIAL_WORKSPACE_ID)).get();

    // We are required to set the name here immediately
    if (id != SPECIAL_WORKSPACE_ID)
        wlr_ext_workspace_handle_v1_set_name(PWORKSPACE->m_pWlrHandle, NAME.c_str());

    PWORKSPACE->m_iID = id;
    PWORKSPACE->m_iMonitorID = monID;

    return PWORKSPACE;
}
