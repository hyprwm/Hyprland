#include "Compositor.hpp"
#include "helpers/Splashes.hpp"
#include <random>
#include <unordered_set>
#include "debug/HyprCtl.hpp"
#include "debug/CrashReporter.hpp"
#ifdef USES_SYSTEMD
#include <systemd/sd-daemon.h> // for sd_notify
#endif
#include <ranges>

int handleCritSignal(int signo, void* data) {
    Debug::log(LOG, "Hyprland received signal {}", signo);

    if (signo == SIGTERM || signo == SIGINT || signo == SIGKILL)
        g_pCompositor->cleanup();

    return 0;
}

void handleUnrecoverableSignal(int sig) {

    // remove our handlers
    signal(SIGABRT, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);

    if (g_pHookSystem && g_pHookSystem->m_bCurrentEventPlugin) {
        longjmp(g_pHookSystem->m_jbHookFaultJumpBuf, 1);
        return;
    }

    CrashReporter::createAndSaveCrash(sig);

    abort();
}

void handleUserSignal(int sig) {
    if (sig == SIGUSR1) {
        // means we have to unwind a timed out event
        throw std::exception();
    }
}

CCompositor::CCompositor() {
    m_iHyprlandPID = getpid();

    m_szInstanceSignature = GIT_COMMIT_HASH + std::string("_") + std::to_string(time(NULL));

    setenv("HYPRLAND_INSTANCE_SIGNATURE", m_szInstanceSignature.c_str(), true);

    if (!std::filesystem::exists("/tmp/hypr")) {
        std::filesystem::create_directory("/tmp/hypr");
        std::filesystem::permissions("/tmp/hypr", std::filesystem::perms::all | std::filesystem::perms::sticky_bit, std::filesystem::perm_options::replace);
    }

    const auto INSTANCEPATH = "/tmp/hypr/" + m_szInstanceSignature;
    std::filesystem::create_directory(INSTANCEPATH);
    std::filesystem::permissions(INSTANCEPATH, std::filesystem::perms::group_all, std::filesystem::perm_options::replace);
    std::filesystem::permissions(INSTANCEPATH, std::filesystem::perms::owner_all, std::filesystem::perm_options::add);

    Debug::init(m_szInstanceSignature);

    Debug::log(LOG, "Instance Signature: {}", m_szInstanceSignature);

    Debug::log(LOG, "Hyprland PID: {}", m_iHyprlandPID);

    Debug::log(LOG, "===== SYSTEM INFO: =====");

    logSystemInfo();

    Debug::log(LOG, "========================");

    Debug::log(NONE, "\n\n"); // pad

    Debug::log(INFO, "If you are crashing, or encounter any bugs, please consult https://wiki.hyprland.org/Crashes-and-Bugs/\n\n");

    setRandomSplash();

    Debug::log(LOG, "\nCurrent splash: {}\n\n", m_szCurrentSplash);
}

CCompositor::~CCompositor() {
    cleanup();
}

void CCompositor::setRandomSplash() {
    std::random_device              dev;
    std::mt19937                    engine(dev());
    std::uniform_int_distribution<> distribution(0, SPLASHES.size() - 1);

    m_szCurrentSplash = SPLASHES[distribution(engine)];
}

void CCompositor::initServer() {

    m_sWLDisplay = wl_display_create();

    m_sWLEventLoop = wl_display_get_event_loop(m_sWLDisplay);

    // register crit signal handler
    wl_event_loop_add_signal(m_sWLEventLoop, SIGTERM, handleCritSignal, nullptr);
    signal(SIGSEGV, handleUnrecoverableSignal);
    signal(SIGABRT, handleUnrecoverableSignal);
    signal(SIGUSR1, handleUserSignal);

    initManagers(STAGE_PRIORITY);

    if (envEnabled("HYPRLAND_TRACE"))
        Debug::trace = true;

    wlr_log_init(WLR_INFO, NULL);

    if (envEnabled("HYPRLAND_LOG_WLR"))
        wlr_log_init(WLR_DEBUG, Debug::wlrLog);
    else
        wlr_log_init(WLR_ERROR, Debug::wlrLog);

    m_sWLRBackend = wlr_backend_autocreate(m_sWLEventLoop, &m_sWLRSession);

    if (!m_sWLRBackend) {
        Debug::log(CRIT, "m_sWLRBackend was NULL! This usually means wlroots could not find a GPU or enountered some issues.");
        throwError("wlr_backend_autocreate() failed!");
    }

    m_iDRMFD = wlr_backend_get_drm_fd(m_sWLRBackend);
    if (m_iDRMFD < 0) {
        Debug::log(CRIT, "Couldn't query the DRM FD!");
        throwError("wlr_backend_get_drm_fd() failed!");
    }

    m_sWLRRenderer = wlr_gles2_renderer_create_with_drm_fd(m_iDRMFD);

    if (!m_sWLRRenderer) {
        Debug::log(CRIT, "m_sWLRRenderer was NULL! This usually means wlroots could not find a GPU or enountered some issues.");
        throwError("wlr_gles2_renderer_create_with_drm_fd() failed!");
    }

    wlr_renderer_init_wl_shm(m_sWLRRenderer, m_sWLDisplay);

    if (wlr_renderer_get_dmabuf_texture_formats(m_sWLRRenderer)) {
        if (wlr_renderer_get_drm_fd(m_sWLRRenderer) >= 0)
            wlr_drm_create(m_sWLDisplay, m_sWLRRenderer);

        m_sWLRLinuxDMABuf = wlr_linux_dmabuf_v1_create_with_renderer(m_sWLDisplay, 4, m_sWLRRenderer);
    }

    m_sWLRAllocator = wlr_allocator_autocreate(m_sWLRBackend, m_sWLRRenderer);

    if (!m_sWLRAllocator) {
        Debug::log(CRIT, "m_sWLRAllocator was NULL!");
        throwError("wlr_allocator_autocreate() failed!");
    }

    m_sWLREGL = wlr_gles2_renderer_get_egl(m_sWLRRenderer);

    if (!m_sWLREGL) {
        Debug::log(CRIT, "m_sWLREGL was NULL!");
        throwError("wlr_gles2_renderer_get_egl() failed!");
    }

    m_sWLRCompositor    = wlr_compositor_create(m_sWLDisplay, 6, m_sWLRRenderer);
    m_sWLRSubCompositor = wlr_subcompositor_create(m_sWLDisplay);
    m_sWLRDataDevMgr    = wlr_data_device_manager_create(m_sWLDisplay);

    wlr_export_dmabuf_manager_v1_create(m_sWLDisplay);
    wlr_data_control_manager_v1_create(m_sWLDisplay);
    wlr_primary_selection_v1_device_manager_create(m_sWLDisplay);
    wlr_viewporter_create(m_sWLDisplay);

    m_sWLRGammaCtrlMgr = wlr_gamma_control_manager_v1_create(m_sWLDisplay);

    m_sWLROutputLayout = wlr_output_layout_create(m_sWLDisplay);

    m_sWLROutputPowerMgr = wlr_output_power_manager_v1_create(m_sWLDisplay);

    m_sWLRXDGShell = wlr_xdg_shell_create(m_sWLDisplay, 6);

    m_sWLRCursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(m_sWLRCursor, m_sWLROutputLayout);

    if (const auto XCURSORENV = getenv("XCURSOR_SIZE"); !XCURSORENV || std::string(XCURSORENV).empty())
        setenv("XCURSOR_SIZE", "24", true);

    const auto XCURSORENV = getenv("XCURSOR_SIZE");
    int        cursorSize = 24;
    try {
        cursorSize = std::stoi(XCURSORENV);
    } catch (std::exception& e) { Debug::log(ERR, "XCURSOR_SIZE invalid in check #2? ({})", XCURSORENV); }

    m_sWLRXCursorMgr = wlr_xcursor_manager_create(nullptr, cursorSize);
    wlr_xcursor_manager_load(m_sWLRXCursorMgr, 1);

    m_sSeat.seat = wlr_seat_create(m_sWLDisplay, "seat0");

    m_sWLRPresentation = wlr_presentation_create(m_sWLDisplay, m_sWLRBackend);

    m_sWLRIdleNotifier = wlr_idle_notifier_v1_create(m_sWLDisplay);

    m_sWLRLayerShell = wlr_layer_shell_v1_create(m_sWLDisplay, 4);

    m_sWLRServerDecoMgr = wlr_server_decoration_manager_create(m_sWLDisplay);
    m_sWLRXDGDecoMgr    = wlr_xdg_decoration_manager_v1_create(m_sWLDisplay);
    wlr_server_decoration_manager_set_default_mode(m_sWLRServerDecoMgr, WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);

    m_sWLROutputMgr = wlr_output_manager_v1_create(m_sWLDisplay);

    m_sWLRKbShInhibitMgr = wlr_keyboard_shortcuts_inhibit_v1_create(m_sWLDisplay);

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

    m_sWLRTextInputMgr = wlr_text_input_manager_v3_create(m_sWLDisplay);

    m_sWLRIMEMgr = wlr_input_method_manager_v2_create(m_sWLDisplay);

    m_sWLRActivation = wlr_xdg_activation_v1_create(m_sWLDisplay);

    m_sWLRHeadlessBackend = wlr_headless_backend_create(m_sWLEventLoop);

    m_sWLRSessionLockMgr = wlr_session_lock_manager_v1_create(m_sWLDisplay);

    m_sWLRCursorShapeMgr = wlr_cursor_shape_manager_v1_create(m_sWLDisplay, 1);

    m_sWLRTearingControlMgr = wlr_tearing_control_manager_v1_create(m_sWLDisplay, 1);

    if (!m_sWLRHeadlessBackend) {
        Debug::log(CRIT, "Couldn't create the headless backend");
        throwError("wlr_headless_backend_create() failed!");
    }

    wlr_single_pixel_buffer_manager_v1_create(m_sWLDisplay);

    wlr_multi_backend_add(m_sWLRBackend, m_sWLRHeadlessBackend);

    initManagers(STAGE_LATE);
}

void CCompositor::initAllSignals() {
    addWLSignal(&m_sWLRBackend->events.new_output, &Events::listen_newOutput, m_sWLRBackend, "Backend");
    addWLSignal(&m_sWLRXDGShell->events.new_toplevel, &Events::listen_newXDGToplevel, m_sWLRXDGShell, "XDG Shell");
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
    addWLSignal(&m_sWLRSessionLockMgr->events.new_lock, &Events::listen_newSessionLock, m_sWLRSessionLockMgr, "SessionLockMgr");
    addWLSignal(&m_sWLRGammaCtrlMgr->events.set_gamma, &Events::listen_setGamma, m_sWLRGammaCtrlMgr, "GammaCtrlMgr");
    addWLSignal(&m_sWLRCursorShapeMgr->events.request_set_shape, &Events::listen_setCursorShape, m_sWLRCursorShapeMgr, "CursorShapeMgr");
    addWLSignal(&m_sWLRTearingControlMgr->events.new_object, &Events::listen_newTearingHint, m_sWLRTearingControlMgr, "TearingControlMgr");

    if (m_sWRLDRMLeaseMgr)
        addWLSignal(&m_sWRLDRMLeaseMgr->events.request, &Events::listen_leaseRequest, &m_sWRLDRMLeaseMgr, "DRM");

    if (m_sWLRSession)
        addWLSignal(&m_sWLRSession->events.active, &Events::listen_sessionActive, m_sWLRSession, "Session");
}

void CCompositor::removeAllSignals() {
    removeWLSignal(&Events::listen_newOutput);
    removeWLSignal(&Events::listen_newXDGToplevel);
    removeWLSignal(&Events::listen_mouseMove);
    removeWLSignal(&Events::listen_mouseMoveAbsolute);
    removeWLSignal(&Events::listen_mouseButton);
    removeWLSignal(&Events::listen_mouseAxis);
    removeWLSignal(&Events::listen_mouseFrame);
    removeWLSignal(&Events::listen_swipeBegin);
    removeWLSignal(&Events::listen_swipeUpdate);
    removeWLSignal(&Events::listen_swipeEnd);
    removeWLSignal(&Events::listen_pinchBegin);
    removeWLSignal(&Events::listen_pinchUpdate);
    removeWLSignal(&Events::listen_pinchEnd);
    removeWLSignal(&Events::listen_touchBegin);
    removeWLSignal(&Events::listen_touchEnd);
    removeWLSignal(&Events::listen_touchUpdate);
    removeWLSignal(&Events::listen_touchFrame);
    removeWLSignal(&Events::listen_holdBegin);
    removeWLSignal(&Events::listen_holdEnd);
    removeWLSignal(&Events::listen_newInput);
    removeWLSignal(&Events::listen_requestMouse);
    removeWLSignal(&Events::listen_requestSetSel);
    removeWLSignal(&Events::listen_requestDrag);
    removeWLSignal(&Events::listen_startDrag);
    removeWLSignal(&Events::listen_requestSetSel);
    removeWLSignal(&Events::listen_requestSetPrimarySel);
    removeWLSignal(&Events::listen_newLayerSurface);
    removeWLSignal(&Events::listen_change);
    removeWLSignal(&Events::listen_outputMgrApply);
    removeWLSignal(&Events::listen_outputMgrTest);
    removeWLSignal(&Events::listen_newConstraint);
    removeWLSignal(&Events::listen_NewXDGDeco);
    removeWLSignal(&Events::listen_newVirtPtr);
    removeWLSignal(&Events::listen_newVirtualKeyboard);
    removeWLSignal(&Events::listen_RendererDestroy);
    removeWLSignal(&Events::listen_newIdleInhibitor);
    removeWLSignal(&Events::listen_powerMgrSetMode);
    removeWLSignal(&Events::listen_newIME);
    removeWLSignal(&Events::listen_newTextInput);
    removeWLSignal(&Events::listen_activateXDG);
    removeWLSignal(&Events::listen_newSessionLock);
    removeWLSignal(&Events::listen_setGamma);
    removeWLSignal(&Events::listen_setCursorShape);
    removeWLSignal(&Events::listen_newTearingHint);

    if (m_sWRLDRMLeaseMgr)
        removeWLSignal(&Events::listen_leaseRequest);

    if (m_sWLRSession)
        removeWLSignal(&Events::listen_sessionActive);
}

void CCompositor::cleanup() {
    if (!m_sWLDisplay || m_bIsShuttingDown)
        return;

    removeLockFile();

    m_bIsShuttingDown   = true;
    Debug::shuttingDown = true;

#ifdef USES_SYSTEMD
    if (sd_booted() > 0 && !envEnabled("HYPRLAND_NO_SD_NOTIFY"))
        sd_notify(0, "STOPPING=1");
#endif

    // unload all remaining plugins while the compositor is
    // still in a normal working state.
    g_pPluginSystem->unloadAllPlugins();

    m_pLastFocus  = nullptr;
    m_pLastWindow = nullptr;

    // end threads
    g_pEventManager->m_tThread = std::thread();

    m_vWorkspaces.clear();
    m_vWindows.clear();

    for (auto& m : m_vMonitors) {
        g_pHyprOpenGL->destroyMonitorResources(m.get());

        wlr_output_state_set_enabled(m->state.wlr(), false);
        m->state.commit();
    }

    m_vMonitors.clear();

    if (g_pXWaylandManager->m_sWLRXWayland) {
        wlr_xwayland_destroy(g_pXWaylandManager->m_sWLRXWayland);
        g_pXWaylandManager->m_sWLRXWayland = nullptr;
    }

    removeAllSignals();

    wl_display_destroy_clients(g_pCompositor->m_sWLDisplay);

    g_pDecorationPositioner.reset();
    g_pPluginSystem.reset();
    g_pHyprNotificationOverlay.reset();
    g_pDebugOverlay.reset();
    g_pEventManager.reset();
    g_pSessionLockManager.reset();
    g_pProtocolManager.reset();
    g_pHyprRenderer.reset();
    g_pHyprOpenGL.reset();
    g_pInputManager.reset();
    g_pThreadManager.reset();
    g_pConfigManager.reset();
    g_pLayoutManager.reset();
    g_pHyprError.reset();
    g_pConfigManager.reset();
    g_pAnimationManager.reset();
    g_pKeybindManager.reset();
    g_pHookSystem.reset();
    g_pWatchdog.reset();
    g_pXWaylandManager.reset();

    wl_display_terminate(m_sWLDisplay);

    m_sWLDisplay = nullptr;
}

void CCompositor::initManagers(eManagersInitStage stage) {
    switch (stage) {
        case STAGE_PRIORITY: {
            Debug::log(LOG, "Creating the HookSystem!");
            g_pHookSystem = std::make_unique<CHookSystemManager>();

            Debug::log(LOG, "Creating the KeybindManager!");
            g_pKeybindManager = std::make_unique<CKeybindManager>();

            Debug::log(LOG, "Creating the AnimationManager!");
            g_pAnimationManager = std::make_unique<CAnimationManager>();

            Debug::log(LOG, "Creating the ConfigManager!");
            g_pConfigManager = std::make_unique<CConfigManager>();

            Debug::log(LOG, "Creating the CHyprError!");
            g_pHyprError = std::make_unique<CHyprError>();

            Debug::log(LOG, "Creating the LayoutManager!");
            g_pLayoutManager = std::make_unique<CLayoutManager>();

            g_pConfigManager->init();
            g_pWatchdog = std::make_unique<CWatchdog>(); // requires config
        } break;
        case STAGE_LATE: {
            Debug::log(LOG, "Creating the ThreadManager!");
            g_pThreadManager = std::make_unique<CThreadManager>();

            Debug::log(LOG, "Creating CHyprCtl");
            g_pHyprCtl = std::make_unique<CHyprCtl>();

            Debug::log(LOG, "Creating the InputManager!");
            g_pInputManager = std::make_unique<CInputManager>();

            Debug::log(LOG, "Creating the CHyprOpenGLImpl!");
            g_pHyprOpenGL = std::make_unique<CHyprOpenGLImpl>();

            Debug::log(LOG, "Creating the HyprRenderer!");
            g_pHyprRenderer = std::make_unique<CHyprRenderer>();

            Debug::log(LOG, "Creating the XWaylandManager!");
            g_pXWaylandManager = std::make_unique<CHyprXWaylandManager>();

            Debug::log(LOG, "Creating the ProtocolManager!");
            g_pProtocolManager = std::make_unique<CProtocolManager>();

            Debug::log(LOG, "Creating the SessionLockManager!");
            g_pSessionLockManager = std::make_unique<CSessionLockManager>();

            Debug::log(LOG, "Creating the EventManager!");
            g_pEventManager = std::make_unique<CEventManager>();
            g_pEventManager->startThread();

            Debug::log(LOG, "Creating the HyprDebugOverlay!");
            g_pDebugOverlay = std::make_unique<CHyprDebugOverlay>();

            Debug::log(LOG, "Creating the HyprNotificationOverlay!");
            g_pHyprNotificationOverlay = std::make_unique<CHyprNotificationOverlay>();

            Debug::log(LOG, "Creating the PluginSystem!");
            g_pPluginSystem = std::make_unique<CPluginSystem>();
            g_pConfigManager->handlePluginLoads();

            Debug::log(LOG, "Creating the DecorationPositioner!");
            g_pDecorationPositioner = std::make_unique<CDecorationPositioner>();
        } break;
        default: UNREACHABLE();
    }
}

void CCompositor::createLockFile() {
    const auto    PATH = "/tmp/hypr/" + m_szInstanceSignature + ".lock";

    std::ofstream ofs(PATH, std::ios::trunc);

    ofs << m_iHyprlandPID << "\n" << m_szWLDisplaySocket << "\n";

    ofs.close();
}

void CCompositor::removeLockFile() {
    const auto PATH = "/tmp/hypr/" + m_szInstanceSignature + ".lock";

    if (std::filesystem::exists(PATH))
        std::filesystem::remove(PATH);
}

void CCompositor::prepareFallbackOutput() {
    // create a backup monitor
    wlr_backend* headless = nullptr;
    wlr_multi_for_each_backend(
        m_sWLRBackend,
        [](wlr_backend* b, void* data) {
            if (wlr_backend_is_headless(b))
                *((wlr_backend**)data) = b;
        },
        &headless);

    if (!headless) {
        Debug::log(WARN, "Unsafe state will be ineffective, no fallback output");
        return;
    }

    wlr_headless_add_output(headless, 1920, 1080);
}

void CCompositor::startCompositor() {
    initAllSignals();

    // get socket, avoid using 0
    for (int candidate = 1; candidate <= 32; candidate++) {
        const auto CANDIDATESTR = ("wayland-" + std::to_string(candidate));
        const auto RETVAL       = wl_display_add_socket(m_sWLDisplay, CANDIDATESTR.c_str());
        if (RETVAL >= 0) {
            m_szWLDisplaySocket = CANDIDATESTR;
            Debug::log(LOG, "wl_display_add_socket for {} succeeded with {}", CANDIDATESTR, RETVAL);
            break;
        } else {
            Debug::log(WARN, "wl_display_add_socket for {} returned {}: skipping candidate {}", CANDIDATESTR, RETVAL, candidate);
        }
    }

    if (m_szWLDisplaySocket.empty()) {
        Debug::log(WARN, "All candidates failed, trying wl_display_add_socket_auto");
        const auto SOCKETSTR = wl_display_add_socket_auto(m_sWLDisplay);
        if (SOCKETSTR)
            m_szWLDisplaySocket = SOCKETSTR;
    }

    if (m_szWLDisplaySocket.empty()) {
        Debug::log(CRIT, "m_szWLDisplaySocket NULL!");
        wlr_backend_destroy(m_sWLRBackend);
        throwError("m_szWLDisplaySocket was null! (wl_display_add_socket and wl_display_add_socket_auto failed)");
    }

    setenv("WAYLAND_DISPLAY", m_szWLDisplaySocket.c_str(), 1);

    signal(SIGPIPE, SIG_IGN);

    if (m_sWLRSession /* Session-less Hyprland usually means a nest, don't update the env in that case */) {
        const auto CMD =
#ifdef USES_SYSTEMD
            "systemctl --user import-environment DISPLAY WAYLAND_DISPLAY HYPRLAND_INSTANCE_SIGNATURE XDG_CURRENT_DESKTOP QT_QPA_PLATFORMTHEME && hash "
            "dbus-update-activation-environment 2>/dev/null && "
#endif
            "dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP HYPRLAND_INSTANCE_SIGNATURE QT_QPA_PLATFORMTHEME";
        g_pKeybindManager->spawn(CMD);
    }

    Debug::log(LOG, "Running on WAYLAND_DISPLAY: {}", m_szWLDisplaySocket);

    if (!wlr_backend_start(m_sWLRBackend)) {
        Debug::log(CRIT, "Backend did not start!");
        wlr_backend_destroy(m_sWLRBackend);
        wl_display_destroy(m_sWLDisplay);
        throwError("The backend could not start!");
    }

    prepareFallbackOutput();

    g_pHyprRenderer->setCursorFromName("left_ptr");

#ifdef USES_SYSTEMD
    if (sd_booted() > 0) {
        // tell systemd that we are ready so it can start other bond, following, related units
        if (!envEnabled("HYPRLAND_NO_SD_NOTIFY"))
            sd_notify(0, "READY=1");
    } else
        Debug::log(LOG, "systemd integration is baked in but system itself is not booted à la systemd!");
#endif

    createLockFile();

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

CMonitor* CCompositor::getMonitorFromDesc(const std::string& desc) {
    for (auto& m : m_vMonitors) {
        if (m->szDescription.starts_with(desc))
            return m.get();
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
        float     bestDistance = 0.f;
        CMonitor* pBestMon     = nullptr;

        for (auto& m : m_vMonitors) {
            float dist = vecToRectDistanceSquared(point, m->vecPosition, m->vecPosition + m->vecSize);

            if (dist < bestDistance || !pBestMon) {
                bestDistance = dist;
                pBestMon     = m.get();
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
    if (windowExists(pWindow) && !pWindow->m_bFadingOut)
        std::erase_if(m_vWindows, [&](std::unique_ptr<CWindow>& el) { return el.get() == pWindow; });
}

bool CCompositor::windowExists(CWindow* pWindow) {
    for (auto& w : m_vWindows) {
        if (w.get() == pWindow)
            return true;
    }

    return false;
}

bool CCompositor::monitorExists(CMonitor* pMonitor) {
    for (auto& m : m_vRealMonitors) {
        if (m.get() == pMonitor)
            return true;
    }

    return false;
}

CWindow* CCompositor::vectorToWindowUnified(const Vector2D& pos, uint8_t properties, CWindow* pIgnoreWindow) {
    const auto         PMONITOR          = getMonitorFromVector(pos);
    static auto* const PRESIZEONBORDER   = &g_pConfigManager->getConfigValuePtr("general:resize_on_border")->intValue;
    static auto* const PBORDERSIZE       = &g_pConfigManager->getConfigValuePtr("general:border_size")->intValue;
    static auto* const PBORDERGRABEXTEND = &g_pConfigManager->getConfigValuePtr("general:extend_border_grab_area")->intValue;
    static auto* const PSPECIALFALLTHRU  = &g_pConfigManager->getConfigValuePtr("input:special_fallthrough")->intValue;
    const auto         BORDER_GRAB_AREA  = *PRESIZEONBORDER ? *PBORDERSIZE + *PBORDERGRABEXTEND : 0;

    // pinned windows on top of floating regardless
    if (properties & ALLOW_FLOATING) {
        for (auto& w : m_vWindows | std::views::reverse) {
            const auto BB  = w->getWindowBoxUnified(properties);
            CBox       box = {BB.x - BORDER_GRAB_AREA, BB.y - BORDER_GRAB_AREA, BB.width + 2 * BORDER_GRAB_AREA, BB.height + 2 * BORDER_GRAB_AREA};
            if (w->m_bIsFloating && w->m_bIsMapped && !w->isHidden() && !w->m_bX11ShouldntFocus && w->m_bPinned && !w->m_bNoFocus && w.get() != pIgnoreWindow) {
                if (box.containsPoint({m_sWLRCursor->x, m_sWLRCursor->y}))
                    return w.get();

                if (!w->m_bIsX11) {
                    if (w->hasPopupAt(pos))
                        return w.get();
                }
            }
        }
    }

    auto windowForWorkspace = [&](bool special) -> CWindow* {
        if (properties & ALLOW_FLOATING) {
            // first loop over floating cuz they're above, m_lWindows should be sorted bottom->top, for tiled it doesn't matter.
            for (auto& w : m_vWindows | std::views::reverse) {

                if (special && !isWorkspaceSpecial(w->m_iWorkspaceID)) // because special floating may creep up into regular
                    continue;

                const auto BB  = w->getWindowBoxUnified(properties);
                CBox       box = {BB.x - BORDER_GRAB_AREA, BB.y - BORDER_GRAB_AREA, BB.width + 2 * BORDER_GRAB_AREA, BB.height + 2 * BORDER_GRAB_AREA};
                if (w->m_bIsFloating && w->m_bIsMapped && isWorkspaceVisible(w->m_iWorkspaceID) && !w->isHidden() && !w->m_bPinned && !w->m_bNoFocus && w.get() != pIgnoreWindow) {
                    // OR windows should add focus to parent
                    if (w->m_bX11ShouldntFocus && w->m_iX11Type != 2)
                        continue;

                    if (box.containsPoint({m_sWLRCursor->x, m_sWLRCursor->y})) {

                        if (w->m_bIsX11 && w->m_iX11Type == 2 && !wlr_xwayland_or_surface_wants_focus(w->m_uSurface.xwayland)) {
                            // Override Redirect
                            return g_pCompositor->m_pLastWindow; // we kinda trick everything here.
                                                                 // TODO: this is wrong, we should focus the parent, but idk how to get it considering it's nullptr in most cases.
                        }

                        return w.get();
                    }

                    if (!w->m_bIsX11) {
                        if (w->hasPopupAt(pos))
                            return w.get();
                    }
                }
            }
        }

        if (properties & FLOATING_ONLY)
            return nullptr;

        const int64_t WORKSPACEID = special ? PMONITOR->specialWorkspaceID : PMONITOR->activeWorkspace;
        const auto    PWORKSPACE  = getWorkspaceByID(WORKSPACEID);

        if (PWORKSPACE->m_bHasFullscreenWindow)
            return getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);

        // for windows, we need to check their extensions too, first.
        for (auto& w : m_vWindows) {
            if (special != isWorkspaceSpecial(w->m_iWorkspaceID))
                continue;

            if (!w->m_bIsX11 && !w->m_bIsFloating && w->m_bIsMapped && w->m_iWorkspaceID == WORKSPACEID && !w->isHidden() && !w->m_bX11ShouldntFocus && !w->m_bNoFocus &&
                w.get() != pIgnoreWindow) {
                if (w->hasPopupAt(pos))
                    return w.get();
            }
        }

        for (auto& w : m_vWindows) {
            if (special != isWorkspaceSpecial(w->m_iWorkspaceID))
                continue;

            CBox box = (properties & USE_PROP_TILED) ? w->getWindowBoxUnified(properties) : CBox{w->m_vPosition, w->m_vSize};
            if (!w->m_bIsFloating && w->m_bIsMapped && box.containsPoint(pos) && w->m_iWorkspaceID == WORKSPACEID && !w->isHidden() && !w->m_bX11ShouldntFocus && !w->m_bNoFocus &&
                w.get() != pIgnoreWindow)
                return w.get();
        }

        return nullptr;
    };

    // special workspace
    if (PMONITOR->specialWorkspaceID && !*PSPECIALFALLTHRU)
        return windowForWorkspace(true);

    if (PMONITOR->specialWorkspaceID) {
        const auto PWINDOW = windowForWorkspace(true);

        if (PWINDOW)
            return PWINDOW;
    }

    return windowForWorkspace(false);
}

wlr_surface* CCompositor::vectorWindowToSurface(const Vector2D& pos, CWindow* pWindow, Vector2D& sl) {

    if (!windowValidMapped(pWindow))
        return nullptr;

    RASSERT(!pWindow->m_bIsX11, "Cannot call vectorWindowToSurface on an X11 window!");

    const auto PSURFACE = pWindow->m_uSurface.xdg;

    double     subx, suby;

    // calc for oversized windows... fucking bullshit, again.
    CBox geom;
    wlr_xdg_surface_get_geometry(pWindow->m_uSurface.xdg, geom.pWlr());
    geom.applyFromWlr();

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

Vector2D CCompositor::vectorToSurfaceLocal(const Vector2D& vec, CWindow* pWindow, wlr_surface* pSurface) {
    if (!windowValidMapped(pWindow))
        return {};

    if (pWindow->m_bIsX11)
        return vec - pWindow->m_vRealPosition.goalv();

    const auto                         PSURFACE = pWindow->m_uSurface.xdg;

    std::tuple<wlr_surface*, int, int> iterData = {pSurface, -1337, -1337};

    wlr_xdg_surface_for_each_surface(
        PSURFACE,
        [](wlr_surface* surf, int x, int y, void* data) {
            const auto PDATA = (std::tuple<wlr_surface*, int, int>*)data;
            if (surf == std::get<0>(*PDATA)) {
                std::get<1>(*PDATA) = x;
                std::get<2>(*PDATA) = y;
            }
        },
        &iterData);

    CBox geom = {};
    wlr_xdg_surface_get_geometry(PSURFACE, geom.pWlr());
    geom.applyFromWlr();

    if (std::get<1>(iterData) == -1337 && std::get<2>(iterData) == -1337)
        return vec - pWindow->m_vRealPosition.goalv();

    return vec - pWindow->m_vRealPosition.goalv() - Vector2D{std::get<1>(iterData), std::get<2>(iterData)} + Vector2D{geom.x, geom.y};
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

    static auto* const PFOLLOWMOUSE        = &g_pConfigManager->getConfigValuePtr("input:follow_mouse")->intValue;
    static auto* const PSPECIALFALLTHROUGH = &g_pConfigManager->getConfigValuePtr("input:special_fallthrough")->intValue;

    if (g_pCompositor->m_sSeat.exclusiveClient) {
        Debug::log(LOG, "Disallowing setting focus to a window due to there being an active input inhibitor layer.");
        return;
    }

    if (!g_pInputManager->m_dExclusiveLSes.empty()) {
        Debug::log(LOG, "Refusing a keyboard focus to a window because of an exclusive ls");
        return;
    }

    g_pLayoutManager->getCurrentLayout()->bringWindowToTop(pWindow);

    if (!pWindow || !windowValidMapped(pWindow)) {

        if (!m_pLastWindow && !pWindow)
            return;

        const auto PLASTWINDOW = m_pLastWindow;
        m_pLastWindow          = nullptr;

        if (windowValidMapped(PLASTWINDOW)) {
            updateWindowAnimatedDecorationValues(PLASTWINDOW);

            g_pXWaylandManager->activateWindow(PLASTWINDOW, false);

            if (PLASTWINDOW->m_phForeignToplevel)
                wlr_foreign_toplevel_handle_v1_set_activated(PLASTWINDOW->m_phForeignToplevel, false);
        }

        wlr_seat_keyboard_notify_clear_focus(m_sSeat.seat);

        g_pEventManager->postEvent(SHyprIPCEvent{"activewindow", ","});
        g_pEventManager->postEvent(SHyprIPCEvent{"activewindowv2", ","});

        EMIT_HOOK_EVENT("activeWindow", (CWindow*)nullptr);

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

    const auto PMONITOR = getMonitorFromID(pWindow->m_iMonitorID);

    if (!isWorkspaceVisible(pWindow->m_iWorkspaceID)) {
        const auto PWORKSPACE = getWorkspaceByID(pWindow->m_iWorkspaceID);
        // This is to fix incorrect feedback on the focus history.
        PWORKSPACE->m_pLastFocusedWindow = pWindow;
        PWORKSPACE->rememberPrevWorkspace(getWorkspaceByID(m_pLastMonitor->activeWorkspace));
        PMONITOR->changeWorkspace(PWORKSPACE, false, true);
        // changeworkspace already calls focusWindow
        return;
    }

    const auto PLASTWINDOW = m_pLastWindow;
    m_pLastWindow          = pWindow;

    /* If special fallthrough is enabled, this behavior will be disabled, as I have no better idea of nicely tracking which
       window focuses are "via keybinds" and which ones aren't. */
    if (PMONITOR->specialWorkspaceID && PMONITOR->specialWorkspaceID != pWindow->m_iWorkspaceID && !pWindow->m_bPinned && !*PSPECIALFALLTHROUGH)
        PMONITOR->setSpecialWorkspace(nullptr);

    // we need to make the PLASTWINDOW not equal to m_pLastWindow so that RENDERDATA is correct for an unfocused window
    if (windowValidMapped(PLASTWINDOW)) {
        PLASTWINDOW->updateDynamicRules();

        updateWindowAnimatedDecorationValues(PLASTWINDOW);

        if (!pWindow->m_bIsX11 || pWindow->m_iX11Type == 1)
            g_pXWaylandManager->activateWindow(PLASTWINDOW, false);

        if (PLASTWINDOW->m_phForeignToplevel)
            wlr_foreign_toplevel_handle_v1_set_activated(PLASTWINDOW->m_phForeignToplevel, false);
    }

    m_pLastWindow = PLASTWINDOW;

    const auto PWINDOWSURFACE = pSurface ? pSurface : pWindow->m_pWLSurface.wlr();

    focusSurface(PWINDOWSURFACE, pWindow);

    g_pXWaylandManager->activateWindow(pWindow, true); // sets the m_pLastWindow

    pWindow->updateDynamicRules();

    updateWindowAnimatedDecorationValues(pWindow);

    if (pWindow->m_bIsUrgent)
        pWindow->m_bIsUrgent = false;

    // Send an event
    g_pEventManager->postEvent(SHyprIPCEvent{"activewindow", g_pXWaylandManager->getAppIDClass(pWindow) + "," + pWindow->m_szTitle});
    g_pEventManager->postEvent(SHyprIPCEvent{"activewindowv2", std::format("{:x}", (uintptr_t)pWindow)});

    EMIT_HOOK_EVENT("activeWindow", pWindow);

    g_pLayoutManager->getCurrentLayout()->onWindowFocusChange(pWindow);

    // TODO: implement this better
    if (!PLASTWINDOW && pWindow->m_sGroupData.pNextWindow) {
        for (auto curr = pWindow->m_sGroupData.pNextWindow; curr != pWindow; curr = curr->m_sGroupData.pNextWindow) {
            if (curr->m_phForeignToplevel)
                wlr_foreign_toplevel_handle_v1_set_activated(curr->m_phForeignToplevel, false);
        }
    }

    if (pWindow->m_phForeignToplevel)
        wlr_foreign_toplevel_handle_v1_set_activated(pWindow->m_phForeignToplevel, true);

    if (!pWindow->m_bIsX11) {
        const auto PCONSTRAINT = wlr_pointer_constraints_v1_constraint_for_surface(m_sWLRPointerConstraints, pWindow->m_uSurface.xdg->surface, m_sSeat.seat);

        if (PCONSTRAINT)
            g_pInputManager->constrainMouse(m_sSeat.mouse, PCONSTRAINT);
    }

    g_pInputManager->recheckIdleInhibitorStatus();

    // move to front of the window history
    const auto HISTORYPIVOT = std::find_if(m_vWindowFocusHistory.begin(), m_vWindowFocusHistory.end(), [&](const auto& other) { return other == pWindow; });
    if (HISTORYPIVOT == m_vWindowFocusHistory.end()) {
        Debug::log(ERR, "BUG THIS: {} has no pivot in history", pWindow);
    } else {
        std::rotate(m_vWindowFocusHistory.begin(), HISTORYPIVOT, HISTORYPIVOT + 1);
    }

    if (*PFOLLOWMOUSE == 0)
        g_pInputManager->sendMotionEventsToFocused();
}

void CCompositor::focusSurface(wlr_surface* pSurface, CWindow* pWindowOwner) {

    if (m_sSeat.seat->keyboard_state.focused_surface == pSurface || (pWindowOwner && m_sSeat.seat->keyboard_state.focused_surface == pWindowOwner->m_pWLSurface.wlr()))
        return; // Don't focus when already focused on this.

    if (g_pSessionLockManager->isSessionLocked() && !g_pSessionLockManager->isSurfaceSessionLock(pSurface))
        return;

    // Unfocus last surface if should
    if (m_pLastFocus && !pWindowOwner)
        g_pXWaylandManager->activateSurface(m_pLastFocus, false);

    if (!pSurface) {
        wlr_seat_keyboard_clear_focus(m_sSeat.seat);
        g_pEventManager->postEvent(SHyprIPCEvent{"activewindow", ","}); // unfocused
        g_pEventManager->postEvent(SHyprIPCEvent{"activewindowv2", ","});
        EMIT_HOOK_EVENT("keyboardFocus", (wlr_surface*)nullptr);
        m_pLastFocus = nullptr;
        return;
    }

    if (const auto KEYBOARD = wlr_seat_get_keyboard(m_sSeat.seat); KEYBOARD) {
        uint32_t keycodes[WLR_KEYBOARD_KEYS_CAP] = {0}; // TODO: maybe send valid, non-keybind codes?
        wlr_seat_keyboard_notify_enter(m_sSeat.seat, pSurface, keycodes, 0, &KEYBOARD->modifiers);

        wlr_seat_keyboard_focus_change_event event = {
            .seat        = m_sSeat.seat,
            .old_surface = m_pLastFocus,
            .new_surface = pSurface,
        };
        wl_signal_emit_mutable(&m_sSeat.seat->keyboard_state.events.focus_change, &event);
    }

    if (pWindowOwner)
        Debug::log(LOG, "Set keyboard focus to surface {:x}, with {}", (uintptr_t)pSurface, pWindowOwner);
    else
        Debug::log(LOG, "Set keyboard focus to surface {:x}", (uintptr_t)pSurface);

    g_pXWaylandManager->activateSurface(pSurface, true);
    m_pLastFocus = pSurface;

    EMIT_HOOK_EVENT("keyboardFocus", pSurface);
}

bool CCompositor::windowValidMapped(CWindow* pWindow) {
    if (!pWindow)
        return false;

    if (!windowExists(pWindow))
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

wlr_surface* CCompositor::vectorToLayerSurface(const Vector2D& pos, std::vector<std::unique_ptr<SLayerSurface>>* layerSurfaces, Vector2D* sCoords,
                                               SLayerSurface** ppLayerSurfaceFound) {
    for (auto& ls : *layerSurfaces | std::views::reverse) {
        if (ls->fadingOut || !ls->layerSurface || (ls->layerSurface && !ls->layerSurface->surface->mapped) || ls->alpha.fl() == 0.f)
            continue;

        auto SURFACEAT = wlr_layer_surface_v1_surface_at(ls->layerSurface, pos.x - ls->geometry.x, pos.y - ls->geometry.y, &sCoords->x, &sCoords->y);

        if (SURFACEAT) {
            if (!pixman_region32_not_empty(&SURFACEAT->input_region))
                continue;

            *ppLayerSurfaceFound = ls.get();
            return SURFACEAT;
        }
    }

    return nullptr;
}

SIMEPopup* CCompositor::vectorToIMEPopup(const Vector2D& pos, std::list<SIMEPopup>& popups) {
    for (auto& popup : popups) {
        auto surface = popup.pSurface->surface;
        CBox box{
            popup.realX,
            popup.realY,
            surface->current.width,
            surface->current.height,
        };
        if (box.containsPoint(pos))
            return &popup;
    }
    return nullptr;
}

CWindow* CCompositor::getWindowFromSurface(wlr_surface* pSurface) {
    for (auto& w : m_vWindows) {
        if (!w->m_bIsMapped || w->m_bFadingOut)
            continue;

        if (w->m_pWLSurface.wlr() == pSurface)
            return w.get();
    }

    return nullptr;
}

CWindow* CCompositor::getWindowFromHandle(uint32_t handle) {
    for (auto& w : m_vWindows) {
        if ((uint32_t)(((uint64_t)w.get()) & 0xFFFFFFFF) == handle) {
            return w.get();
        }
    }

    return nullptr;
}

CWindow* CCompositor::getWindowFromZWLRHandle(wl_resource* handle) {
    for (auto& w : m_vWindows) {
        if (!w->m_bIsMapped || w->isHidden() || !w->m_phForeignToplevel)
            continue;

        wl_resource* current;

        wl_list_for_each(current, &w->m_phForeignToplevel->resources, link) {
            if (current == handle) {
                return w.get();
            }
        }
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

        if (m->specialWorkspaceID == w)
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
    auto it = m_vWorkspaces.begin();
    while (it != m_vWorkspaces.end()) {

        const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(it->get());
        if (WORKSPACERULE.isPersistent) {
            ++it;
            continue;
        }

        const auto& WORKSPACE          = *it;
        const auto  WINDOWSONWORKSPACE = getWindowsOnWorkspace(WORKSPACE->m_iID);

        if (WINDOWSONWORKSPACE == 0) {
            if (!isWorkspaceVisible(WORKSPACE->m_iID)) {

                if (WORKSPACE->m_bIsSpecialWorkspace) {
                    if (WORKSPACE->m_fAlpha.fl() > 0.f /* don't abruptly end the fadeout */) {
                        ++it;
                        continue;
                    }

                    const auto PMONITOR = getMonitorFromID(WORKSPACE->m_iMonitorID);

                    if (PMONITOR && PMONITOR->specialWorkspaceID == WORKSPACE->m_iID)
                        PMONITOR->setSpecialWorkspace(nullptr);
                }

                it = m_vWorkspaces.erase(it);
                continue;
            }
            if (!WORKSPACE->m_bOnCreatedEmptyExecuted) {
                if (auto cmd = WORKSPACERULE.onCreatedEmptyRunCmd)
                    g_pKeybindManager->spawn(*cmd);

                WORKSPACE->m_bOnCreatedEmptyExecuted = true;
            }
        }

        ++it;
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

CWindow* CCompositor::getUrgentWindow() {
    for (auto& w : m_vWindows) {
        if (w->m_bIsMapped && w->m_bIsUrgent)
            return w.get();
    }

    return nullptr;
}

bool CCompositor::hasUrgentWindowOnWorkspace(const int& id) {
    for (auto& w : m_vWindows) {
        if (w->m_iWorkspaceID == id && w->m_bIsMapped && w->m_bIsUrgent)
            return true;
    }

    return false;
}

CWindow* CCompositor::getFirstWindowOnWorkspace(const int& id) {
    for (auto& w : m_vWindows) {
        if (w->m_iWorkspaceID == id && w->m_bIsMapped && !w->isHidden())
            return w.get();
    }

    return nullptr;
}

CWindow* CCompositor::getTopLeftWindowOnWorkspace(const int& id) {
    const auto PWORKSPACE = getWorkspaceByID(id);

    if (!PWORKSPACE)
        return nullptr;

    const auto PMONITOR = getMonitorFromID(PWORKSPACE->m_iMonitorID);

    for (auto& w : m_vWindows) {
        if (w->m_iWorkspaceID != id || !w->m_bIsMapped || w->isHidden())
            continue;

        const auto WINDOWIDEALBB = w->getWindowIdealBoundingBoxIgnoreReserved();

        if (WINDOWIDEALBB.x <= PMONITOR->vecPosition.x + 1 && WINDOWIDEALBB.y <= PMONITOR->vecPosition.y + 1)
            return w.get();
    }
    return nullptr;
}

bool CCompositor::doesSeatAcceptInput(wlr_surface* surface) {
    if (g_pSessionLockManager->isSessionLocked()) {
        if (g_pSessionLockManager->isSurfaceSessionLock(surface))
            return true;

        if (surface && m_sSeat.exclusiveClient == wl_resource_get_client(surface->resource))
            return true;

        return false;
    }

    if (m_sSeat.exclusiveClient) {
        if (surface && m_sSeat.exclusiveClient == wl_resource_get_client(surface->resource))
            return true;

        return false;
    }

    return true;
}

bool CCompositor::isWindowActive(CWindow* pWindow) {
    if (!m_pLastWindow && !m_pLastFocus)
        return false;

    if (!windowValidMapped(pWindow))
        return false;

    const auto PSURFACE = pWindow->m_pWLSurface.wlr();

    return PSURFACE == m_pLastFocus || pWindow == m_pLastWindow;
}

void CCompositor::changeWindowZOrder(CWindow* pWindow, bool top) {
    if (!windowValidMapped(pWindow))
        return;

    auto moveToZ = [&](CWindow* pw, bool top) -> void {
        if (top) {
            for (auto it = m_vWindows.begin(); it != m_vWindows.end(); ++it) {
                if (it->get() == pw) {
                    std::rotate(it, it + 1, m_vWindows.end());
                    break;
                }
            }
        } else {
            for (auto it = m_vWindows.rbegin(); it != m_vWindows.rend(); ++it) {
                if (it->get() == pw) {
                    std::rotate(it, it + 1, m_vWindows.rend());
                    break;
                }
            }
        }

        if (pw->m_bIsMapped)
            g_pHyprRenderer->damageMonitor(getMonitorFromID(pw->m_iMonitorID));
    };

    if (top)
        pWindow->m_bCreatedOverFullscreen = true;

    if (!pWindow->m_bIsX11) {
        moveToZ(pWindow, top);
        return;
    } else {
        // move X11 window stack

        std::deque<CWindow*> toMove;

        auto                 x11Stack = [&](CWindow* pw, bool top, auto&& x11Stack) -> void {
            if (top)
                toMove.emplace_back(pw);
            else
                toMove.emplace_front(pw);

            for (auto& w : m_vWindows) {
                if (w->m_bIsMapped && !w->isHidden() && w->m_bIsX11 && w->X11TransientFor() == pw) {
                    x11Stack(w.get(), top, x11Stack);
                }
            }
        };

        x11Stack(pWindow, top, x11Stack);

        for (auto it : toMove) {
            moveToZ(it, top);
        }
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

            w->m_bFadingOut = false;
            removeWindowFromVectorSafe(w);
            std::erase(m_vWindowsFadingOut, w);

            Debug::log(LOG, "Cleanup: destroyed a window");
            return;
        }
    }

    for (auto& ls : m_vSurfacesFadingOut) {

        // sometimes somehow fucking happens wtf
        bool exists = false;
        for (auto& m : m_vMonitors) {
            for (auto& lsl : m->m_aLayerSurfaceLayers) {
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
            std::erase(m_vSurfacesFadingOut, ls);

            Debug::log(LOG, "Fading out a non-existent LS??");

            return;
        }

        if (ls->monitorID != monid)
            continue;

        // mark blur for recalc
        if (ls->layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND || ls->layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM)
            g_pHyprOpenGL->markBlurDirtyForMonitor(getMonitorFromID(monid));

        if (ls->fadingOut && ls->readyToDelete && !ls->alpha.isBeingAnimated()) {
            for (auto& m : m_vMonitors) {
                for (auto& lsl : m->m_aLayerSurfaceLayers) {
                    if (!lsl.empty() && std::find_if(lsl.begin(), lsl.end(), [&](std::unique_ptr<SLayerSurface>& other) { return other.get() == ls; }) != lsl.end()) {
                        std::erase_if(lsl, [&](std::unique_ptr<SLayerSurface>& other) { return other.get() == ls; });
                    }
                }
            }

            std::erase(m_vSurfacesFadingOut, ls);

            Debug::log(LOG, "Cleanup: destroyed a layersurface");

            glFlush(); // to free mem NOW.
            return;
        }
    }
}

void CCompositor::addToFadingOutSafe(SLayerSurface* pLS) {
    const auto FOUND = std::find_if(m_vSurfacesFadingOut.begin(), m_vSurfacesFadingOut.end(), [&](SLayerSurface* other) { return other == pLS; });

    if (FOUND != m_vSurfacesFadingOut.end())
        return; // if it's already added, don't add it.

    m_vSurfacesFadingOut.emplace_back(pLS);
}

void CCompositor::addToFadingOutSafe(CWindow* pWindow) {
    const auto FOUND = std::find_if(m_vWindowsFadingOut.begin(), m_vWindowsFadingOut.end(), [&](CWindow* other) { return other == pWindow; });

    if (FOUND != m_vWindowsFadingOut.end())
        return; // if it's already added, don't add it.

    m_vWindowsFadingOut.emplace_back(pWindow);
}

CWindow* CCompositor::getWindowInDirection(CWindow* pWindow, char dir) {

    if (!isDirection(dir))
        return nullptr;

    // 0 -> history, 1 -> shared length
    static auto* const PMETHOD = &g_pConfigManager->getConfigValuePtr("binds:focus_preferred_method")->intValue;

    const auto         PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    if (!PMONITOR)
        return nullptr; // ??

    const auto WINDOWIDEALBB = pWindow->m_bIsFullscreen ? wlr_box{(int)PMONITOR->vecPosition.x, (int)PMONITOR->vecPosition.y, (int)PMONITOR->vecSize.x, (int)PMONITOR->vecSize.y} :
                                                          pWindow->getWindowIdealBoundingBoxIgnoreReserved();

    const auto POSA  = Vector2D(WINDOWIDEALBB.x, WINDOWIDEALBB.y);
    const auto SIZEA = Vector2D(WINDOWIDEALBB.width, WINDOWIDEALBB.height);

    const auto PWORKSPACE   = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);
    auto       leaderValue  = -1;
    CWindow*   leaderWindow = nullptr;

    if (!pWindow->m_bIsFloating) {

        // for tiled windows, we calc edges
        for (auto& w : m_vWindows) {
            if (w.get() == pWindow || !w->m_bIsMapped || w->isHidden() || w->m_bIsFloating || !isWorkspaceVisible(w->m_iWorkspaceID))
                continue;

            if (pWindow->m_iMonitorID == w->m_iMonitorID && pWindow->m_iWorkspaceID != w->m_iWorkspaceID)
                continue;

            if (PWORKSPACE->m_bHasFullscreenWindow && !w->m_bIsFullscreen && !w->m_bCreatedOverFullscreen)
                continue;

            const auto BWINDOWIDEALBB = w->getWindowIdealBoundingBoxIgnoreReserved();

            const auto POSB  = Vector2D(BWINDOWIDEALBB.x, BWINDOWIDEALBB.y);
            const auto SIZEB = Vector2D(BWINDOWIDEALBB.width, BWINDOWIDEALBB.height);

            double     intersectLength = -1;

            switch (dir) {
                case 'l':
                    if (STICKS(POSA.x, POSB.x + SIZEB.x)) {
                        intersectLength = std::max(0.0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                    }
                    break;
                case 'r':
                    if (STICKS(POSA.x + SIZEA.x, POSB.x)) {
                        intersectLength = std::max(0.0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                    }
                    break;
                case 't':
                case 'u':
                    if (STICKS(POSA.y, POSB.y + SIZEB.y)) {
                        intersectLength = std::max(0.0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    }
                    break;
                case 'b':
                case 'd':
                    if (STICKS(POSA.y + SIZEA.y, POSB.y)) {
                        intersectLength = std::max(0.0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    }
                    break;
            }

            if (*PMETHOD == 0 /* history */) {
                if (intersectLength > 0) {

                    // get idx
                    int windowIDX = -1;
                    for (size_t i = 0; i < g_pCompositor->m_vWindowFocusHistory.size(); ++i) {
                        if (g_pCompositor->m_vWindowFocusHistory[i] == w.get()) {
                            windowIDX = i;
                            break;
                        }
                    }

                    windowIDX = g_pCompositor->m_vWindowFocusHistory.size() - windowIDX;

                    if (windowIDX > leaderValue) {
                        leaderValue  = windowIDX;
                        leaderWindow = w.get();
                    }
                }
            } else /* length */ {
                if (intersectLength > leaderValue) {
                    leaderValue  = intersectLength;
                    leaderWindow = w.get();
                }
            }
        }
    } else {
        // for floating windows, we calculate best distance and angle.
        // if there is a window with angle better than THRESHOLD, only distance counts

        if (dir == 'u')
            dir = 't';
        if (dir == 'd')
            dir = 'b';

        static const std::unordered_map<char, Vector2D> VECTORS = {{'r', {1, 0}}, {'t', {0, -1}}, {'b', {0, 1}}, {'l', {-1, 0}}};

        //
        auto vectorAngles = [](Vector2D a, Vector2D b) -> double {
            double dot = a.x * b.x + a.y * b.y;
            double ang = std::acos(dot / (a.size() * b.size()));
            return ang;
        };

        float           bestAngleAbs = 2.0 * M_PI;
        constexpr float THRESHOLD    = 0.3 * M_PI;

        for (auto& w : m_vWindows) {
            if (w.get() == pWindow || !w->m_bIsMapped || w->isHidden() || !w->m_bIsFloating || !isWorkspaceVisible(w->m_iWorkspaceID))
                continue;

            if (pWindow->m_iMonitorID == w->m_iMonitorID && pWindow->m_iWorkspaceID != w->m_iWorkspaceID)
                continue;

            if (PWORKSPACE->m_bHasFullscreenWindow && !w->m_bIsFullscreen && !w->m_bCreatedOverFullscreen)
                continue;

            const auto DIST  = w->middle().distance(pWindow->middle());
            const auto ANGLE = vectorAngles(Vector2D{w->middle() - pWindow->middle()}, VECTORS.at(dir));

            if (ANGLE > M_PI_2)
                continue; // if the angle is over 90 degrees, ignore. Wrong direction entirely.

            if ((bestAngleAbs < THRESHOLD && DIST < leaderValue && ANGLE < THRESHOLD) || (ANGLE < bestAngleAbs && bestAngleAbs > THRESHOLD) || leaderValue == -1) {
                leaderValue  = DIST;
                bestAngleAbs = ANGLE;
                leaderWindow = w.get();
            }
        }

        if (!leaderWindow && PWORKSPACE->m_bHasFullscreenWindow)
            leaderWindow = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);
    }

    if (leaderValue != -1)
        return leaderWindow;

    return nullptr;
}

CWindow* CCompositor::getNextWindowOnWorkspace(CWindow* pWindow, bool focusableOnly, std::optional<bool> floating) {
    bool gotToWindow = false;
    for (auto& w : m_vWindows) {
        if (w.get() != pWindow && !gotToWindow)
            continue;

        if (w.get() == pWindow) {
            gotToWindow = true;
            continue;
        }

        if (floating.has_value() && w->m_bIsFloating != floating.value())
            continue;

        if (w->m_iWorkspaceID == pWindow->m_iWorkspaceID && w->m_bIsMapped && !w->isHidden() && (!focusableOnly || !w->m_bNoFocus))
            return w.get();
    }

    for (auto& w : m_vWindows) {
        if (floating.has_value() && w->m_bIsFloating != floating.value())
            continue;

        if (w.get() != pWindow && w->m_iWorkspaceID == pWindow->m_iWorkspaceID && w->m_bIsMapped && !w->isHidden() && (!focusableOnly || !w->m_bNoFocus))
            return w.get();
    }

    return nullptr;
}

CWindow* CCompositor::getPrevWindowOnWorkspace(CWindow* pWindow, bool focusableOnly, std::optional<bool> floating) {
    bool gotToWindow = false;
    for (auto& w : m_vWindows | std::views::reverse) {
        if (w.get() != pWindow && !gotToWindow)
            continue;

        if (w.get() == pWindow) {
            gotToWindow = true;
            continue;
        }

        if (floating.has_value() && w->m_bIsFloating != floating.value())
            continue;

        if (w->m_iWorkspaceID == pWindow->m_iWorkspaceID && w->m_bIsMapped && !w->isHidden() && (!focusableOnly || !w->m_bNoFocus))
            return w.get();
    }

    for (auto& w : m_vWindows | std::views::reverse) {
        if (floating.has_value() && w->m_bIsFloating != floating.value())
            continue;

        if (w.get() != pWindow && w->m_iWorkspaceID == pWindow->m_iWorkspaceID && w->m_bIsMapped && !w->isHidden() && (!focusableOnly || !w->m_bNoFocus))
            return w.get();
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
    if (str.starts_with("name:")) {
        return getWorkspaceByName(str.substr(str.find_first_of(':') + 1));
    }

    try {
        std::string name = "";
        return getWorkspaceByID(getWorkspaceIDFromString(str, name));
    } catch (std::exception& e) { Debug::log(ERR, "Error in getWorkspaceByString, invalid id"); }

    return nullptr;
}

bool CCompositor::isPointOnAnyMonitor(const Vector2D& point) {
    for (auto& m : m_vMonitors) {
        if (VECINRECT(point, m->vecPosition.x, m->vecPosition.y, m->vecSize.x + m->vecPosition.x, m->vecSize.y + m->vecPosition.y))
            return true;
    }

    return false;
}

bool CCompositor::isPointOnReservedArea(const Vector2D& point, const CMonitor* pMonitor) {
    const auto PMONITOR = pMonitor ? pMonitor : getMonitorFromVector(point);

    const auto XY1 = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
    const auto XY2 = PMONITOR->vecPosition + PMONITOR->vecSize - PMONITOR->vecReservedBottomRight;

    return !VECINRECT(point, XY1.x, XY1.y, XY2.x, XY2.y);
}

void checkFocusSurfaceIter(wlr_surface* pSurface, int x, int y, void* data) {
    auto pair    = (std::pair<wlr_surface*, bool>*)data;
    pair->second = pair->second || pSurface == pair->first;
}

CWindow* CCompositor::getConstraintWindow(SMouse* pMouse) {
    if (!pMouse->currentConstraint)
        return nullptr;

    const auto PSURFACE = pMouse->currentConstraint->surface;

    for (auto& w : m_vWindows) {
        if (w->isHidden() || !w->m_bIsMapped || !w->m_pWLSurface.exists())
            continue;

        if (w->m_bIsX11) {
            if (PSURFACE == w->m_pWLSurface.wlr())
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
    const auto POSA  = m_pLastMonitor->vecPosition;
    const auto SIZEA = m_pLastMonitor->vecSize;

    auto       longestIntersect        = -1;
    CMonitor*  longestIntersectMonitor = nullptr;

    for (auto& m : m_vMonitors) {
        if (m.get() == m_pLastMonitor)
            continue;

        const auto POSB  = m->vecPosition;
        const auto SIZEB = m->vecSize;
        switch (dir) {
            case 'l':
                if (STICKS(POSA.x, POSB.x + SIZEB.x)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect        = INTERSECTLEN;
                        longestIntersectMonitor = m.get();
                    }
                }
                break;
            case 'r':
                if (STICKS(POSA.x + SIZEA.x, POSB.x)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect        = INTERSECTLEN;
                        longestIntersectMonitor = m.get();
                    }
                }
                break;
            case 't':
            case 'u':
                if (STICKS(POSA.y, POSB.y + SIZEB.y)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect        = INTERSECTLEN;
                        longestIntersectMonitor = m.get();
                    }
                }
                break;
            case 'b':
            case 'd':
                if (STICKS(POSA.y + SIZEA.y, POSB.y)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect        = INTERSECTLEN;
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

void CCompositor::updateWorkspaceWindows(const int64_t& id) {
    for (auto& w : m_vWindows) {
        if (!w->m_bIsMapped || w->m_iWorkspaceID != id)
            continue;

        w->updateDynamicRules();
    }
}

void CCompositor::updateWindowAnimatedDecorationValues(CWindow* pWindow) {
    // optimization
    static auto* const ACTIVECOL              = (CGradientValueData*)g_pConfigManager->getConfigValuePtr("general:col.active_border")->data.get();
    static auto* const INACTIVECOL            = (CGradientValueData*)g_pConfigManager->getConfigValuePtr("general:col.inactive_border")->data.get();
    static auto* const NOGROUPACTIVECOL       = (CGradientValueData*)g_pConfigManager->getConfigValuePtr("general:col.nogroup_border_active")->data.get();
    static auto* const NOGROUPINACTIVECOL     = (CGradientValueData*)g_pConfigManager->getConfigValuePtr("general:col.nogroup_border")->data.get();
    static auto* const GROUPACTIVECOL         = (CGradientValueData*)g_pConfigManager->getConfigValuePtr("group:col.border_active")->data.get();
    static auto* const GROUPINACTIVECOL       = (CGradientValueData*)g_pConfigManager->getConfigValuePtr("group:col.border_inactive")->data.get();
    static auto* const GROUPACTIVELOCKEDCOL   = (CGradientValueData*)g_pConfigManager->getConfigValuePtr("group:col.border_locked_active")->data.get();
    static auto* const GROUPINACTIVELOCKEDCOL = (CGradientValueData*)g_pConfigManager->getConfigValuePtr("group:col.border_locked_inactive")->data.get();
    static auto* const PINACTIVEALPHA         = &g_pConfigManager->getConfigValuePtr("decoration:inactive_opacity")->floatValue;
    static auto* const PACTIVEALPHA           = &g_pConfigManager->getConfigValuePtr("decoration:active_opacity")->floatValue;
    static auto* const PFULLSCREENALPHA       = &g_pConfigManager->getConfigValuePtr("decoration:fullscreen_opacity")->floatValue;
    static auto* const PSHADOWCOL             = &g_pConfigManager->getConfigValuePtr("decoration:col.shadow")->intValue;
    static auto* const PSHADOWCOLINACTIVE     = &g_pConfigManager->getConfigValuePtr("decoration:col.shadow_inactive")->intValue;
    static auto* const PDIMSTRENGTH           = &g_pConfigManager->getConfigValuePtr("decoration:dim_strength")->floatValue;
    static auto* const PDIMENABLED            = &g_pConfigManager->getConfigValuePtr("decoration:dim_inactive")->intValue;

    auto               setBorderColor = [&](CGradientValueData grad) -> void {
        if (grad == pWindow->m_cRealBorderColor)
            return;

        pWindow->m_cRealBorderColorPrevious = pWindow->m_cRealBorderColor;
        pWindow->m_cRealBorderColor         = grad;
        pWindow->m_fBorderFadeAnimationProgress.setValueAndWarp(0.f);
        pWindow->m_fBorderFadeAnimationProgress = 1.f;
    };

    // border
    const auto RENDERDATA = g_pLayoutManager->getCurrentLayout()->requestRenderHints(pWindow);
    if (RENDERDATA.isBorderGradient)
        setBorderColor(*RENDERDATA.borderGradient);
    else {
        const bool GROUPLOCKED = pWindow->m_sGroupData.pNextWindow ? pWindow->getGroupHead()->m_sGroupData.locked : false;
        if (pWindow == m_pLastWindow) {
            const auto* const ACTIVECOLOR =
                !pWindow->m_sGroupData.pNextWindow ? (!pWindow->m_sGroupData.deny ? ACTIVECOL : NOGROUPACTIVECOL) : (GROUPLOCKED ? GROUPACTIVELOCKEDCOL : GROUPACTIVECOL);
            setBorderColor(pWindow->m_sSpecialRenderData.activeBorderColor.toUnderlying().m_vColors.empty() ? *ACTIVECOLOR :
                                                                                                              pWindow->m_sSpecialRenderData.activeBorderColor.toUnderlying());
        } else {
            const auto* const INACTIVECOLOR =
                !pWindow->m_sGroupData.pNextWindow ? (!pWindow->m_sGroupData.deny ? INACTIVECOL : NOGROUPINACTIVECOL) : (GROUPLOCKED ? GROUPINACTIVELOCKEDCOL : GROUPINACTIVECOL);
            setBorderColor(pWindow->m_sSpecialRenderData.inactiveBorderColor.toUnderlying().m_vColors.empty() ? *INACTIVECOLOR :
                                                                                                                pWindow->m_sSpecialRenderData.inactiveBorderColor.toUnderlying());
        }
    }

    // tick angle if it's not running (aka dead)
    if (!pWindow->m_fBorderAngleAnimationProgress.isBeingAnimated())
        pWindow->m_fBorderAngleAnimationProgress.setValueAndWarp(0.f);

    // opacity
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);
    if (pWindow->m_bIsFullscreen && PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL) {
        pWindow->m_fActiveInactiveAlpha = *PFULLSCREENALPHA;
    } else {
        if (pWindow == m_pLastWindow)
            pWindow->m_fActiveInactiveAlpha = pWindow->m_sSpecialRenderData.alphaOverride.toUnderlying() ? pWindow->m_sSpecialRenderData.alpha.toUnderlying() :
                                                                                                           pWindow->m_sSpecialRenderData.alpha.toUnderlying() * *PACTIVEALPHA;
        else
            pWindow->m_fActiveInactiveAlpha = pWindow->m_sSpecialRenderData.alphaInactive.toUnderlying() != -1 ?
                (pWindow->m_sSpecialRenderData.alphaInactiveOverride.toUnderlying() ? pWindow->m_sSpecialRenderData.alphaInactive.toUnderlying() :
                                                                                      pWindow->m_sSpecialRenderData.alphaInactive.toUnderlying() * *PINACTIVEALPHA) :
                *PINACTIVEALPHA;
    }

    // dim
    if (pWindow == m_pLastWindow || pWindow->m_sAdditionalConfigData.forceNoDim || !*PDIMENABLED) {
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

    pWindow->updateWindowDecos();
}

int CCompositor::getNextAvailableMonitorID(std::string const& name) {
    // reuse ID if it's already in the map, and the monitor with that ID is not being used by another monitor
    if (m_mMonitorIDMap.contains(name) && !std::any_of(m_vRealMonitors.begin(), m_vRealMonitors.end(), [&](auto m) { return m->ID == m_mMonitorIDMap[name]; }))
        return m_mMonitorIDMap[name];

    // otherwise, find minimum available ID that is not in the map
    std::unordered_set<uint64_t> usedIDs;
    for (auto const& monitor : m_vRealMonitors) {
        usedIDs.insert(monitor->ID);
    }

    uint64_t nextID = 0;
    while (usedIDs.count(nextID) > 0) {
        nextID++;
    }
    m_mMonitorIDMap[name] = nextID;
    return nextID;
}

void CCompositor::swapActiveWorkspaces(CMonitor* pMonitorA, CMonitor* pMonitorB) {

    const auto PWORKSPACEA = g_pCompositor->getWorkspaceByID(pMonitorA->activeWorkspace);
    const auto PWORKSPACEB = g_pCompositor->getWorkspaceByID(pMonitorB->activeWorkspace);

    PWORKSPACEA->m_iMonitorID = pMonitorB->ID;
    PWORKSPACEA->moveToMonitor(pMonitorB->ID);

    for (auto& w : m_vWindows) {
        if (w->m_iWorkspaceID == PWORKSPACEA->m_iID) {
            if (w->m_bPinned) {
                w->m_iWorkspaceID = PWORKSPACEB->m_iID;
                continue;
            }

            w->m_iMonitorID = pMonitorB->ID;

            // additionally, move floating and fs windows manually
            if (w->m_bIsFloating)
                w->m_vRealPosition = w->m_vRealPosition.vec() - pMonitorA->vecPosition + pMonitorB->vecPosition;

            if (w->m_bIsFullscreen) {
                w->m_vRealPosition = pMonitorB->vecPosition;
                w->m_vRealSize     = pMonitorB->vecSize;
            }

            w->updateToplevel();
        }
    }

    PWORKSPACEB->m_iMonitorID = pMonitorA->ID;
    PWORKSPACEB->moveToMonitor(pMonitorA->ID);

    for (auto& w : m_vWindows) {
        if (w->m_iWorkspaceID == PWORKSPACEB->m_iID) {
            if (w->m_bPinned) {
                w->m_iWorkspaceID = PWORKSPACEA->m_iID;
                continue;
            }

            w->m_iMonitorID = pMonitorA->ID;

            // additionally, move floating and fs windows manually
            if (w->m_bIsFloating)
                w->m_vRealPosition = w->m_vRealPosition.vec() - pMonitorB->vecPosition + pMonitorA->vecPosition;

            if (w->m_bIsFullscreen) {
                w->m_vRealPosition = pMonitorA->vecPosition;
                w->m_vRealSize     = pMonitorA->vecSize;
            }

            w->updateToplevel();
        }
    }

    pMonitorA->activeWorkspace = PWORKSPACEB->m_iID;
    pMonitorB->activeWorkspace = PWORKSPACEA->m_iID;

    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(pMonitorA->ID);
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(pMonitorB->ID);

    updateFullscreenFadeOnWorkspace(PWORKSPACEB);
    updateFullscreenFadeOnWorkspace(PWORKSPACEA);

    g_pInputManager->sendMotionEventsToFocused();

    // event
    g_pEventManager->postEvent(SHyprIPCEvent{"moveworkspace", PWORKSPACEA->m_szName + "," + pMonitorB->szName});
    EMIT_HOOK_EVENT("moveWorkspace", (std::vector<void*>{PWORKSPACEA, pMonitorB}));
    g_pEventManager->postEvent(SHyprIPCEvent{"moveworkspace", PWORKSPACEB->m_szName + "," + pMonitorA->szName});
    EMIT_HOOK_EVENT("moveWorkspace", (std::vector<void*>{PWORKSPACEB, pMonitorA}));
}

CMonitor* CCompositor::getMonitorFromString(const std::string& name) {
    if (name[0] == '+' || name[0] == '-') {
        // relative

        if (m_vMonitors.size() == 1)
            return m_vMonitors.begin()->get();

        const auto OFFSET = name[0] == '-' ? name : name.substr(1);

        if (!isNumber(OFFSET)) {
            Debug::log(ERR, "Error in getMonitorFromString: Not a number in relative.");
            return nullptr;
        }

        int offsetLeft = std::stoi(OFFSET);
        offsetLeft     = offsetLeft < 0 ? -((-offsetLeft) % m_vMonitors.size()) : offsetLeft % m_vMonitors.size();

        int currentPlace = 0;
        for (int i = 0; i < (int)m_vMonitors.size(); i++) {
            if (m_vMonitors[i].get() == m_pLastMonitor) {
                currentPlace = i;
                break;
            }
        }

        currentPlace += offsetLeft;

        if (currentPlace < 0) {
            currentPlace = m_vMonitors.size() + currentPlace;
        } else {
            currentPlace = currentPlace % m_vMonitors.size();
        }

        if (currentPlace != std::clamp(currentPlace, 0, (int)m_vMonitors.size() - 1)) {
            Debug::log(WARN, "Error in getMonitorFromString: Vaxry's code sucks.");
            currentPlace = std::clamp(currentPlace, 0, (int)m_vMonitors.size() - 1);
        }

        return m_vMonitors[currentPlace].get();
    } else if (isNumber(name)) {
        // change by ID
        int monID = -1;
        try {
            monID = std::stoi(name);
        } catch (std::exception& e) {
            // shouldn't happen but jic
            Debug::log(ERR, "Error in getMonitorFromString: invalid num");
            return nullptr;
        }

        if (monID > -1 && monID < (int)m_vMonitors.size()) {
            return getMonitorFromID(monID);
        } else {
            Debug::log(ERR, "Error in getMonitorFromString: invalid arg 1");
            return nullptr;
        }
    } else if (name.starts_with("desc:")) {
        const auto DESCRIPTION = name.substr(5);

        for (auto& m : m_vMonitors) {
            if (!m->output)
                continue;

            if (m->szDescription.starts_with(DESCRIPTION)) {
                return m.get();
            }
        }

        return nullptr;
    } else {
        if (name == "current")
            return g_pCompositor->m_pLastMonitor;

        if (isDirection(name)) {
            const auto PMONITOR = getMonitorInDirection(name[0]);
            return PMONITOR;
        } else {
            for (auto& m : m_vMonitors) {
                if (m->szName == name) {
                    return m.get();
                }
            }
        }
    }

    return nullptr;
}

void CCompositor::moveWorkspaceToMonitor(CWorkspace* pWorkspace, CMonitor* pMonitor, bool noWarpCursor) {

    // We trust the workspace and monitor to be correct.

    if (pWorkspace->m_iMonitorID == pMonitor->ID)
        return;

    Debug::log(LOG, "moveWorkspaceToMonitor: Moving {} to monitor {}", pWorkspace->m_iID, pMonitor->ID);

    const auto POLDMON = getMonitorFromID(pWorkspace->m_iMonitorID);

    const bool SWITCHINGISACTIVE = POLDMON ? POLDMON->activeWorkspace == pWorkspace->m_iID : false;

    // fix old mon
    int nextWorkspaceOnMonitorID = -1;
    if (!SWITCHINGISACTIVE)
        nextWorkspaceOnMonitorID = pWorkspace->m_iID;
    else {
        for (auto& w : m_vWorkspaces) {
            if (w->m_iMonitorID == POLDMON->ID && w->m_iID != pWorkspace->m_iID && !w->m_bIsSpecialWorkspace) {
                nextWorkspaceOnMonitorID = w->m_iID;
                break;
            }
        }

        if (nextWorkspaceOnMonitorID == -1) {
            nextWorkspaceOnMonitorID = 1;

            while (getWorkspaceByID(nextWorkspaceOnMonitorID) || [&]() -> bool {
                const auto B = g_pConfigManager->getBoundMonitorForWS(std::to_string(nextWorkspaceOnMonitorID));
                return B && B != POLDMON;
            }())
                nextWorkspaceOnMonitorID++;

            Debug::log(LOG, "moveWorkspaceToMonitor: Plugging gap with new {}", nextWorkspaceOnMonitorID);

            g_pCompositor->createNewWorkspace(nextWorkspaceOnMonitorID, POLDMON->ID);
        }

        Debug::log(LOG, "moveWorkspaceToMonitor: Plugging gap with existing {}", nextWorkspaceOnMonitorID);
        POLDMON->changeWorkspace(nextWorkspaceOnMonitorID);
    }

    // move the workspace
    pWorkspace->m_iMonitorID = pMonitor->ID;
    pWorkspace->moveToMonitor(pMonitor->ID);

    for (auto& w : m_vWindows) {
        if (w->m_iWorkspaceID == pWorkspace->m_iID) {
            if (w->m_bPinned) {
                w->m_iWorkspaceID = nextWorkspaceOnMonitorID;
                continue;
            }

            w->m_iMonitorID = pMonitor->ID;

            // additionally, move floating and fs windows manually
            if (w->m_bIsMapped && !w->isHidden()) {
                if (POLDMON) {
                    if (w->m_bIsFloating)
                        w->m_vRealPosition = w->m_vRealPosition.vec() - POLDMON->vecPosition + pMonitor->vecPosition;

                    if (w->m_bIsFullscreen) {
                        w->m_vRealPosition = pMonitor->vecPosition;
                        w->m_vRealSize     = pMonitor->vecSize;
                    }
                } else {
                    w->m_vRealPosition = Vector2D{(int)w->m_vRealPosition.goalv().x % (int)pMonitor->vecSize.x, (int)w->m_vRealPosition.goalv().y % (int)pMonitor->vecSize.y};
                }
            }

            w->updateToplevel();
        }
    }

    if (SWITCHINGISACTIVE && POLDMON == g_pCompositor->m_pLastMonitor) { // if it was active, preserve its' status. If it wasn't, don't.
        Debug::log(LOG, "moveWorkspaceToMonitor: SWITCHINGISACTIVE, active {} -> {}", pMonitor->activeWorkspace, pWorkspace->m_iID);

        if (const auto PWORKSPACE = getWorkspaceByID(pMonitor->activeWorkspace); PWORKSPACE)
            getWorkspaceByID(pMonitor->activeWorkspace)->startAnim(false, false);

        pMonitor->activeWorkspace = pWorkspace->m_iID;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(pMonitor->ID);

        pWorkspace->startAnim(true, true, true);

        if (!noWarpCursor)
            wlr_cursor_warp(m_sWLRCursor, nullptr, pMonitor->vecPosition.x + pMonitor->vecTransformedSize.x / 2, pMonitor->vecPosition.y + pMonitor->vecTransformedSize.y / 2);

        g_pInputManager->sendMotionEventsToFocused();
    }

    // finalize
    if (POLDMON) {
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(POLDMON->ID);
        updateFullscreenFadeOnWorkspace(getWorkspaceByID(POLDMON->activeWorkspace));
    }

    updateFullscreenFadeOnWorkspace(pWorkspace);

    // event
    g_pEventManager->postEvent(SHyprIPCEvent{"moveworkspace", pWorkspace->m_szName + "," + pMonitor->szName});
    EMIT_HOOK_EVENT("moveWorkspace", (std::vector<void*>{pWorkspace, pMonitor}));
}

bool CCompositor::workspaceIDOutOfBounds(const int64_t& id) {
    int64_t lowestID  = INT64_MAX;
    int64_t highestID = INT64_MIN;

    for (auto& w : m_vWorkspaces) {
        if (w->m_bIsSpecialWorkspace)
            continue;

        if (w->m_iID < lowestID)
            lowestID = w->m_iID;

        if (w->m_iID > highestID)
            highestID = w->m_iID;
    }

    return std::clamp(id, lowestID, highestID) != id;
}

void CCompositor::updateFullscreenFadeOnWorkspace(CWorkspace* pWorkspace) {

    const auto FULLSCREEN = pWorkspace->m_bHasFullscreenWindow;

    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_iWorkspaceID == pWorkspace->m_iID) {

            if (w->m_bFadingOut || w->m_bPinned || w->m_bIsFullscreen)
                continue;

            if (!FULLSCREEN)
                w->m_fAlpha = 1.f;
            else if (!w->m_bIsFullscreen)
                w->m_fAlpha = !w->m_bCreatedOverFullscreen ? 0.f : 1.f;
        }
    }

    const auto PMONITOR = getMonitorFromID(pWorkspace->m_iMonitorID);

    if (pWorkspace->m_iID == PMONITOR->activeWorkspace) {
        for (auto& ls : PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
            if (!ls->fadingOut)
                ls->alpha = FULLSCREEN && pWorkspace->m_efFullscreenMode == FULLSCREEN_FULL ? 0.f : 1.f;
        }
    }
}

void CCompositor::setWindowFullscreen(CWindow* pWindow, bool on, eFullscreenMode mode) {
    if (!windowValidMapped(pWindow) || g_pCompositor->m_bUnsafeState)
        return;

    if (pWindow->m_bPinned) {
        Debug::log(LOG, "Pinned windows cannot be fullscreen'd");
        return;
    }

    const auto PMONITOR = getMonitorFromID(pWindow->m_iMonitorID);

    const auto PWORKSPACE = getWorkspaceByID(pWindow->m_iWorkspaceID);

    const auto MODE = mode == FULLSCREEN_INVALID ? PWORKSPACE->m_efFullscreenMode : mode;

    if (PWORKSPACE->m_bHasFullscreenWindow && on) {
        Debug::log(LOG, "Rejecting fullscreen ON on a fullscreen workspace");
        return;
    }

    g_pLayoutManager->getCurrentLayout()->fullscreenRequestForWindow(pWindow, MODE, on);

    g_pXWaylandManager->setWindowFullscreen(pWindow, pWindow->shouldSendFullscreenState());

    updateWindowAnimatedDecorationValues(pWindow);

    // make all windows on the same workspace under the fullscreen window
    for (auto& w : m_vWindows) {
        if (w->m_iWorkspaceID == PWORKSPACE->m_iID && !w->m_bIsFullscreen && !w->m_bFadingOut && !w->m_bPinned)
            w->m_bCreatedOverFullscreen = false;
    }
    updateFullscreenFadeOnWorkspace(PWORKSPACE);

    g_pXWaylandManager->setWindowSize(pWindow, pWindow->m_vRealSize.goalv(), true);

    forceReportSizesToWindowsOnWorkspace(pWindow->m_iWorkspaceID);

    g_pInputManager->recheckIdleInhibitorStatus();

    // DMAbuf stuff for direct scanout
    g_pHyprRenderer->setWindowScanoutMode(pWindow);

    g_pConfigManager->ensureVRR(PMONITOR);
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

    if (!pMonitor->m_bEnabled)
        return;

    if (pMonitor->renderingActive)
        pMonitor->pendingFrame = true;

    wlr_output_schedule_frame(pMonitor->output);
}

CWindow* CCompositor::getWindowByRegex(const std::string& regexp) {
    eFocusWindowMode mode = MODE_CLASS_REGEX;

    std::regex       regexCheck(regexp);
    std::string      matchCheck;
    if (regexp.starts_with("title:")) {
        mode       = MODE_TITLE_REGEX;
        regexCheck = std::regex(regexp.substr(6));
    } else if (regexp.starts_with("address:")) {
        mode       = MODE_ADDRESS;
        matchCheck = regexp.substr(8);
    } else if (regexp.starts_with("pid:")) {
        mode       = MODE_PID;
        matchCheck = regexp.substr(4);
    } else if (regexp.starts_with("floating") || regexp.starts_with("tiled")) {
        // first floating on the current ws
        if (!m_pLastWindow)
            return nullptr;

        const bool FLOAT = regexp.starts_with("floating");

        for (auto& w : m_vWindows) {
            if (!w->m_bIsMapped || w->m_bIsFloating != FLOAT || w->m_iWorkspaceID != m_pLastWindow->m_iWorkspaceID || w->isHidden())
                continue;

            return w.get();
        }

        return nullptr;
    }

    for (auto& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped || (w->isHidden() && !g_pLayoutManager->getCurrentLayout()->isWindowReachable(w.get())))
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
                std::string addr = std::format("0x{:x}", (uintptr_t)w.get());
                if (matchCheck != addr)
                    continue;
                break;
            }
            case MODE_PID: {
                std::string pid = std::format("{}", w->getPID());
                if (matchCheck != pid)
                    continue;
                break;
            }
            default: break;
        }

        return w.get();
    }

    return nullptr;
}

void CCompositor::warpCursorTo(const Vector2D& pos, bool force) {

    // warpCursorTo should only be used for warps that
    // should be disabled with no_cursor_warps

    static auto* const PNOWARPS = &g_pConfigManager->getConfigValuePtr("general:no_cursor_warps")->intValue;

    if (*PNOWARPS && !force)
        return;

    if (!m_sSeat.mouse)
        return;

    wlr_cursor_warp(m_sWLRCursor, m_sSeat.mouse->mouse, pos.x, pos.y);

    const auto PMONITORNEW = getMonitorFromVector(pos);
    if (PMONITORNEW != m_pLastMonitor)
        setActiveMonitor(PMONITORNEW);
}

SLayerSurface* CCompositor::getLayerSurfaceFromWlr(wlr_layer_surface_v1* pLS) {
    for (auto& m : m_vMonitors) {
        for (auto& lsl : m->m_aLayerSurfaceLayers) {
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
    std::pair<wlr_surface*, bool> result = {pSurface, false};

    for (auto& m : m_vMonitors) {
        for (auto& lsl : m->m_aLayerSurfaceLayers) {
            for (auto& ls : lsl) {
                if (ls->layerSurface && ls->layerSurface->surface == pSurface)
                    return ls.get();

                static auto iter = [](wlr_surface* surf, int x, int y, void* data) -> void {
                    if (surf == ((std::pair<wlr_surface*, bool>*)data)->first) {
                        *(bool*)data = true;
                        return;
                    }
                };

                if (!ls->layerSurface || !ls->mapped)
                    continue;

                wlr_surface_for_each_surface(ls->layerSurface->surface, iter, &result);

                if (result.second)
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

    const auto  PMONITOR = m_pLastMonitor;

    bool        xIsPercent = false;
    bool        yIsPercent = false;
    bool        isExact    = false;

    std::string x = args.substr(0, args.find_first_of(' '));
    std::string y = args.substr(args.find_first_of(' ') + 1);

    if (x == "exact") {
        x       = y.substr(0, y.find_first_of(' '));
        y       = y.substr(y.find_first_of(' ') + 1);
        isExact = true;
    }

    if (x.contains('%')) {
        xIsPercent = true;
        x          = x.substr(0, x.length() - 1);
    }

    if (y.contains('%')) {
        yIsPercent = true;
        y          = y.substr(0, y.length() - 1);
    }

    if (!isNumber(x) || !isNumber(y)) {
        Debug::log(ERR, "parseWindowVectorArgsRelative: args not numbers");
        return relativeTo;
    }

    int X = 0;
    int Y = 0;

    if (isExact) {
        X = xIsPercent ? std::stof(x) * 0.01 * PMONITOR->vecSize.x : std::stoi(x);
        Y = yIsPercent ? std::stof(y) * 0.01 * PMONITOR->vecSize.y : std::stoi(y);
    } else {
        X = xIsPercent ? std::stof(x) * 0.01 * relativeTo.x + relativeTo.x : std::stoi(x) + relativeTo.x;
        Y = yIsPercent ? std::stof(y) * 0.01 * relativeTo.y + relativeTo.y : std::stoi(y) + relativeTo.y;
    }

    return Vector2D(X, Y);
}

void CCompositor::forceReportSizesToWindowsOnWorkspace(const int& wid) {
    for (auto& w : m_vWindows) {
        if (w->m_iWorkspaceID == wid && w->m_bIsMapped && !w->isHidden()) {
            g_pXWaylandManager->setWindowSize(w.get(), w->m_vRealSize.vec(), true);
        }
    }
}

CWorkspace* CCompositor::createNewWorkspace(const int& id, const int& monid, const std::string& name) {
    const auto NAME  = name == "" ? std::to_string(id) : name;
    auto       monID = monid;

    // check if bound
    if (const auto PMONITOR = g_pConfigManager->getBoundMonitorForWS(NAME); PMONITOR) {
        monID = PMONITOR->ID;
    }

    const bool SPECIAL = id >= SPECIAL_WORKSPACE_START && id <= -2;

    const auto PWORKSPACE = m_vWorkspaces.emplace_back(std::make_unique<CWorkspace>(monID, NAME, SPECIAL)).get();

    PWORKSPACE->m_iID        = id;
    PWORKSPACE->m_iMonitorID = monID;

    PWORKSPACE->m_fAlpha.setValueAndWarp(0);

    return PWORKSPACE;
}

void CCompositor::renameWorkspace(const int& id, const std::string& name) {
    const auto PWORKSPACE = getWorkspaceByID(id);

    if (!PWORKSPACE)
        return;

    if (isWorkspaceSpecial(id))
        return;

    Debug::log(LOG, "renameWorkspace: Renaming workspace {} to '{}'", id, name);
    PWORKSPACE->m_szName = name;

    g_pEventManager->postEvent({"renameworkspace", std::to_string(PWORKSPACE->m_iID) + "," + PWORKSPACE->m_szName});
}

void CCompositor::setActiveMonitor(CMonitor* pMonitor) {
    if (m_pLastMonitor == pMonitor)
        return;

    if (!pMonitor) {
        m_pLastMonitor = nullptr;
        return;
    }

    const auto PWORKSPACE = getWorkspaceByID(pMonitor->activeWorkspace);

    g_pEventManager->postEvent(SHyprIPCEvent{"focusedmon", pMonitor->szName + "," + PWORKSPACE->m_szName});
    EMIT_HOOK_EVENT("focusedMon", pMonitor);
    m_pLastMonitor = pMonitor;
}

bool CCompositor::isWorkspaceSpecial(const int& id) {
    return id >= SPECIAL_WORKSPACE_START && id <= -2;
}

int CCompositor::getNewSpecialID() {
    int highest = SPECIAL_WORKSPACE_START;
    for (auto& ws : m_vWorkspaces) {
        if (ws->m_bIsSpecialWorkspace && ws->m_iID > highest) {
            highest = ws->m_iID;
        }
    }

    return highest + 1;
}

void CCompositor::performUserChecks() {
    ; // intentional
}

void CCompositor::moveWindowToWorkspaceSafe(CWindow* pWindow, CWorkspace* pWorkspace) {
    if (!pWindow || !pWorkspace)
        return;

    if (pWindow->m_bPinned && pWorkspace->m_bIsSpecialWorkspace)
        return;

    const bool FULLSCREEN     = pWindow->m_bIsFullscreen;
    const auto FULLSCREENMODE = getWorkspaceByID(pWindow->m_iWorkspaceID)->m_efFullscreenMode;

    if (FULLSCREEN)
        setWindowFullscreen(pWindow, false, FULLSCREEN_FULL);

    pWindow->moveToWorkspace(pWorkspace->m_iID);
    pWindow->updateToplevel();
    pWindow->updateDynamicRules();
    pWindow->uncacheWindowDecos();

    if (!pWindow->m_bIsFloating) {
        g_pLayoutManager->getCurrentLayout()->onWindowRemovedTiling(pWindow);
        pWindow->m_iWorkspaceID = pWorkspace->m_iID;
        pWindow->m_iMonitorID   = pWorkspace->m_iMonitorID;
        g_pLayoutManager->getCurrentLayout()->onWindowCreatedTiling(pWindow);
    } else {
        const auto PWINDOWMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
        const auto POSTOMON       = pWindow->m_vRealPosition.goalv() - PWINDOWMONITOR->vecPosition;

        const auto PWORKSPACEMONITOR = g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID);

        pWindow->m_iWorkspaceID = pWorkspace->m_iID;
        pWindow->m_iMonitorID   = pWorkspace->m_iMonitorID;

        pWindow->m_vRealPosition = POSTOMON + PWORKSPACEMONITOR->vecPosition;
    }

    if (pWindow->m_sGroupData.pNextWindow) {
        CWindow* next = pWindow->m_sGroupData.pNextWindow;
        while (next != pWindow) {
            next->moveToWorkspace(pWorkspace->m_iID);
            next->updateToplevel();
            next = next->m_sGroupData.pNextWindow;
        }
    }

    if (FULLSCREEN)
        setWindowFullscreen(pWindow, true, FULLSCREENMODE);

    g_pCompositor->updateWorkspaceWindows(pWorkspace->m_iID);
    g_pCompositor->updateWorkspaceWindows(pWindow->m_iWorkspaceID);
}

CWindow* CCompositor::getForceFocus() {
    for (auto& w : m_vWindows) {
        if (!w->m_bIsMapped || w->isHidden() || !isWorkspaceVisible(w->m_iWorkspaceID))
            continue;

        if (!w->m_bStayFocused)
            continue;

        return w.get();
    }

    return nullptr;
}

void CCompositor::notifyIdleActivity() {
    wlr_idle_notifier_v1_notify_activity(g_pCompositor->m_sWLRIdleNotifier, g_pCompositor->m_sSeat.seat);
}

void CCompositor::setIdleActivityInhibit(bool enabled) {
    wlr_idle_notifier_v1_set_inhibited(g_pCompositor->m_sWLRIdleNotifier, !enabled);
}
void CCompositor::arrangeMonitors() {
    static auto* const     PXWLFORCESCALEZERO = &g_pConfigManager->getConfigValuePtr("xwayland:force_zero_scaling")->intValue;

    std::vector<CMonitor*> toArrange;
    std::vector<CMonitor*> arranged;

    for (auto& m : m_vMonitors)
        toArrange.push_back(m.get());

    Debug::log(LOG, "arrangeMonitors: {} to arrange", toArrange.size());

    for (auto it = toArrange.begin(); it != toArrange.end();) {
        auto m = *it;

        if (m->activeMonitorRule.offset != Vector2D{-INT32_MAX, -INT32_MAX}) {
            // explicit.
            Debug::log(LOG, "arrangeMonitors: {} explicit {:j2}", m->szName, m->activeMonitorRule.offset);

            m->moveTo(m->activeMonitorRule.offset);
            arranged.push_back(m);
            it = toArrange.erase(it);

            if (it == toArrange.end())
                break;

            continue;
        }

        ++it;
    }

    // auto left
    int maxOffset = 0;
    for (auto& m : arranged) {
        if (m->vecPosition.x + m->vecSize.x > maxOffset)
            maxOffset = m->vecPosition.x + m->vecSize.x;
    }

    for (auto& m : toArrange) {
        Debug::log(LOG, "arrangeMonitors: {} auto [{}, {:.2f}]", m->szName, maxOffset, 0.f);
        m->moveTo({maxOffset, 0});
        maxOffset += m->vecSize.x;
    }

    // reset maxOffset (reuse)
    // and set xwayland positions aka auto for all
    maxOffset = 0;
    for (auto& m : m_vMonitors) {
        Debug::log(LOG, "arrangeMonitors: {} xwayland [{}, {:.2f}]", m->szName, maxOffset, 0.f);
        m->vecXWaylandPosition = {maxOffset, 0};
        maxOffset += (*PXWLFORCESCALEZERO ? m->vecTransformedSize.x : m->vecSize.x);

        if (*PXWLFORCESCALEZERO)
            m->xwaylandScale = m->scale;
        else
            m->xwaylandScale = 1.f;
    }
}

void CCompositor::enterUnsafeState() {
    if (m_bUnsafeState)
        return;

    Debug::log(LOG, "Entering unsafe state");

    if (!m_pUnsafeOutput->m_bEnabled)
        m_pUnsafeOutput->onConnect(false);

    m_bUnsafeState = true;
}

void CCompositor::leaveUnsafeState() {
    if (!m_bUnsafeState)
        return;

    Debug::log(LOG, "Leaving unsafe state");

    m_bUnsafeState = false;

    CMonitor* pNewMonitor = nullptr;
    for (auto& pMonitor : m_vMonitors) {
        if (pMonitor->output != m_pUnsafeOutput->output) {
            pNewMonitor = pMonitor.get();
            break;
        }
    }

    RASSERT(pNewMonitor, "Tried to leave unsafe without a monitor");

    if (m_pUnsafeOutput->m_bEnabled)
        m_pUnsafeOutput->onDisconnect();

    for (auto& m : m_vMonitors) {
        scheduleFrameForMonitor(m.get());
    }
}

void CCompositor::setPreferredScaleForSurface(wlr_surface* pSurface, double scale) {
    g_pProtocolManager->m_pFractionalScaleProtocolManager->setPreferredScaleForSurface(pSurface, scale);
    wlr_surface_set_preferred_buffer_scale(pSurface, static_cast<int32_t>(std::ceil(scale)));

    const auto PSURFACE = CWLSurface::surfaceFromWlr(pSurface);
    if (!PSURFACE) {
        Debug::log(WARN, "Orphaned wlr_surface {:x} in setPreferredScaleForSurface", (uintptr_t)pSurface);
        return;
    }

    PSURFACE->m_fLastScale = scale;
    PSURFACE->m_iLastScale = static_cast<int32_t>(std::ceil(scale));
}

void CCompositor::setPreferredTransformForSurface(wlr_surface* pSurface, wl_output_transform transform) {
    wlr_surface_set_preferred_buffer_transform(pSurface, transform);

    const auto PSURFACE = CWLSurface::surfaceFromWlr(pSurface);
    if (!PSURFACE) {
        Debug::log(WARN, "Orphaned wlr_surface {:x} in setPreferredTransformForSurface", (uintptr_t)pSurface);
        return;
    }

    PSURFACE->m_eLastTransform = transform;
}

void CCompositor::updateSuspendedStates() {
    for (auto& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped)
            continue;

        w->setSuspended(w->isHidden() || !isWorkspaceVisible(w->m_iWorkspaceID));
    }
}
