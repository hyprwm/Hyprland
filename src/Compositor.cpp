#include "Compositor.hpp"
#include "helpers/Splashes.hpp"
#include "config/ConfigValue.hpp"
#include "managers/CursorManager.hpp"
#include "managers/TokenManager.hpp"
#include "managers/PointerManager.hpp"
#include "managers/SeatManager.hpp"
#include "managers/eventLoop/EventLoopManager.hpp"
#include <random>
#include <unordered_set>
#include "debug/HyprCtl.hpp"
#include "debug/CrashReporter.hpp"
#ifdef USES_SYSTEMD
#include <helpers/SdDaemon.hpp> // for SdNotify
#endif
#include <ranges>
#include "helpers/VarList.hpp"
#include "protocols/FractionalScale.hpp"
#include "protocols/PointerConstraints.hpp"
#include "protocols/LayerShell.hpp"
#include "protocols/XDGShell.hpp"
#include "protocols/core/Compositor.hpp"
#include "protocols/core/Subcompositor.hpp"
#include "desktop/LayerSurface.hpp"
#include "xwayland/XWayland.hpp"

#include <sys/types.h>
#include <sys/stat.h>

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

    // Kill the program if the crash-reporter is caught in a deadlock.
    signal(SIGALRM, [](int _) {
        char const* msg = "\nCrashReporter exceeded timeout, forcefully exiting\n";
        write(2, msg, strlen(msg));
        abort();
    });
    alarm(15);

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

    m_szHyprTempDataRoot = std::string{getenv("XDG_RUNTIME_DIR")} + "/hypr";

    if (m_szHyprTempDataRoot.starts_with("/hypr")) {
        std::cout << "Bailing out, XDG_RUNTIME_DIR is invalid\n";
        throw std::runtime_error("CCompositor() failed");
    }

    if (!m_szHyprTempDataRoot.starts_with("/run/user"))
        std::cout << "[!!WARNING!!] XDG_RUNTIME_DIR looks non-standard. Proceeding anyways...\n";

    std::random_device              dev;
    std::mt19937                    engine(dev());
    std::uniform_int_distribution<> distribution(0, INT32_MAX);

    m_szInstanceSignature = GIT_COMMIT_HASH + std::string("_") + std::to_string(time(NULL)) + "_" + std::to_string(distribution(engine));

    setenv("HYPRLAND_INSTANCE_SIGNATURE", m_szInstanceSignature.c_str(), true);

    if (!std::filesystem::exists(m_szHyprTempDataRoot))
        mkdir(m_szHyprTempDataRoot.c_str(), S_IRWXU);
    else if (!std::filesystem::is_directory(m_szHyprTempDataRoot)) {
        std::cout << "Bailing out, " << m_szHyprTempDataRoot << " is not a directory\n";
        throw std::runtime_error("CCompositor() failed");
    }

    m_szInstancePath = m_szHyprTempDataRoot + "/" + m_szInstanceSignature;

    if (std::filesystem::exists(m_szInstancePath)) {
        std::cout << "Bailing out, " << m_szInstancePath << " exists??\n";
        throw std::runtime_error("CCompositor() failed");
    }

    if (mkdir(m_szInstancePath.c_str(), S_IRWXU) < 0) {
        std::cout << "Bailing out, couldn't create " << m_szInstancePath << "\n";
        throw std::runtime_error("CCompositor() failed");
    }

    Debug::init(m_szInstancePath);

    Debug::log(LOG, "Instance Signature: {}", m_szInstanceSignature);

    Debug::log(LOG, "Runtime directory: {}", m_szInstancePath);

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
    m_critSigSource = wl_event_loop_add_signal(m_sWLEventLoop, SIGTERM, handleCritSignal, nullptr);

    if (!envEnabled("HYPRLAND_NO_CRASHREPORTER")) {
        signal(SIGSEGV, handleUnrecoverableSignal);
        signal(SIGABRT, handleUnrecoverableSignal);
    }
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

    bool isHeadlessOnly = true;
    wlr_multi_for_each_backend(
        m_sWLRBackend,
        [](wlr_backend* backend, void* isHeadlessOnly) {
            if (!wlr_backend_is_headless(backend) && !wlr_backend_is_libinput(backend))
                *(bool*)isHeadlessOnly = false;
        },
        &isHeadlessOnly);

    if (isHeadlessOnly) {
        m_sWLRRenderer = wlr_renderer_autocreate(m_sWLRBackend); // TODO: remove this, it's barely needed now.
    } else {
        m_iDRMFD = wlr_backend_get_drm_fd(m_sWLRBackend);
        if (m_iDRMFD < 0) {
            Debug::log(CRIT, "Couldn't query the DRM FD!");
            throwError("wlr_backend_get_drm_fd() failed!");
        }

        m_sWLRRenderer = wlr_gles2_renderer_create_with_drm_fd(m_iDRMFD);
    }

    if (!m_sWLRRenderer) {
        Debug::log(CRIT, "m_sWLRRenderer was NULL! This usually means wlroots could not find a GPU or enountered some issues.");
        throwError("wlr_gles2_renderer_create_with_drm_fd() failed!");
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

    initManagers(STAGE_BASICINIT);

    m_sWRLDRMLeaseMgr = wlr_drm_lease_v1_manager_create(m_sWLDisplay, m_sWLRBackend);
    if (!m_sWRLDRMLeaseMgr) {
        Debug::log(INFO, "Failed to create wlr_drm_lease_v1_manager");
        Debug::log(INFO, "VR will not be available");
    }

    m_sWLRHeadlessBackend = wlr_headless_backend_create(m_sWLEventLoop);

    if (!m_sWLRHeadlessBackend) {
        Debug::log(CRIT, "Couldn't create the headless backend");
        throwError("wlr_headless_backend_create() failed!");
    }

    wlr_multi_backend_add(m_sWLRBackend, m_sWLRHeadlessBackend);

    initManagers(STAGE_LATE);
}

void CCompositor::initAllSignals() {
    addWLSignal(&m_sWLRBackend->events.new_output, &Events::listen_newOutput, m_sWLRBackend, "Backend");
    addWLSignal(&m_sWLRBackend->events.new_input, &Events::listen_newInput, m_sWLRBackend, "Backend");
    addWLSignal(&m_sWLRRenderer->events.destroy, &Events::listen_RendererDestroy, m_sWLRRenderer, "WLRRenderer");

    if (m_sWRLDRMLeaseMgr)
        addWLSignal(&m_sWRLDRMLeaseMgr->events.request, &Events::listen_leaseRequest, &m_sWRLDRMLeaseMgr, "DRM");

    if (m_sWLRSession)
        addWLSignal(&m_sWLRSession->events.active, &Events::listen_sessionActive, m_sWLRSession, "Session");
}

void CCompositor::removeAllSignals() {
    removeWLSignal(&Events::listen_newOutput);
    removeWLSignal(&Events::listen_newInput);
    removeWLSignal(&Events::listen_RendererDestroy);

    if (m_sWRLDRMLeaseMgr)
        removeWLSignal(&Events::listen_leaseRequest);

    if (m_sWLRSession)
        removeWLSignal(&Events::listen_sessionActive);
}

void CCompositor::cleanEnvironment() {
    // in compositor constructor
    unsetenv("WAYLAND_DISPLAY");
    // in startCompositor
    unsetenv("HYPRLAND_INSTANCE_SIGNATURE");

    // in main
    unsetenv("HYPRLAND_CMD");
    unsetenv("XDG_BACKEND");
    unsetenv("XDG_CURRENT_DESKTOP");

    if (m_sWLRSession) {
        const auto CMD =
#ifdef USES_SYSTEMD
            "systemctl --user unset-environment DISPLAY WAYLAND_DISPLAY HYPRLAND_INSTANCE_SIGNATURE XDG_CURRENT_DESKTOP QT_QPA_PLATFORMTHEME && hash "
            "dbus-update-activation-environment 2>/dev/null && "
#endif
            "dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP HYPRLAND_INSTANCE_SIGNATURE QT_QPA_PLATFORMTHEME";
        g_pKeybindManager->spawn(CMD);
    }
}

void CCompositor::cleanup() {
    if (!m_sWLDisplay || m_bIsShuttingDown)
        return;

    signal(SIGABRT, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);

    removeLockFile();

    m_bIsShuttingDown   = true;
    Debug::shuttingDown = true;

#ifdef USES_SYSTEMD
    if (Systemd::SdBooted() > 0 && !envEnabled("HYPRLAND_NO_SD_NOTIFY"))
        Systemd::SdNotify(0, "STOPPING=1");
#endif

    cleanEnvironment();

    // unload all remaining plugins while the compositor is
    // still in a normal working state.
    g_pPluginSystem->unloadAllPlugins();

    m_pLastFocus.reset();
    m_pLastWindow.reset();

    m_vWorkspaces.clear();
    m_vWindows.clear();

    for (auto& m : m_vMonitors) {
        g_pHyprOpenGL->destroyMonitorResources(m.get());

        wlr_output_state_set_enabled(m->state.wlr(), false);
        m->state.commit();
    }

    g_pXWayland.reset();

    m_vMonitors.clear();

    wl_display_destroy_clients(g_pCompositor->m_sWLDisplay);
    removeAllSignals();

    g_pInputManager.reset();
    g_pDecorationPositioner.reset();
    g_pCursorManager.reset();
    g_pPluginSystem.reset();
    g_pHyprNotificationOverlay.reset();
    g_pDebugOverlay.reset();
    g_pEventManager.reset();
    g_pSessionLockManager.reset();
    g_pProtocolManager.reset();
    g_pHyprRenderer.reset();
    g_pHyprOpenGL.reset();
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
    g_pPointerManager.reset();
    g_pSeatManager.reset();

    if (m_sWLRRenderer)
        wlr_renderer_destroy(m_sWLRRenderer);

    if (m_sWLRAllocator)
        wlr_allocator_destroy(m_sWLRAllocator);

    if (m_sWLRBackend)
        wlr_backend_destroy(m_sWLRBackend);

    if (m_critSigSource)
        wl_event_source_remove(m_critSigSource);

    wl_display_terminate(m_sWLDisplay);
    m_sWLDisplay = nullptr;

    std::string waylandSocket = std::string{getenv("XDG_RUNTIME_DIR")} + "/" + m_szWLDisplaySocket;
    std::filesystem::remove(waylandSocket);
    std::filesystem::remove(waylandSocket + ".lock");
}

void CCompositor::initManagers(eManagersInitStage stage) {
    switch (stage) {
        case STAGE_PRIORITY: {
            Debug::log(LOG, "Creating the EventLoopManager!");
            g_pEventLoopManager = std::make_unique<CEventLoopManager>();

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

            Debug::log(LOG, "Creating the TokenManager!");
            g_pTokenManager = std::make_unique<CTokenManager>();

            g_pConfigManager->init();
            g_pWatchdog = std::make_unique<CWatchdog>(); // requires config

            Debug::log(LOG, "Creating the PointerManager!");
            g_pPointerManager = std::make_unique<CPointerManager>();
        } break;
        case STAGE_BASICINIT: {
            Debug::log(LOG, "Creating the CHyprOpenGLImpl!");
            g_pHyprOpenGL = std::make_unique<CHyprOpenGLImpl>();

            Debug::log(LOG, "Creating the ProtocolManager!");
            g_pProtocolManager = std::make_unique<CProtocolManager>();

            Debug::log(LOG, "Creating the SeatManager!");
            g_pSeatManager = std::make_unique<CSeatManager>();
        } break;
        case STAGE_LATE: {
            Debug::log(LOG, "Creating the ThreadManager!");
            g_pThreadManager = std::make_unique<CThreadManager>();

            Debug::log(LOG, "Creating CHyprCtl");
            g_pHyprCtl = std::make_unique<CHyprCtl>();

            Debug::log(LOG, "Creating the InputManager!");
            g_pInputManager = std::make_unique<CInputManager>();

            Debug::log(LOG, "Creating the HyprRenderer!");
            g_pHyprRenderer = std::make_unique<CHyprRenderer>();

            Debug::log(LOG, "Creating the XWaylandManager!");
            g_pXWaylandManager = std::make_unique<CHyprXWaylandManager>();

            Debug::log(LOG, "Creating the SessionLockManager!");
            g_pSessionLockManager = std::make_unique<CSessionLockManager>();

            Debug::log(LOG, "Creating the EventManager!");
            g_pEventManager = std::make_unique<CEventManager>();

            Debug::log(LOG, "Creating the HyprDebugOverlay!");
            g_pDebugOverlay = std::make_unique<CHyprDebugOverlay>();

            Debug::log(LOG, "Creating the HyprNotificationOverlay!");
            g_pHyprNotificationOverlay = std::make_unique<CHyprNotificationOverlay>();

            Debug::log(LOG, "Creating the PluginSystem!");
            g_pPluginSystem = std::make_unique<CPluginSystem>();
            g_pConfigManager->handlePluginLoads();

            Debug::log(LOG, "Creating the DecorationPositioner!");
            g_pDecorationPositioner = std::make_unique<CDecorationPositioner>();

            Debug::log(LOG, "Creating the CursorManager!");
            g_pCursorManager = std::make_unique<CCursorManager>();

            Debug::log(LOG, "Starting XWayland");
            g_pXWayland = std::make_unique<CXWayland>();
        } break;
        default: UNREACHABLE();
    }
}

void CCompositor::createLockFile() {
    const auto    PATH = m_szInstancePath + "/hyprland.lock";

    std::ofstream ofs(PATH, std::ios::trunc);

    ofs << m_iHyprlandPID << "\n" << m_szWLDisplaySocket << "\n";

    ofs.close();
}

void CCompositor::removeLockFile() {
    const auto PATH = m_szInstancePath + "/hyprland.lock";

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
    if (Systemd::SdBooted() > 0) {
        // tell systemd that we are ready so it can start other bond, following, related units
        if (!envEnabled("HYPRLAND_NO_SD_NOTIFY"))
            Systemd::SdNotify(0, "READY=1");
    } else
        Debug::log(LOG, "systemd integration is baked in but system itself is not booted à la systemd!");
#endif

    createLockFile();

    EMIT_HOOK_EVENT("ready", nullptr);

    // This blocks until we are done.
    Debug::log(LOG, "Hyprland is ready, running the event loop!");
    g_pEventLoopManager->enterLoop(m_sWLDisplay, m_sWLEventLoop);
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
    return getMonitorFromVector(g_pPointerManager->position());
}

CMonitor* CCompositor::getMonitorFromVector(const Vector2D& point) {
    SP<CMonitor> mon;
    for (auto& m : m_vMonitors) {
        if (CBox{m->vecPosition, m->vecSize}.containsPoint(point)) {
            mon = m;
            break;
        }
    }

    if (!mon) {
        float        bestDistance = 0.f;
        SP<CMonitor> pBestMon;

        for (auto& m : m_vMonitors) {
            float dist = vecToRectDistanceSquared(point, m->vecPosition, m->vecPosition + m->vecSize);

            if (dist < bestDistance || !pBestMon) {
                bestDistance = dist;
                pBestMon     = m;
            }
        }

        if (!pBestMon) { // ?????
            Debug::log(WARN, "getMonitorFromVector no close mon???");
            return m_vMonitors.front().get();
        }

        return pBestMon.get();
    }

    return mon.get();
}

void CCompositor::removeWindowFromVectorSafe(PHLWINDOW pWindow) {
    if (!pWindow->m_bFadingOut) {
        EMIT_HOOK_EVENT("destroyWindow", pWindow);

        std::erase_if(m_vWindows, [&](SP<CWindow>& el) { return el == pWindow; });
        std::erase_if(m_vWindowsFadingOut, [&](PHLWINDOWREF el) { return el.lock() == pWindow; });
    }
}

bool CCompositor::monitorExists(CMonitor* pMonitor) {
    for (auto& m : m_vRealMonitors) {
        if (m.get() == pMonitor)
            return true;
    }

    return false;
}

PHLWINDOW CCompositor::vectorToWindowUnified(const Vector2D& pos, uint8_t properties, PHLWINDOW pIgnoreWindow) {
    const auto  PMONITOR          = getMonitorFromVector(pos);
    static auto PRESIZEONBORDER   = CConfigValue<Hyprlang::INT>("general:resize_on_border");
    static auto PBORDERSIZE       = CConfigValue<Hyprlang::INT>("general:border_size");
    static auto PBORDERGRABEXTEND = CConfigValue<Hyprlang::INT>("general:extend_border_grab_area");
    static auto PSPECIALFALLTHRU  = CConfigValue<Hyprlang::INT>("input:special_fallthrough");
    const auto  BORDER_GRAB_AREA  = *PRESIZEONBORDER ? *PBORDERSIZE + *PBORDERGRABEXTEND : 0;

    // pinned windows on top of floating regardless
    if (properties & ALLOW_FLOATING) {
        for (auto& w : m_vWindows | std::views::reverse) {
            const auto BB  = w->getWindowBoxUnified(properties);
            CBox       box = BB.copy().expand(w->m_iX11Type == 2 ? BORDER_GRAB_AREA : 0);
            if (w->m_bIsFloating && w->m_bIsMapped && !w->isHidden() && !w->m_bX11ShouldntFocus && w->m_bPinned && !w->m_sAdditionalConfigData.noFocus && w != pIgnoreWindow) {
                if (box.containsPoint(g_pPointerManager->position()))
                    return w;

                if (!w->m_bIsX11) {
                    if (w->hasPopupAt(pos))
                        return w;
                }
            }
        }
    }

    auto windowForWorkspace = [&](bool special) -> PHLWINDOW {
        auto floating = [&](bool aboveFullscreen) -> PHLWINDOW {
            for (auto& w : m_vWindows | std::views::reverse) {

                if (special && !w->onSpecialWorkspace()) // because special floating may creep up into regular
                    continue;

                const auto BB             = w->getWindowBoxUnified(properties);
                const auto PWINDOWMONITOR = getMonitorFromID(w->m_iMonitorID);

                // to avoid focusing windows behind special workspaces from other monitors
                if (!*PSPECIALFALLTHRU && PWINDOWMONITOR && PWINDOWMONITOR->activeSpecialWorkspace && w->m_pWorkspace != PWINDOWMONITOR->activeSpecialWorkspace &&
                    BB.x >= PWINDOWMONITOR->vecPosition.x && BB.y >= PWINDOWMONITOR->vecPosition.y &&
                    BB.x + BB.width <= PWINDOWMONITOR->vecPosition.x + PWINDOWMONITOR->vecSize.x && BB.y + BB.height <= PWINDOWMONITOR->vecPosition.y + PWINDOWMONITOR->vecSize.y)
                    continue;

                CBox box = BB.copy().expand(w->m_iX11Type == 2 ? BORDER_GRAB_AREA : 0);
                if (w->m_bIsFloating && w->m_bIsMapped && isWorkspaceVisible(w->m_pWorkspace) && !w->isHidden() && !w->m_bPinned && !w->m_sAdditionalConfigData.noFocus &&
                    w != pIgnoreWindow && (!aboveFullscreen || w->m_bCreatedOverFullscreen)) {
                    // OR windows should add focus to parent
                    if (w->m_bX11ShouldntFocus && w->m_iX11Type != 2)
                        continue;

                    if (box.containsPoint(g_pPointerManager->position())) {

                        if (w->m_bIsX11 && w->m_iX11Type == 2 && !w->m_pXWaylandSurface->wantsFocus()) {
                            // Override Redirect
                            return g_pCompositor->m_pLastWindow.lock(); // we kinda trick everything here.
                                // TODO: this is wrong, we should focus the parent, but idk how to get it considering it's nullptr in most cases.
                        }

                        return w;
                    }

                    if (!w->m_bIsX11) {
                        if (w->hasPopupAt(pos))
                            return w;
                    }
                }
            }

            return nullptr;
        };

        if (properties & ALLOW_FLOATING) {
            // first loop over floating cuz they're above, m_lWindows should be sorted bottom->top, for tiled it doesn't matter.
            auto found = floating(true);
            if (found)
                return found;
        }

        if (properties & FLOATING_ONLY)
            return floating(false);

        const int64_t WORKSPACEID = special ? PMONITOR->activeSpecialWorkspaceID() : PMONITOR->activeWorkspaceID();
        const auto    PWORKSPACE  = getWorkspaceByID(WORKSPACEID);

        if (PWORKSPACE->m_bHasFullscreenWindow)
            return getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);

        auto found = floating(false);
        if (found)
            return found;

        // for windows, we need to check their extensions too, first.
        for (auto& w : m_vWindows) {
            if (special != w->onSpecialWorkspace())
                continue;

            if (!w->m_bIsX11 && !w->m_bIsFloating && w->m_bIsMapped && w->workspaceID() == WORKSPACEID && !w->isHidden() && !w->m_bX11ShouldntFocus &&
                !w->m_sAdditionalConfigData.noFocus && w != pIgnoreWindow) {
                if (w->hasPopupAt(pos))
                    return w;
            }
        }

        for (auto& w : m_vWindows) {
            if (special != w->onSpecialWorkspace())
                continue;

            CBox box = (properties & USE_PROP_TILED) ? w->getWindowBoxUnified(properties) : CBox{w->m_vPosition, w->m_vSize};
            if (!w->m_bIsFloating && w->m_bIsMapped && box.containsPoint(pos) && w->workspaceID() == WORKSPACEID && !w->isHidden() && !w->m_bX11ShouldntFocus &&
                !w->m_sAdditionalConfigData.noFocus && w != pIgnoreWindow)
                return w;
        }

        return nullptr;
    };

    // special workspace
    if (PMONITOR->activeSpecialWorkspace && !*PSPECIALFALLTHRU)
        return windowForWorkspace(true);

    if (PMONITOR->activeSpecialWorkspace) {
        const auto PWINDOW = windowForWorkspace(true);

        if (PWINDOW)
            return PWINDOW;
    }

    return windowForWorkspace(false);
}

SP<CWLSurfaceResource> CCompositor::vectorWindowToSurface(const Vector2D& pos, PHLWINDOW pWindow, Vector2D& sl) {

    if (!validMapped(pWindow))
        return nullptr;

    RASSERT(!pWindow->m_bIsX11, "Cannot call vectorWindowToSurface on an X11 window!");

    // try popups first
    const auto PPOPUP = pWindow->m_pPopupHead->at(pos);

    if (PPOPUP) {
        const auto OFF = PPOPUP->coordsRelativeToParent();
        sl             = pos - pWindow->m_vRealPosition.goal() - OFF;
        return PPOPUP->m_pWLSurface->resource();
    }

    auto [surf, local] = pWindow->m_pWLSurface->resource()->at(pos - pWindow->m_vRealPosition.goal(), true);
    if (surf) {
        sl = local;
        return surf;
    }

    return nullptr;
}

Vector2D CCompositor::vectorToSurfaceLocal(const Vector2D& vec, PHLWINDOW pWindow, SP<CWLSurfaceResource> pSurface) {
    if (!validMapped(pWindow))
        return {};

    if (pWindow->m_bIsX11)
        return vec - pWindow->m_vRealPosition.goal();

    const auto PPOPUP = pWindow->m_pPopupHead->at(vec);
    if (PPOPUP)
        return vec - PPOPUP->coordsGlobal();

    std::tuple<SP<CWLSurfaceResource>, Vector2D> iterData = {pSurface, {-1337, -1337}};

    pWindow->m_pWLSurface->resource()->breadthfirst(
        [](SP<CWLSurfaceResource> surf, const Vector2D& offset, void* data) {
            const auto PDATA = (std::tuple<SP<CWLSurfaceResource>, Vector2D>*)data;
            if (surf == std::get<0>(*PDATA))
                std::get<1>(*PDATA) = offset;
        },
        &iterData);

    CBox geom = pWindow->m_pXDGSurface->current.geometry;

    if (std::get<1>(iterData) == Vector2D{-1337, -1337})
        return vec - pWindow->m_vRealPosition.goal();

    return vec - pWindow->m_vRealPosition.goal() - std::get<1>(iterData) + Vector2D{geom.x, geom.y};
}

CMonitor* CCompositor::getMonitorFromOutput(wlr_output* out) {
    for (auto& m : m_vMonitors) {
        if (m->output == out) {
            return m.get();
        }
    }

    return nullptr;
}

CMonitor* CCompositor::getRealMonitorFromOutput(wlr_output* out) {
    for (auto& m : m_vRealMonitors) {
        if (m->output == out) {
            return m.get();
        }
    }

    return nullptr;
}

void CCompositor::focusWindow(PHLWINDOW pWindow, SP<CWLSurfaceResource> pSurface) {

    static auto PFOLLOWMOUSE        = CConfigValue<Hyprlang::INT>("input:follow_mouse");
    static auto PSPECIALFALLTHROUGH = CConfigValue<Hyprlang::INT>("input:special_fallthrough");

    if (g_pSessionLockManager->isSessionLocked()) {
        Debug::log(LOG, "Refusing a keyboard focus to a window because of a sessionlock");
        return;
    }

    if (!g_pInputManager->m_dExclusiveLSes.empty()) {
        Debug::log(LOG, "Refusing a keyboard focus to a window because of an exclusive ls");
        return;
    }

    if (pWindow && pWindow->m_bIsX11 && pWindow->m_iX11Type == 2 && !pWindow->m_pXWaylandSurface->wantsFocus())
        return;

    g_pLayoutManager->getCurrentLayout()->bringWindowToTop(pWindow);

    if (!pWindow || !validMapped(pWindow)) {

        if (m_pLastWindow.expired() && !pWindow)
            return;

        const auto PLASTWINDOW = m_pLastWindow.lock();
        m_pLastWindow.reset();

        if (PLASTWINDOW && PLASTWINDOW->m_bIsMapped) {
            updateWindowAnimatedDecorationValues(PLASTWINDOW);

            g_pXWaylandManager->activateWindow(PLASTWINDOW, false);
        }

        g_pSeatManager->setKeyboardFocus(nullptr);

        g_pEventManager->postEvent(SHyprIPCEvent{"activewindow", ","});
        g_pEventManager->postEvent(SHyprIPCEvent{"activewindowv2", ""});

        EMIT_HOOK_EVENT("activeWindow", (PHLWINDOW) nullptr);

        g_pLayoutManager->getCurrentLayout()->onWindowFocusChange(nullptr);

        m_pLastFocus.reset();

        g_pInputManager->recheckIdleInhibitorStatus();
        return;
    }

    if (pWindow->m_sAdditionalConfigData.noFocus) {
        Debug::log(LOG, "Ignoring focus to nofocus window!");
        return;
    }

    if (m_pLastWindow.lock() == pWindow && g_pSeatManager->state.keyboardFocus == pSurface)
        return;

    if (pWindow->m_bPinned)
        pWindow->m_pWorkspace = m_pLastMonitor->activeWorkspace;

    const auto PMONITOR = getMonitorFromID(pWindow->m_iMonitorID);

    if (!isWorkspaceVisible(pWindow->m_pWorkspace)) {
        const auto PWORKSPACE = pWindow->m_pWorkspace;
        // This is to fix incorrect feedback on the focus history.
        PWORKSPACE->m_pLastFocusedWindow = pWindow;
        PWORKSPACE->rememberPrevWorkspace(m_pLastMonitor->activeWorkspace);
        if (PWORKSPACE->m_bIsSpecialWorkspace)
            m_pLastMonitor->changeWorkspace(PWORKSPACE, false, true); // if special ws, open on current monitor
        else
            PMONITOR->changeWorkspace(PWORKSPACE, false, true);
        // changeworkspace already calls focusWindow
        return;
    }

    const auto PLASTWINDOW = m_pLastWindow.lock();
    m_pLastWindow          = pWindow;

    /* If special fallthrough is enabled, this behavior will be disabled, as I have no better idea of nicely tracking which
       window focuses are "via keybinds" and which ones aren't. */
    if (PMONITOR->activeSpecialWorkspace && PMONITOR->activeSpecialWorkspace != pWindow->m_pWorkspace && !pWindow->m_bPinned && !*PSPECIALFALLTHROUGH)
        PMONITOR->setSpecialWorkspace(nullptr);

    // we need to make the PLASTWINDOW not equal to m_pLastWindow so that RENDERDATA is correct for an unfocused window
    if (PLASTWINDOW && PLASTWINDOW->m_bIsMapped) {
        PLASTWINDOW->updateDynamicRules();

        updateWindowAnimatedDecorationValues(PLASTWINDOW);

        if (!pWindow->m_bIsX11 || pWindow->m_iX11Type == 1)
            g_pXWaylandManager->activateWindow(PLASTWINDOW, false);
    }

    m_pLastWindow = PLASTWINDOW;

    const auto PWINDOWSURFACE = pSurface ? pSurface : pWindow->m_pWLSurface->resource();

    focusSurface(PWINDOWSURFACE, pWindow);

    g_pXWaylandManager->activateWindow(pWindow, true); // sets the m_pLastWindow

    pWindow->updateDynamicRules();

    updateWindowAnimatedDecorationValues(pWindow);

    if (pWindow->m_bIsUrgent)
        pWindow->m_bIsUrgent = false;

    // Send an event
    g_pEventManager->postEvent(SHyprIPCEvent{"activewindow", pWindow->m_szClass + "," + pWindow->m_szTitle});
    g_pEventManager->postEvent(SHyprIPCEvent{"activewindowv2", std::format("{:x}", (uintptr_t)pWindow.get())});

    EMIT_HOOK_EVENT("activeWindow", pWindow);

    g_pLayoutManager->getCurrentLayout()->onWindowFocusChange(pWindow);

    g_pInputManager->recheckIdleInhibitorStatus();

    // move to front of the window history
    const auto HISTORYPIVOT = std::find_if(m_vWindowFocusHistory.begin(), m_vWindowFocusHistory.end(), [&](const auto& other) { return other.lock() == pWindow; });
    if (HISTORYPIVOT == m_vWindowFocusHistory.end()) {
        Debug::log(ERR, "BUG THIS: {} has no pivot in history", pWindow);
    } else {
        std::rotate(m_vWindowFocusHistory.begin(), HISTORYPIVOT, HISTORYPIVOT + 1);
    }

    if (*PFOLLOWMOUSE == 0)
        g_pInputManager->sendMotionEventsToFocused();
}

void CCompositor::focusSurface(SP<CWLSurfaceResource> pSurface, PHLWINDOW pWindowOwner) {

    if (g_pSeatManager->state.keyboardFocus == pSurface || (pWindowOwner && g_pSeatManager->state.keyboardFocus == pWindowOwner->m_pWLSurface->resource()))
        return; // Don't focus when already focused on this.

    if (g_pSessionLockManager->isSessionLocked() && !g_pSessionLockManager->isSurfaceSessionLock(pSurface))
        return;

    if (g_pSeatManager->seatGrab && !g_pSeatManager->seatGrab->accepts(pSurface)) {
        Debug::log(LOG, "surface {:x} won't receive kb focus becuase grab rejected it", (uintptr_t)pSurface);
        return;
    }

    const auto PLASTSURF = m_pLastFocus.lock();

    // Unfocus last surface if should
    if (m_pLastFocus && !pWindowOwner)
        g_pXWaylandManager->activateSurface(m_pLastFocus.lock(), false);

    if (!pSurface) {
        g_pSeatManager->setKeyboardFocus(nullptr);
        g_pEventManager->postEvent(SHyprIPCEvent{"activewindow", ","}); // unfocused
        g_pEventManager->postEvent(SHyprIPCEvent{"activewindowv2", ""});
        EMIT_HOOK_EVENT("keyboardFocus", (SP<CWLSurfaceResource>)nullptr);
        m_pLastFocus.reset();
        return;
    }

    if (g_pSeatManager->keyboard)
        g_pSeatManager->setKeyboardFocus(pSurface);

    if (pWindowOwner)
        Debug::log(LOG, "Set keyboard focus to surface {:x}, with {}", (uintptr_t)pSurface, pWindowOwner);
    else
        Debug::log(LOG, "Set keyboard focus to surface {:x}", (uintptr_t)pSurface);

    g_pXWaylandManager->activateSurface(pSurface, true);
    m_pLastFocus = pSurface;

    EMIT_HOOK_EVENT("keyboardFocus", pSurface);

    const auto SURF    = CWLSurface::fromResource(pSurface);
    const auto OLDSURF = CWLSurface::fromResource(PLASTSURF);

    if (OLDSURF && OLDSURF->constraint())
        OLDSURF->constraint()->deactivate();

    if (SURF && SURF->constraint())
        SURF->constraint()->activate();
}

SP<CWLSurfaceResource> CCompositor::vectorToLayerPopupSurface(const Vector2D& pos, CMonitor* monitor, Vector2D* sCoords, PHLLS* ppLayerSurfaceFound) {
    for (auto& lsl : monitor->m_aLayerSurfaceLayers | std::views::reverse) {
        for (auto& ls : lsl | std::views::reverse) {
            if (ls->fadingOut || !ls->layerSurface || (ls->layerSurface && !ls->layerSurface->mapped) || ls->alpha.value() == 0.f)
                continue;

            auto SURFACEAT = ls->popupHead->at(pos, true);

            if (SURFACEAT) {
                *ppLayerSurfaceFound = ls.lock();
                *sCoords             = pos - SURFACEAT->coordsGlobal();
                return SURFACEAT->m_pWLSurface->resource();
            }
        }
    }

    return nullptr;
}

SP<CWLSurfaceResource> CCompositor::vectorToLayerSurface(const Vector2D& pos, std::vector<PHLLSREF>* layerSurfaces, Vector2D* sCoords, PHLLS* ppLayerSurfaceFound) {
    for (auto& ls : *layerSurfaces | std::views::reverse) {
        if (ls->fadingOut || !ls->layerSurface || (ls->layerSurface && !ls->layerSurface->surface->mapped) || ls->alpha.value() == 0.f)
            continue;

        auto [surf, local] = ls->layerSurface->surface->at(pos - ls->geometry.pos());

        if (surf) {
            if (surf->current.input.empty())
                continue;

            *ppLayerSurfaceFound = ls.lock();

            *sCoords = local;

            return surf;
        }
    }

    return nullptr;
}

PHLWINDOW CCompositor::getWindowFromSurface(SP<CWLSurfaceResource> pSurface) {
    for (auto& w : m_vWindows) {
        if (!w->m_bIsMapped || w->m_bFadingOut)
            continue;

        if (w->m_pWLSurface->resource() == pSurface)
            return w;
    }

    return nullptr;
}

PHLWINDOW CCompositor::getWindowFromHandle(uint32_t handle) {
    for (auto& w : m_vWindows) {
        if ((uint32_t)(((uint64_t)w.get()) & 0xFFFFFFFF) == handle) {
            return w;
        }
    }

    return nullptr;
}

PHLWINDOW CCompositor::getFullscreenWindowOnWorkspace(const int& ID) {
    for (auto& w : m_vWindows) {
        if (w->workspaceID() == ID && w->m_bIsFullscreen)
            return w;
    }

    return nullptr;
}

bool CCompositor::isWorkspaceVisible(PHLWORKSPACE w) {
    return valid(w) && w->m_bVisible;
}

PHLWORKSPACE CCompositor::getWorkspaceByID(const int& id) {
    for (auto& w : m_vWorkspaces) {
        if (w->m_iID == id && !w->inert())
            return w;
    }

    return nullptr;
}

void CCompositor::sanityCheckWorkspaces() {
    auto it = m_vWorkspaces.begin();
    while (it != m_vWorkspaces.end()) {
        const auto& WORKSPACE = *it;

        // If ref == 1, only the compositor holds a ref, which means it's inactive and has no mapped windows.
        if (!WORKSPACE->m_bPersistent && WORKSPACE.strongRef() == 1) {
            it = m_vWorkspaces.erase(it);
            continue;
        }

        ++it;
    }
}

int CCompositor::getWindowsOnWorkspace(const int& id, std::optional<bool> onlyTiled, std::optional<bool> onlyVisible) {
    int no = 0;
    for (auto& w : m_vWindows) {
        if (w->workspaceID() != id || !w->m_bIsMapped)
            continue;
        if (onlyTiled.has_value() && w->m_bIsFloating == onlyTiled.value())
            continue;
        if (onlyVisible.has_value() && w->isHidden() == onlyVisible.value())
            continue;
        no++;
    }

    return no;
}

int CCompositor::getGroupsOnWorkspace(const int& id, std::optional<bool> onlyTiled, std::optional<bool> onlyVisible) {
    int no = 0;
    for (auto& w : m_vWindows) {
        if (w->workspaceID() != id || !w->m_bIsMapped)
            continue;
        if (!w->m_sGroupData.head)
            continue;
        if (onlyTiled.has_value() && w->m_bIsFloating == onlyTiled.value())
            continue;
        if (onlyVisible.has_value() && w->isHidden() == onlyVisible.value())
            continue;
        no++;
    }
    return no;
}

PHLWINDOW CCompositor::getUrgentWindow() {
    for (auto& w : m_vWindows) {
        if (w->m_bIsMapped && w->m_bIsUrgent)
            return w;
    }

    return nullptr;
}

bool CCompositor::hasUrgentWindowOnWorkspace(const int& id) {
    for (auto& w : m_vWindows) {
        if (w->workspaceID() == id && w->m_bIsMapped && w->m_bIsUrgent)
            return true;
    }

    return false;
}

PHLWINDOW CCompositor::getFirstWindowOnWorkspace(const int& id) {
    for (auto& w : m_vWindows) {
        if (w->workspaceID() == id && w->m_bIsMapped && !w->isHidden())
            return w;
    }

    return nullptr;
}

PHLWINDOW CCompositor::getTopLeftWindowOnWorkspace(const int& id) {
    const auto PWORKSPACE = getWorkspaceByID(id);

    if (!PWORKSPACE)
        return nullptr;

    const auto PMONITOR = getMonitorFromID(PWORKSPACE->m_iMonitorID);

    for (auto& w : m_vWindows) {
        if (w->workspaceID() != id || !w->m_bIsMapped || w->isHidden())
            continue;

        const auto WINDOWIDEALBB = w->getWindowIdealBoundingBoxIgnoreReserved();

        if (WINDOWIDEALBB.x <= PMONITOR->vecPosition.x + 1 && WINDOWIDEALBB.y <= PMONITOR->vecPosition.y + 1)
            return w;
    }
    return nullptr;
}

bool CCompositor::isWindowActive(PHLWINDOW pWindow) {
    if (m_pLastWindow.expired() && !m_pLastFocus)
        return false;

    if (!pWindow->m_bIsMapped)
        return false;

    const auto PSURFACE = pWindow->m_pWLSurface->resource();

    return PSURFACE == m_pLastFocus || pWindow == m_pLastWindow.lock();
}

void CCompositor::changeWindowZOrder(PHLWINDOW pWindow, bool top) {
    if (!validMapped(pWindow))
        return;

    auto moveToZ = [&](PHLWINDOW pw, bool top) -> void {
        if (top) {
            for (auto it = m_vWindows.begin(); it != m_vWindows.end(); ++it) {
                if (*it == pw) {
                    std::rotate(it, it + 1, m_vWindows.end());
                    break;
                }
            }
        } else {
            for (auto it = m_vWindows.rbegin(); it != m_vWindows.rend(); ++it) {
                if (*it == pw) {
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

    if (!pWindow->m_bIsX11)
        moveToZ(pWindow, top);
    else {
        // move X11 window stack

        std::deque<PHLWINDOW> toMove;

        auto                  x11Stack = [&](PHLWINDOW pw, bool top, auto&& x11Stack) -> void {
            if (top)
                toMove.emplace_back(pw);
            else
                toMove.emplace_front(pw);

            for (auto& w : m_vWindows) {
                if (w->m_bIsMapped && !w->isHidden() && w->m_bIsX11 && w->X11TransientFor() == pw && w != pw && std::find(toMove.begin(), toMove.end(), w) == toMove.end()) {
                    x11Stack(w, top, x11Stack);
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
    for (auto& ww : m_vWindowsFadingOut) {

        auto w = ww.lock();

        if (w->m_iMonitorID != (long unsigned int)monid)
            continue;

        if (!w->m_bFadingOut || w->m_fAlpha.value() == 0.f) {

            w->m_bFadingOut = false;

            if (!w->m_bReadyToDelete)
                continue;

            removeWindowFromVectorSafe(w);

            w.reset();

            Debug::log(LOG, "Cleanup: destroyed a window");
            return;
        }
    }

    for (auto& lsr : m_vSurfacesFadingOut) {

        auto ls = lsr.lock();

        if (!ls)
            continue;

        if (ls->monitorID != monid)
            continue;

        // mark blur for recalc
        if (ls->layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND || ls->layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM)
            g_pHyprOpenGL->markBlurDirtyForMonitor(getMonitorFromID(monid));

        if (ls->fadingOut && ls->readyToDelete && ls->isFadedOut()) {
            for (auto& m : m_vMonitors) {
                for (auto& lsl : m->m_aLayerSurfaceLayers) {
                    if (!lsl.empty() && std::find_if(lsl.begin(), lsl.end(), [&](auto& other) { return other == ls; }) != lsl.end()) {
                        std::erase_if(lsl, [&](auto& other) { return other == ls; });
                    }
                }
            }

            std::erase_if(m_vSurfacesFadingOut, [ls](const auto& el) { return el.lock() == ls; });
            std::erase_if(m_vLayers, [ls](const auto& el) { return el == ls; });

            ls.reset();

            Debug::log(LOG, "Cleanup: destroyed a layersurface");

            glFlush(); // to free mem NOW.
            return;
        }
    }
}

void CCompositor::addToFadingOutSafe(PHLLS pLS) {
    const auto FOUND = std::find_if(m_vSurfacesFadingOut.begin(), m_vSurfacesFadingOut.end(), [&](auto& other) { return other.lock() == pLS; });

    if (FOUND != m_vSurfacesFadingOut.end())
        return; // if it's already added, don't add it.

    m_vSurfacesFadingOut.emplace_back(pLS);
}

void CCompositor::addToFadingOutSafe(PHLWINDOW pWindow) {
    const auto FOUND = std::find_if(m_vWindowsFadingOut.begin(), m_vWindowsFadingOut.end(), [&](PHLWINDOWREF& other) { return other.lock() == pWindow; });

    if (FOUND != m_vWindowsFadingOut.end())
        return; // if it's already added, don't add it.

    m_vWindowsFadingOut.emplace_back(pWindow);
}

PHLWINDOW CCompositor::getWindowInDirection(PHLWINDOW pWindow, char dir) {

    if (!isDirection(dir))
        return nullptr;

    // 0 -> history, 1 -> shared length
    static auto PMETHOD          = CConfigValue<Hyprlang::INT>("binds:focus_preferred_method");
    static auto PMONITORFALLBACK = CConfigValue<Hyprlang::INT>("binds:window_direction_monitor_fallback");

    const auto  PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    if (!PMONITOR)
        return nullptr; // ??

    const auto WINDOWIDEALBB = pWindow->m_bIsFullscreen ? wlr_box{(int)PMONITOR->vecPosition.x, (int)PMONITOR->vecPosition.y, (int)PMONITOR->vecSize.x, (int)PMONITOR->vecSize.y} :
                                                          pWindow->getWindowIdealBoundingBoxIgnoreReserved();

    const auto POSA  = Vector2D(WINDOWIDEALBB.x, WINDOWIDEALBB.y);
    const auto SIZEA = Vector2D(WINDOWIDEALBB.width, WINDOWIDEALBB.height);

    const auto PWORKSPACE   = pWindow->m_pWorkspace;
    auto       leaderValue  = -1;
    PHLWINDOW  leaderWindow = nullptr;

    if (!pWindow->m_bIsFloating) {

        // for tiled windows, we calc edges
        for (auto& w : m_vWindows) {
            if (w == pWindow || !w->m_bIsMapped || w->isHidden() || (!w->m_bIsFullscreen && w->m_bIsFloating) || !isWorkspaceVisible(w->m_pWorkspace))
                continue;

            if (pWindow->m_iMonitorID == w->m_iMonitorID && pWindow->m_pWorkspace != w->m_pWorkspace)
                continue;

            if (PWORKSPACE->m_bHasFullscreenWindow && !w->m_bIsFullscreen && !w->m_bCreatedOverFullscreen)
                continue;

            if (!*PMONITORFALLBACK && pWindow->m_iMonitorID != w->m_iMonitorID)
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
                        if (g_pCompositor->m_vWindowFocusHistory[i].lock() == w) {
                            windowIDX = i;
                            break;
                        }
                    }

                    windowIDX = g_pCompositor->m_vWindowFocusHistory.size() - windowIDX;

                    if (windowIDX > leaderValue) {
                        leaderValue  = windowIDX;
                        leaderWindow = w;
                    }
                }
            } else /* length */ {
                if (intersectLength > leaderValue) {
                    leaderValue  = intersectLength;
                    leaderWindow = w;
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
            if (w == pWindow || !w->m_bIsMapped || w->isHidden() || (!w->m_bIsFullscreen && !w->m_bIsFloating) || !isWorkspaceVisible(w->m_pWorkspace))
                continue;

            if (pWindow->m_iMonitorID == w->m_iMonitorID && pWindow->m_pWorkspace != w->m_pWorkspace)
                continue;

            if (PWORKSPACE->m_bHasFullscreenWindow && !w->m_bIsFullscreen && !w->m_bCreatedOverFullscreen)
                continue;

            if (!*PMONITORFALLBACK && pWindow->m_iMonitorID != w->m_iMonitorID)
                continue;

            const auto DIST  = w->middle().distance(pWindow->middle());
            const auto ANGLE = vectorAngles(Vector2D{w->middle() - pWindow->middle()}, VECTORS.at(dir));

            if (ANGLE > M_PI_2)
                continue; // if the angle is over 90 degrees, ignore. Wrong direction entirely.

            if ((bestAngleAbs < THRESHOLD && DIST < leaderValue && ANGLE < THRESHOLD) || (ANGLE < bestAngleAbs && bestAngleAbs > THRESHOLD) || leaderValue == -1) {
                leaderValue  = DIST;
                bestAngleAbs = ANGLE;
                leaderWindow = w;
            }
        }

        if (!leaderWindow && PWORKSPACE->m_bHasFullscreenWindow)
            leaderWindow = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);
    }

    if (leaderValue != -1)
        return leaderWindow;

    return nullptr;
}

PHLWINDOW CCompositor::getNextWindowOnWorkspace(PHLWINDOW pWindow, bool focusableOnly, std::optional<bool> floating) {
    bool gotToWindow = false;
    for (auto& w : m_vWindows) {
        if (w != pWindow && !gotToWindow)
            continue;

        if (w == pWindow) {
            gotToWindow = true;
            continue;
        }

        if (floating.has_value() && w->m_bIsFloating != floating.value())
            continue;

        if (w->m_pWorkspace == pWindow->m_pWorkspace && w->m_bIsMapped && !w->isHidden() && (!focusableOnly || !w->m_sAdditionalConfigData.noFocus))
            return w;
    }

    for (auto& w : m_vWindows) {
        if (floating.has_value() && w->m_bIsFloating != floating.value())
            continue;

        if (w != pWindow && w->m_pWorkspace == pWindow->m_pWorkspace && w->m_bIsMapped && !w->isHidden() && (!focusableOnly || !w->m_sAdditionalConfigData.noFocus))
            return w;
    }

    return nullptr;
}

PHLWINDOW CCompositor::getPrevWindowOnWorkspace(PHLWINDOW pWindow, bool focusableOnly, std::optional<bool> floating) {
    bool gotToWindow = false;
    for (auto& w : m_vWindows | std::views::reverse) {
        if (w != pWindow && !gotToWindow)
            continue;

        if (w == pWindow) {
            gotToWindow = true;
            continue;
        }

        if (floating.has_value() && w->m_bIsFloating != floating.value())
            continue;

        if (w->m_pWorkspace == pWindow->m_pWorkspace && w->m_bIsMapped && !w->isHidden() && (!focusableOnly || !w->m_sAdditionalConfigData.noFocus))
            return w;
    }

    for (auto& w : m_vWindows | std::views::reverse) {
        if (floating.has_value() && w->m_bIsFloating != floating.value())
            continue;

        if (w != pWindow && w->m_pWorkspace == pWindow->m_pWorkspace && w->m_bIsMapped && !w->isHidden() && (!focusableOnly || !w->m_sAdditionalConfigData.noFocus))
            return w;
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

PHLWORKSPACE CCompositor::getWorkspaceByName(const std::string& name) {
    for (auto& w : m_vWorkspaces) {
        if (w->m_szName == name && !w->inert())
            return w;
    }

    return nullptr;
}

PHLWORKSPACE CCompositor::getWorkspaceByString(const std::string& str) {
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

CMonitor* CCompositor::getMonitorInDirection(const char& dir) {
    return this->getMonitorInDirection(m_pLastMonitor.get(), dir);
}

CMonitor* CCompositor::getMonitorInDirection(CMonitor* pSourceMonitor, const char& dir) {
    if (!pSourceMonitor)
        return nullptr;

    const auto POSA  = pSourceMonitor->vecPosition;
    const auto SIZEA = pSourceMonitor->vecSize;

    auto       longestIntersect        = -1;
    CMonitor*  longestIntersectMonitor = nullptr;

    for (auto& m : m_vMonitors) {
        if (m == m_pLastMonitor)
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

        updateWindowAnimatedDecorationValues(w);
    }
}

void CCompositor::updateWorkspaceWindows(const int64_t& id) {
    for (auto& w : m_vWindows) {
        if (!w->m_bIsMapped || w->workspaceID() != id)
            continue;

        w->updateDynamicRules();
    }
}

void CCompositor::updateWindowAnimatedDecorationValues(PHLWINDOW pWindow) {
    // optimization
    static auto PACTIVECOL              = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.active_border");
    static auto PINACTIVECOL            = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.inactive_border");
    static auto PNOGROUPACTIVECOL       = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.nogroup_border_active");
    static auto PNOGROUPINACTIVECOL     = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.nogroup_border");
    static auto PGROUPACTIVECOL         = CConfigValue<Hyprlang::CUSTOMTYPE>("group:col.border_active");
    static auto PGROUPINACTIVECOL       = CConfigValue<Hyprlang::CUSTOMTYPE>("group:col.border_inactive");
    static auto PGROUPACTIVELOCKEDCOL   = CConfigValue<Hyprlang::CUSTOMTYPE>("group:col.border_locked_active");
    static auto PGROUPINACTIVELOCKEDCOL = CConfigValue<Hyprlang::CUSTOMTYPE>("group:col.border_locked_inactive");
    static auto PINACTIVEALPHA          = CConfigValue<Hyprlang::FLOAT>("decoration:inactive_opacity");
    static auto PACTIVEALPHA            = CConfigValue<Hyprlang::FLOAT>("decoration:active_opacity");
    static auto PFULLSCREENALPHA        = CConfigValue<Hyprlang::FLOAT>("decoration:fullscreen_opacity");
    static auto PSHADOWCOL              = CConfigValue<Hyprlang::INT>("decoration:col.shadow");
    static auto PSHADOWCOLINACTIVE      = CConfigValue<Hyprlang::INT>("decoration:col.shadow_inactive");
    static auto PDIMSTRENGTH            = CConfigValue<Hyprlang::FLOAT>("decoration:dim_strength");
    static auto PDIMENABLED             = CConfigValue<Hyprlang::INT>("decoration:dim_inactive");

    auto* const ACTIVECOL              = (CGradientValueData*)(PACTIVECOL.ptr())->getData();
    auto* const INACTIVECOL            = (CGradientValueData*)(PINACTIVECOL.ptr())->getData();
    auto* const NOGROUPACTIVECOL       = (CGradientValueData*)(PNOGROUPACTIVECOL.ptr())->getData();
    auto* const NOGROUPINACTIVECOL     = (CGradientValueData*)(PNOGROUPINACTIVECOL.ptr())->getData();
    auto* const GROUPACTIVECOL         = (CGradientValueData*)(PGROUPACTIVECOL.ptr())->getData();
    auto* const GROUPINACTIVECOL       = (CGradientValueData*)(PGROUPINACTIVECOL.ptr())->getData();
    auto* const GROUPACTIVELOCKEDCOL   = (CGradientValueData*)(PGROUPACTIVELOCKEDCOL.ptr())->getData();
    auto* const GROUPINACTIVELOCKEDCOL = (CGradientValueData*)(PGROUPINACTIVELOCKEDCOL.ptr())->getData();

    auto        setBorderColor = [&](CGradientValueData grad) -> void {
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
        const bool GROUPLOCKED = pWindow->m_sGroupData.pNextWindow.lock() ? pWindow->getGroupHead()->m_sGroupData.locked : false;
        if (pWindow == m_pLastWindow) {
            const auto* const ACTIVECOLOR =
                !pWindow->m_sGroupData.pNextWindow.lock() ? (!pWindow->m_sGroupData.deny ? ACTIVECOL : NOGROUPACTIVECOL) : (GROUPLOCKED ? GROUPACTIVELOCKEDCOL : GROUPACTIVECOL);
            setBorderColor(pWindow->m_sSpecialRenderData.activeBorderColor.toUnderlying().m_vColors.empty() ? *ACTIVECOLOR :
                                                                                                              pWindow->m_sSpecialRenderData.activeBorderColor.toUnderlying());
        } else {
            const auto* const INACTIVECOLOR = !pWindow->m_sGroupData.pNextWindow.lock() ? (!pWindow->m_sGroupData.deny ? INACTIVECOL : NOGROUPINACTIVECOL) :
                                                                                          (GROUPLOCKED ? GROUPINACTIVELOCKEDCOL : GROUPINACTIVECOL);
            setBorderColor(pWindow->m_sSpecialRenderData.inactiveBorderColor.toUnderlying().m_vColors.empty() ? *INACTIVECOLOR :
                                                                                                                pWindow->m_sSpecialRenderData.inactiveBorderColor.toUnderlying());
        }
    }

    // tick angle if it's not running (aka dead)
    if (!pWindow->m_fBorderAngleAnimationProgress.isBeingAnimated())
        pWindow->m_fBorderAngleAnimationProgress.setValueAndWarp(0.f);

    // opacity
    const auto PWORKSPACE = pWindow->m_pWorkspace;
    if (pWindow->m_bIsFullscreen && PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL) {
        pWindow->m_fActiveInactiveAlpha = pWindow->m_sSpecialRenderData.alphaFullscreen.toUnderlying() != -1 ?
            (pWindow->m_sSpecialRenderData.alphaFullscreenOverride.toUnderlying() ? pWindow->m_sSpecialRenderData.alphaFullscreen.toUnderlying() :
                                                                                    pWindow->m_sSpecialRenderData.alphaFullscreen.toUnderlying() * *PFULLSCREENALPHA) :
            *PFULLSCREENALPHA;
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
    if (pWindow == m_pLastWindow.lock() || pWindow->m_sAdditionalConfigData.forceNoDim || !*PDIMENABLED) {
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

    const auto PWORKSPACEA = pMonitorA->activeWorkspace;
    const auto PWORKSPACEB = pMonitorB->activeWorkspace;

    PWORKSPACEA->m_iMonitorID = pMonitorB->ID;
    PWORKSPACEA->moveToMonitor(pMonitorB->ID);

    for (auto& w : m_vWindows) {
        if (w->m_pWorkspace == PWORKSPACEA) {
            if (w->m_bPinned) {
                w->m_pWorkspace = PWORKSPACEB;
                continue;
            }

            w->m_iMonitorID = pMonitorB->ID;

            // additionally, move floating and fs windows manually
            if (w->m_bIsFloating)
                w->m_vRealPosition = w->m_vRealPosition.goal() - pMonitorA->vecPosition + pMonitorB->vecPosition;

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
        if (w->m_pWorkspace == PWORKSPACEB) {
            if (w->m_bPinned) {
                w->m_pWorkspace = PWORKSPACEA;
                continue;
            }

            w->m_iMonitorID = pMonitorA->ID;

            // additionally, move floating and fs windows manually
            if (w->m_bIsFloating)
                w->m_vRealPosition = w->m_vRealPosition.goal() - pMonitorB->vecPosition + pMonitorA->vecPosition;

            if (w->m_bIsFullscreen) {
                w->m_vRealPosition = pMonitorA->vecPosition;
                w->m_vRealSize     = pMonitorA->vecSize;
            }

            w->updateToplevel();
        }
    }

    pMonitorA->activeWorkspace = PWORKSPACEB;
    pMonitorB->activeWorkspace = PWORKSPACEA;

    PWORKSPACEA->rememberPrevWorkspace(PWORKSPACEB);
    PWORKSPACEB->rememberPrevWorkspace(PWORKSPACEA);

    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(pMonitorA->ID);
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(pMonitorB->ID);

    updateFullscreenFadeOnWorkspace(PWORKSPACEB);
    updateFullscreenFadeOnWorkspace(PWORKSPACEA);

    if (pMonitorA->ID == g_pCompositor->m_pLastMonitor->ID || pMonitorB->ID == g_pCompositor->m_pLastMonitor->ID) {
        const auto LASTWIN = pMonitorA->ID == g_pCompositor->m_pLastMonitor->ID ? PWORKSPACEB->getLastFocusedWindow() : PWORKSPACEA->getLastFocusedWindow();
        g_pCompositor->focusWindow(LASTWIN ? LASTWIN :
                                             (g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(), RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING)));

        const auto PNEWWORKSPACE = pMonitorA->ID == g_pCompositor->m_pLastMonitor->ID ? PWORKSPACEB : PWORKSPACEA;
        g_pEventManager->postEvent(SHyprIPCEvent{"workspace", PNEWWORKSPACE->m_szName});
        g_pEventManager->postEvent(SHyprIPCEvent{"workspacev2", std::format("{},{}", PNEWWORKSPACE->m_iID, PNEWWORKSPACE->m_szName)});
        EMIT_HOOK_EVENT("workspace", PNEWWORKSPACE);
    }

    // event
    g_pEventManager->postEvent(SHyprIPCEvent{"moveworkspace", PWORKSPACEA->m_szName + "," + pMonitorB->szName});
    g_pEventManager->postEvent(SHyprIPCEvent{"moveworkspacev2", std::format("{},{},{}", PWORKSPACEA->m_iID, PWORKSPACEA->m_szName, pMonitorB->szName)});
    EMIT_HOOK_EVENT("moveWorkspace", (std::vector<std::any>{PWORKSPACEA, pMonitorB}));
    g_pEventManager->postEvent(SHyprIPCEvent{"moveworkspace", PWORKSPACEB->m_szName + "," + pMonitorA->szName});
    g_pEventManager->postEvent(SHyprIPCEvent{"moveworkspacev2", std::format("{},{},{}", PWORKSPACEB->m_iID, PWORKSPACEB->m_szName, pMonitorA->szName)});
    EMIT_HOOK_EVENT("moveWorkspace", (std::vector<std::any>{PWORKSPACEB, pMonitorA}));
}

CMonitor* CCompositor::getMonitorFromString(const std::string& name) {
    if (name == "current")
        return g_pCompositor->m_pLastMonitor.get();
    else if (isDirection(name))
        return getMonitorInDirection(name[0]);
    else if (name[0] == '+' || name[0] == '-') {
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
            if (m_vMonitors[i] == m_pLastMonitor) {
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
    } else {
        for (auto& m : m_vMonitors) {
            if (!m->output)
                continue;

            if (m->matchesStaticSelector(name)) {
                return m.get();
            }
        }
    }

    return nullptr;
}

void CCompositor::moveWorkspaceToMonitor(PHLWORKSPACE pWorkspace, CMonitor* pMonitor, bool noWarpCursor) {

    // We trust the monitor to be correct.

    if (pWorkspace->m_iMonitorID == pMonitor->ID)
        return;

    Debug::log(LOG, "moveWorkspaceToMonitor: Moving {} to monitor {}", pWorkspace->m_iID, pMonitor->ID);

    const auto POLDMON = getMonitorFromID(pWorkspace->m_iMonitorID);

    const bool SWITCHINGISACTIVE = POLDMON ? POLDMON->activeWorkspace == pWorkspace : false;

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
        POLDMON->changeWorkspace(nextWorkspaceOnMonitorID, false, true, true);
    }

    // move the workspace
    pWorkspace->m_iMonitorID = pMonitor->ID;
    pWorkspace->moveToMonitor(pMonitor->ID);

    for (auto& w : m_vWindows) {
        if (w->m_pWorkspace == pWorkspace) {
            if (w->m_bPinned) {
                w->m_pWorkspace = g_pCompositor->getWorkspaceByID(nextWorkspaceOnMonitorID);
                continue;
            }

            w->m_iMonitorID = pMonitor->ID;

            // additionally, move floating and fs windows manually
            if (w->m_bIsMapped && !w->isHidden()) {
                if (POLDMON) {
                    if (w->m_bIsFloating)
                        w->m_vRealPosition = w->m_vRealPosition.goal() - POLDMON->vecPosition + pMonitor->vecPosition;

                    if (w->m_bIsFullscreen) {
                        w->m_vRealPosition = pMonitor->vecPosition;
                        w->m_vRealSize     = pMonitor->vecSize;
                    }
                } else {
                    w->m_vRealPosition = Vector2D{(int)w->m_vRealPosition.goal().x % (int)pMonitor->vecSize.x, (int)w->m_vRealPosition.goal().y % (int)pMonitor->vecSize.y};
                }
            }

            w->updateToplevel();
        }
    }

    if (SWITCHINGISACTIVE && POLDMON == g_pCompositor->m_pLastMonitor.get()) { // if it was active, preserve its' status. If it wasn't, don't.
        Debug::log(LOG, "moveWorkspaceToMonitor: SWITCHINGISACTIVE, active {} -> {}", pMonitor->activeWorkspaceID(), pWorkspace->m_iID);

        if (valid(pMonitor->activeWorkspace)) {
            pMonitor->activeWorkspace->m_bVisible = false;
            pMonitor->activeWorkspace->startAnim(false, false);
        }

        setActiveMonitor(pMonitor);
        pMonitor->activeWorkspace = pWorkspace;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(pMonitor->ID);

        pWorkspace->startAnim(true, true, true);
        pWorkspace->m_bVisible = true;

        if (!noWarpCursor)
            g_pPointerManager->warpTo(pMonitor->vecPosition + pMonitor->vecTransformedSize / 2.F);

        g_pInputManager->sendMotionEventsToFocused();
    }

    // finalize
    if (POLDMON) {
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(POLDMON->ID);
        updateFullscreenFadeOnWorkspace(POLDMON->activeWorkspace);
    }

    updateFullscreenFadeOnWorkspace(pWorkspace);

    // event
    g_pEventManager->postEvent(SHyprIPCEvent{"moveworkspace", pWorkspace->m_szName + "," + pMonitor->szName});
    g_pEventManager->postEvent(SHyprIPCEvent{"moveworkspacev2", std::format("{},{},{}", pWorkspace->m_iID, pWorkspace->m_szName, pMonitor->szName)});
    EMIT_HOOK_EVENT("moveWorkspace", (std::vector<std::any>{pWorkspace, pMonitor}));
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

void CCompositor::updateFullscreenFadeOnWorkspace(PHLWORKSPACE pWorkspace) {

    const auto FULLSCREEN = pWorkspace->m_bHasFullscreenWindow;

    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_pWorkspace == pWorkspace) {

            if (w->m_bFadingOut || w->m_bPinned || w->m_bIsFullscreen)
                continue;

            if (!FULLSCREEN)
                w->m_fAlpha = 1.f;
            else if (!w->m_bIsFullscreen)
                w->m_fAlpha = !w->m_bCreatedOverFullscreen ? 0.f : 1.f;
        }
    }

    const auto PMONITOR = getMonitorFromID(pWorkspace->m_iMonitorID);

    if (pWorkspace->m_iID == PMONITOR->activeWorkspaceID() || pWorkspace->m_iID == PMONITOR->activeSpecialWorkspaceID()) {
        for (auto& ls : PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
            if (!ls->fadingOut)
                ls->alpha = FULLSCREEN && pWorkspace->m_efFullscreenMode == FULLSCREEN_FULL ? 0.f : 1.f;
        }
    }
}

void CCompositor::setWindowFullscreen(PHLWINDOW pWindow, bool on, eFullscreenMode mode) {
    if (!validMapped(pWindow) || g_pCompositor->m_bUnsafeState)
        return;

    if (pWindow->m_bPinned) {
        Debug::log(LOG, "Pinned windows cannot be fullscreen'd");
        return;
    }

    if (pWindow->m_bIsFullscreen == on) {
        Debug::log(LOG, "Window is already in the required fullscreen state");
        return;
    }

    const auto PMONITOR = getMonitorFromID(pWindow->m_iMonitorID);

    const auto PWORKSPACE = pWindow->m_pWorkspace;

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
        if (w->m_pWorkspace == PWORKSPACE && !w->m_bIsFullscreen && !w->m_bFadingOut && !w->m_bPinned)
            w->m_bCreatedOverFullscreen = false;
    }
    updateFullscreenFadeOnWorkspace(PWORKSPACE);

    g_pXWaylandManager->setWindowSize(pWindow, pWindow->m_vRealSize.goal(), true);

    forceReportSizesToWindowsOnWorkspace(pWindow->workspaceID());

    g_pInputManager->recheckIdleInhibitorStatus();

    // DMAbuf stuff for direct scanout
    g_pHyprRenderer->setWindowScanoutMode(pWindow);

    g_pConfigManager->ensureVRR(PMONITOR);
}

PHLWINDOW CCompositor::getX11Parent(PHLWINDOW pWindow) {
    if (!pWindow->m_bIsX11)
        return nullptr;

    for (auto& w : m_vWindows) {
        if (!w->m_bIsX11)
            continue;

        if (w->m_pXWaylandSurface == pWindow->m_pXWaylandSurface->parent)
            return w;
    }

    return nullptr;
}

void CCompositor::updateWorkspaceWindowDecos(const int& id) {
    for (auto& w : m_vWindows) {
        if (w->workspaceID() != id)
            continue;

        w->updateWindowDecos();
    }
}

void CCompositor::updateWorkspaceSpecialRenderData(const int& id) {
    const auto PWORKSPACE    = getWorkspaceByID(id);
    const auto WORKSPACERULE = PWORKSPACE ? g_pConfigManager->getWorkspaceRuleFor(PWORKSPACE) : SWorkspaceRule{};

    for (auto& w : m_vWindows) {
        if (w->workspaceID() != id)
            continue;

        w->updateSpecialRenderData(WORKSPACERULE);
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

PHLWINDOW CCompositor::getWindowByRegex(const std::string& regexp) {
    if (regexp.starts_with("active"))
        return m_pLastWindow.lock();

    eFocusWindowMode mode = MODE_CLASS_REGEX;

    std::regex       regexCheck(regexp);
    std::string      matchCheck;
    if (regexp.starts_with("class:")) {
        regexCheck = std::regex(regexp.substr(6));
    } else if (regexp.starts_with("initialclass:")) {
        mode       = MODE_INITIAL_CLASS_REGEX;
        regexCheck = std::regex(regexp.substr(13));
    } else if (regexp.starts_with("title:")) {
        mode       = MODE_TITLE_REGEX;
        regexCheck = std::regex(regexp.substr(6));
    } else if (regexp.starts_with("initialtitle:")) {
        mode       = MODE_INITIAL_TITLE_REGEX;
        regexCheck = std::regex(regexp.substr(13));
    } else if (regexp.starts_with("address:")) {
        mode       = MODE_ADDRESS;
        matchCheck = regexp.substr(8);
    } else if (regexp.starts_with("pid:")) {
        mode       = MODE_PID;
        matchCheck = regexp.substr(4);
    } else if (regexp.starts_with("floating") || regexp.starts_with("tiled")) {
        // first floating on the current ws
        if (!valid(m_pLastWindow))
            return nullptr;

        const bool FLOAT = regexp.starts_with("floating");

        for (auto& w : m_vWindows) {
            if (!w->m_bIsMapped || w->m_bIsFloating != FLOAT || w->m_pWorkspace != m_pLastWindow->m_pWorkspace || w->isHidden())
                continue;

            return w;
        }

        return nullptr;
    }

    for (auto& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped || (w->isHidden() && !g_pLayoutManager->getCurrentLayout()->isWindowReachable(w)))
            continue;

        switch (mode) {
            case MODE_CLASS_REGEX: {
                const auto windowClass = w->m_szClass;
                if (!std::regex_search(windowClass, regexCheck))
                    continue;
                break;
            }
            case MODE_INITIAL_CLASS_REGEX: {
                const auto initialWindowClass = w->m_szInitialClass;
                if (!std::regex_search(initialWindowClass, regexCheck))
                    continue;
                break;
            }
            case MODE_TITLE_REGEX: {
                const auto windowTitle = w->m_szTitle;
                if (!std::regex_search(windowTitle, regexCheck))
                    continue;
                break;
            }
            case MODE_INITIAL_TITLE_REGEX: {
                const auto initialWindowTitle = w->m_szInitialTitle;
                if (!std::regex_search(initialWindowTitle, regexCheck))
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

        return w;
    }

    return nullptr;
}

void CCompositor::warpCursorTo(const Vector2D& pos, bool force) {

    // warpCursorTo should only be used for warps that
    // should be disabled with no_warps

    static auto PNOWARPS = CConfigValue<Hyprlang::INT>("cursor:no_warps");

    if (*PNOWARPS && !force) {
        const auto PMONITORNEW = getMonitorFromVector(pos);
        if (PMONITORNEW != m_pLastMonitor.get())
            setActiveMonitor(PMONITORNEW);
        return;
    }

    g_pPointerManager->warpTo(pos);

    const auto PMONITORNEW = getMonitorFromVector(pos);
    if (PMONITORNEW != m_pLastMonitor.get())
        setActiveMonitor(PMONITORNEW);
}

void CCompositor::closeWindow(PHLWINDOW pWindow) {
    if (pWindow && validMapped(pWindow)) {
        g_pXWaylandManager->sendCloseWindow(pWindow);
    }
}

PHLLS CCompositor::getLayerSurfaceFromSurface(SP<CWLSurfaceResource> pSurface) {
    std::pair<SP<CWLSurfaceResource>, bool> result = {pSurface, false};

    for (auto& ls : m_vLayers) {
        if (ls->layerSurface && ls->layerSurface->surface == pSurface)
            return ls;

        if (!ls->layerSurface || !ls->mapped)
            continue;

        ls->layerSurface->surface->breadthfirst(
            [](SP<CWLSurfaceResource> surf, const Vector2D& offset, void* data) {
                if (surf == ((std::pair<SP<CWLSurfaceResource>, bool>*)data)->first) {
                    *(bool*)data = true;
                    return;
                }
            },
            &result);

        if (result.second)
            return ls;
    }

    return nullptr;
}

// returns a delta
Vector2D CCompositor::parseWindowVectorArgsRelative(const std::string& args, const Vector2D& relativeTo) {
    if (!args.contains(' ') && !args.contains('\t'))
        return relativeTo;

    const auto  PMONITOR = m_pLastMonitor;

    bool        xIsPercent = false;
    bool        yIsPercent = false;
    bool        isExact    = false;

    CVarList    varList(args, 0, 's', true);
    std::string x = varList[0];
    std::string y = varList[1];

    if (x == "exact") {
        x       = varList[1];
        y       = varList[2];
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
        if (w->workspaceID() == wid && w->m_bIsMapped && !w->isHidden()) {
            g_pXWaylandManager->setWindowSize(w, w->m_vRealSize.value(), true);
        }
    }
}

PHLWORKSPACE CCompositor::createNewWorkspace(const int& id, const int& monid, const std::string& name, bool isEmtpy) {
    const auto NAME  = name == "" ? std::to_string(id) : name;
    auto       monID = monid;

    // check if bound
    if (const auto PMONITOR = g_pConfigManager->getBoundMonitorForWS(NAME); PMONITOR) {
        monID = PMONITOR->ID;
    }

    const bool SPECIAL = id >= SPECIAL_WORKSPACE_START && id <= -2;

    const auto PWORKSPACE = m_vWorkspaces.emplace_back(CWorkspace::create(id, monID, NAME, SPECIAL, isEmtpy));

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
    if (m_pLastMonitor.get() == pMonitor)
        return;

    if (!pMonitor) {
        m_pLastMonitor.reset();
        return;
    }

    const auto PWORKSPACE = pMonitor->activeWorkspace;

    g_pEventManager->postEvent(SHyprIPCEvent{"focusedmon", pMonitor->szName + "," + (PWORKSPACE ? PWORKSPACE->m_szName : "?")});
    EMIT_HOOK_EVENT("focusedMon", pMonitor);
    m_pLastMonitor = pMonitor->self;
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

void CCompositor::moveWindowToWorkspaceSafe(PHLWINDOW pWindow, PHLWORKSPACE pWorkspace) {
    if (!pWindow || !pWorkspace)
        return;

    if (pWindow->m_bPinned && pWorkspace->m_bIsSpecialWorkspace)
        return;

    const bool FULLSCREEN     = pWindow->m_bIsFullscreen;
    const auto FULLSCREENMODE = pWindow->m_pWorkspace->m_efFullscreenMode;

    if (FULLSCREEN)
        setWindowFullscreen(pWindow, false, FULLSCREEN_FULL);

    if (!pWindow->m_bIsFloating) {
        g_pLayoutManager->getCurrentLayout()->onWindowRemovedTiling(pWindow);
        pWindow->moveToWorkspace(pWorkspace);
        pWindow->m_iMonitorID = pWorkspace->m_iMonitorID;
        g_pLayoutManager->getCurrentLayout()->onWindowCreatedTiling(pWindow);
    } else {
        const auto PWINDOWMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
        const auto POSTOMON       = pWindow->m_vRealPosition.goal() - PWINDOWMONITOR->vecPosition;

        const auto PWORKSPACEMONITOR = g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID);

        pWindow->moveToWorkspace(pWorkspace);
        pWindow->m_iMonitorID = pWorkspace->m_iMonitorID;

        pWindow->m_vRealPosition = POSTOMON + PWORKSPACEMONITOR->vecPosition;
    }

    pWindow->updateToplevel();
    pWindow->updateDynamicRules();
    pWindow->uncacheWindowDecos();

    if (!pWindow->m_sGroupData.pNextWindow.expired()) {
        PHLWINDOW next = pWindow->m_sGroupData.pNextWindow.lock();
        while (next != pWindow) {
            next->moveToWorkspace(pWorkspace);
            next->updateToplevel();
            next = next->m_sGroupData.pNextWindow.lock();
        }
    }

    if (FULLSCREEN)
        setWindowFullscreen(pWindow, true, FULLSCREENMODE);

    g_pCompositor->updateWorkspaceWindows(pWorkspace->m_iID);
    g_pCompositor->updateWorkspaceWindows(pWindow->workspaceID());
}

PHLWINDOW CCompositor::getForceFocus() {
    for (auto& w : m_vWindows) {
        if (!w->m_bIsMapped || w->isHidden() || !isWorkspaceVisible(w->m_pWorkspace))
            continue;

        if (!w->m_bStayFocused)
            continue;

        return w;
    }

    return nullptr;
}

void CCompositor::arrangeMonitors() {
    static auto* const     PXWLFORCESCALEZERO = (Hyprlang::INT* const*)g_pConfigManager->getConfigValuePtr("xwayland:force_zero_scaling");

    std::vector<CMonitor*> toArrange;
    std::vector<CMonitor*> arranged;

    for (auto& m : m_vMonitors)
        toArrange.push_back(m.get());

    Debug::log(LOG, "arrangeMonitors: {} to arrange", toArrange.size());

    for (auto it = toArrange.begin(); it != toArrange.end();) {
        auto m = *it;

        if (m->activeMonitorRule.offset != Vector2D{-INT32_MAX, -INT32_MAX}) {
            // explicit.
            Debug::log(LOG, "arrangeMonitors: {} explicit {:j}", m->szName, m->activeMonitorRule.offset);

            m->moveTo(m->activeMonitorRule.offset);
            arranged.push_back(m);
            it = toArrange.erase(it);

            if (it == toArrange.end())
                break;

            continue;
        }

        ++it;
    }

    // Variables to store the max and min values of monitors on each axis.
    int maxXOffsetRight = 0;
    int maxXOffsetLeft  = 0;
    int maxYOffsetUp    = 0;
    int maxYOffsetDown  = 0;

    // Finds the max and min values of explicitely placed monitors.
    for (auto& m : arranged) {
        if (m->vecPosition.x + m->vecSize.x > maxXOffsetRight)
            maxXOffsetRight = m->vecPosition.x + m->vecSize.x;
        if (m->vecPosition.x < maxXOffsetLeft)
            maxXOffsetLeft = m->vecPosition.x;
        if (m->vecPosition.y + m->vecSize.y > maxYOffsetDown)
            maxYOffsetDown = m->vecPosition.y + m->vecSize.y;
        if (m->vecPosition.y < maxYOffsetUp)
            maxYOffsetUp = m->vecPosition.y;
    }

    // Iterates through all non-explicitly placed monitors.
    for (auto& m : toArrange) {
        // Moves the monitor to their appropriate position on the x/y axis and
        // increments/decrements the corresponding max offset.
        Vector2D newPosition = {0, 0};
        switch (m->activeMonitorRule.autoDir) {
            case eAutoDirs::DIR_AUTO_UP:
                newPosition.y = maxYOffsetUp - m->vecSize.y;
                maxYOffsetUp  = newPosition.y;
                break;
            case eAutoDirs::DIR_AUTO_DOWN:
                newPosition.y = maxYOffsetDown;
                maxYOffsetDown += m->vecSize.y;
                break;
            case eAutoDirs::DIR_AUTO_LEFT:
                newPosition.x  = maxXOffsetLeft - m->vecSize.x;
                maxXOffsetLeft = newPosition.x;
                break;
            case eAutoDirs::DIR_AUTO_RIGHT:
            case eAutoDirs::DIR_AUTO_NONE:
                newPosition.x = maxXOffsetRight;
                maxXOffsetRight += m->vecSize.x;
                break;
            default: UNREACHABLE();
        }
        Debug::log(LOG, "arrangeMonitors: {} auto {:j}", m->szName, m->vecPosition);
        m->moveTo(newPosition);
    }

    // reset maxXOffsetRight (reuse)
    // and set xwayland positions aka auto for all
    maxXOffsetRight = 0;
    for (auto& m : m_vMonitors) {
        Debug::log(LOG, "arrangeMonitors: {} xwayland [{}, {}]", m->szName, maxXOffsetRight, 0);
        m->vecXWaylandPosition = {maxXOffsetRight, 0};
        maxXOffsetRight += (*PXWLFORCESCALEZERO ? m->vecTransformedSize.x : m->vecSize.x);

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

    setActiveMonitor(m_pUnsafeOutput);
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

void CCompositor::setPreferredScaleForSurface(SP<CWLSurfaceResource> pSurface, double scale) {
    PROTO::fractional->sendScale(pSurface, scale);
    pSurface->sendPreferredScale(std::ceil(scale));

    const auto PSURFACE = CWLSurface::fromResource(pSurface);
    if (!PSURFACE) {
        Debug::log(WARN, "Orphaned CWLSurfaceResource {:x} in setPreferredScaleForSurface", (uintptr_t)pSurface);
        return;
    }

    PSURFACE->m_fLastScale = scale;
    PSURFACE->m_iLastScale = static_cast<int32_t>(std::ceil(scale));
}

void CCompositor::setPreferredTransformForSurface(SP<CWLSurfaceResource> pSurface, wl_output_transform transform) {
    pSurface->sendPreferredTransform(transform);

    const auto PSURFACE = CWLSurface::fromResource(pSurface);
    if (!PSURFACE) {
        Debug::log(WARN, "Orphaned CWLSurfaceResource {:x} in setPreferredTransformForSurface", (uintptr_t)pSurface);
        return;
    }

    PSURFACE->m_eLastTransform = transform;
}

void CCompositor::updateSuspendedStates() {
    for (auto& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped)
            continue;

        w->setSuspended(w->isHidden() || !isWorkspaceVisible(w->m_pWorkspace));
    }
}

PHLWINDOW CCompositor::windowForCPointer(CWindow* pWindow) {
    for (auto& w : m_vWindows) {
        if (w.get() != pWindow)
            continue;

        return w;
    }

    return {};
}
