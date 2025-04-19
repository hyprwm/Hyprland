#include <ranges>
#include <re2/re2.h>

#include "Compositor.hpp"
#include "debug/Log.hpp"
#include "desktop/DesktopTypes.hpp"
#include "helpers/Splashes.hpp"
#include "config/ConfigValue.hpp"
#include "config/ConfigWatcher.hpp"
#include "managers/CursorManager.hpp"
#include "managers/TokenManager.hpp"
#include "managers/PointerManager.hpp"
#include "managers/SeatManager.hpp"
#include "managers/VersionKeeperManager.hpp"
#include "managers/DonationNagManager.hpp"
#include "managers/ANRManager.hpp"
#include "managers/eventLoop/EventLoopManager.hpp"
#include "managers/permissions/DynamicPermissionManager.hpp"
#include <algorithm>
#include <aquamarine/output/Output.hpp>
#include <bit>
#include <ctime>
#include <random>
#include <print>
#include <cstring>
#include <filesystem>
#include <unordered_set>
#include "debug/HyprCtl.hpp"
#include "debug/CrashReporter.hpp"
#ifdef USES_SYSTEMD
#include <helpers/SdDaemon.hpp> // for SdNotify
#endif
#include "helpers/fs/FsUtils.hpp"
#include "protocols/FractionalScale.hpp"
#include "protocols/PointerConstraints.hpp"
#include "protocols/LayerShell.hpp"
#include "protocols/XDGShell.hpp"
#include "protocols/XDGOutput.hpp"
#include "protocols/SecurityContext.hpp"
#include "protocols/ColorManagement.hpp"
#include "protocols/core/Compositor.hpp"
#include "protocols/core/Subcompositor.hpp"
#include "desktop/LayerSurface.hpp"
#include "render/Renderer.hpp"
#include "xwayland/XWayland.hpp"
#include "helpers/ByteOperations.hpp"
#include "render/decorations/CHyprGroupBarDecoration.hpp"

#include "managers/KeybindManager.hpp"
#include "managers/SessionLockManager.hpp"
#include "managers/XWaylandManager.hpp"

#include "config/ConfigManager.hpp"
#include "render/OpenGL.hpp"
#include "managers/input/InputManager.hpp"
#include "managers/AnimationManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/HookSystemManager.hpp"
#include "managers/ProtocolManager.hpp"
#include "managers/LayoutManager.hpp"
#include "plugins/PluginSystem.hpp"
#include "hyprerror/HyprError.hpp"
#include "debug/HyprNotificationOverlay.hpp"
#include "debug/HyprDebugOverlay.hpp"

#include <hyprutils/string/String.hpp>
#include <aquamarine/input/Input.hpp>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <malloc.h>
#include <unistd.h>

using namespace Hyprutils::String;
using namespace Aquamarine;
using enum NContentType::eContentType;
using namespace NColorManagement;

static int handleCritSignal(int signo, void* data) {
    Debug::log(LOG, "Hyprland received signal {}", signo);

    if (signo == SIGTERM || signo == SIGINT || signo == SIGKILL)
        g_pCompositor->stopCompositor();

    return 0;
}

static void handleUnrecoverableSignal(int sig) {

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

    NCrashReporter::createAndSaveCrash(sig);

    abort();
}

static void handleUserSignal(int sig) {
    if (sig == SIGUSR1) {
        // means we have to unwind a timed out event
        throw std::exception();
    }
}

static eLogLevel aqLevelToHl(Aquamarine::eBackendLogLevel level) {
    switch (level) {
        case Aquamarine::eBackendLogLevel::AQ_LOG_TRACE: return TRACE;
        case Aquamarine::eBackendLogLevel::AQ_LOG_DEBUG: return LOG;
        case Aquamarine::eBackendLogLevel::AQ_LOG_ERROR: return ERR;
        case Aquamarine::eBackendLogLevel::AQ_LOG_WARNING: return WARN;
        case Aquamarine::eBackendLogLevel::AQ_LOG_CRITICAL: return CRIT;
        default: break;
    }

    return NONE;
}

static void aqLog(Aquamarine::eBackendLogLevel level, std::string msg) {
    Debug::log(aqLevelToHl(level), "[AQ] {}", msg);
}

void CCompositor::bumpNofile() {
    if (!getrlimit(RLIMIT_NOFILE, &m_sOriginalNofile))
        Debug::log(LOG, "Old rlimit: soft -> {}, hard -> {}", m_sOriginalNofile.rlim_cur, m_sOriginalNofile.rlim_max);
    else {
        Debug::log(ERR, "Failed to get NOFILE rlimits");
        m_sOriginalNofile.rlim_max = 0;
        return;
    }

    rlimit newLimit = m_sOriginalNofile;

    newLimit.rlim_cur = newLimit.rlim_max;

    if (setrlimit(RLIMIT_NOFILE, &newLimit) < 0) {
        Debug::log(ERR, "Failed bumping NOFILE limits higher");
        m_sOriginalNofile.rlim_max = 0;
        return;
    }

    if (!getrlimit(RLIMIT_NOFILE, &newLimit))
        Debug::log(LOG, "New rlimit: soft -> {}, hard -> {}", newLimit.rlim_cur, newLimit.rlim_max);
}

void CCompositor::restoreNofile() {
    if (m_sOriginalNofile.rlim_max <= 0)
        return;

    if (setrlimit(RLIMIT_NOFILE, &m_sOriginalNofile) < 0)
        Debug::log(ERR, "Failed restoring NOFILE limits");
}

void CCompositor::setMallocThreshold() {
#ifdef M_TRIM_THRESHOLD
    // The default is 128 pages,
    // which is very large and can lead to a lot of memory used for no reason
    // because trimming hasn't happened
    static const int PAGESIZE = sysconf(_SC_PAGESIZE);
    mallopt(M_TRIM_THRESHOLD, 6 * PAGESIZE);
#endif
}

CCompositor::CCompositor(bool onlyConfig) : m_bOnlyConfigVerification(onlyConfig), m_iHyprlandPID(getpid()) {
    if (onlyConfig)
        return;

    setMallocThreshold();

    m_szHyprTempDataRoot = std::string{getenv("XDG_RUNTIME_DIR")} + "/hypr";

    if (m_szHyprTempDataRoot.starts_with("/hypr")) {
        std::println("Bailing out, $XDG_RUNTIME_DIR is invalid");
        throw std::runtime_error("CCompositor() failed");
    }

    if (!m_szHyprTempDataRoot.starts_with("/run/user"))
        std::println("[!!WARNING!!] XDG_RUNTIME_DIR looks non-standard. Proceeding anyways...");

    std::random_device              dev;
    std::mt19937                    engine(dev());
    std::uniform_int_distribution<> distribution(0, INT32_MAX);

    m_szInstanceSignature = std::format("{}_{}_{}", GIT_COMMIT_HASH, std::time(nullptr), distribution(engine));

    setenv("HYPRLAND_INSTANCE_SIGNATURE", m_szInstanceSignature.c_str(), true);

    if (!std::filesystem::exists(m_szHyprTempDataRoot))
        mkdir(m_szHyprTempDataRoot.c_str(), S_IRWXU);
    else if (!std::filesystem::is_directory(m_szHyprTempDataRoot)) {
        std::println("Bailing out, {} is not a directory", m_szHyprTempDataRoot);
        throw std::runtime_error("CCompositor() failed");
    }

    m_szInstancePath = m_szHyprTempDataRoot + "/" + m_szInstanceSignature;

    if (std::filesystem::exists(m_szInstancePath)) {
        std::println("Bailing out, {} exists??", m_szInstancePath);
        throw std::runtime_error("CCompositor() failed");
    }

    if (mkdir(m_szInstancePath.c_str(), S_IRWXU) < 0) {
        std::println("Bailing out, couldn't create {}", m_szInstancePath);
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

    bumpNofile();
}

CCompositor::~CCompositor() {
    if (!m_bIsShuttingDown && !m_bOnlyConfigVerification)
        cleanup();
}

void CCompositor::setRandomSplash() {
    auto        tt    = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto        local = *localtime(&tt);

    const auto* SPLASHES = &NSplashes::SPLASHES;

    if (local.tm_mon + 1 == 12 && local.tm_mday >= 23 && local.tm_mday <= 27) // dec 23-27
        SPLASHES = &NSplashes::SPLASHES_CHRISTMAS;
    if ((local.tm_mon + 1 == 12 && local.tm_mday >= 29) || (local.tm_mon + 1 == 1 && local.tm_mday <= 3))
        SPLASHES = &NSplashes::SPLASHES_NEWYEAR;

    std::random_device              dev;
    std::mt19937                    engine(dev());
    std::uniform_int_distribution<> distribution(0, SPLASHES->size() - 1);

    m_szCurrentSplash = SPLASHES->at(distribution(engine));
}

static std::vector<SP<Aquamarine::IOutput>> pendingOutputs;

//

static bool filterGlobals(const wl_client* client, const wl_global* global, void* data) {
    if (!PROTO::securityContext->isClientSandboxed(client))
        return true;

    return !g_pProtocolManager || !g_pProtocolManager->isGlobalPrivileged(global);
}

//
void CCompositor::initServer(std::string socketName, int socketFd) {
    if (m_bOnlyConfigVerification) {
        g_pHookSystem       = makeUnique<CHookSystemManager>();
        g_pKeybindManager   = makeUnique<CKeybindManager>();
        g_pAnimationManager = makeUnique<CHyprAnimationManager>();
        g_pConfigManager    = makeUnique<CConfigManager>();

        std::println("\n\n======== Config parsing result:\n\n{}", g_pConfigManager->verify());
        return;
    }

    m_sWLDisplay = wl_display_create();

    wl_display_set_global_filter(m_sWLDisplay, ::filterGlobals, nullptr);

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

    // set the buffer size to 1MB to avoid disconnects due to an app hanging for a short while
    wl_display_set_default_max_buffer_size(m_sWLDisplay, 1_MB);

    Aquamarine::SBackendOptions options{};
    options.logFunction = aqLog;

    std::vector<Aquamarine::SBackendImplementationOptions> implementations;
    Aquamarine::SBackendImplementationOptions              option;
    option.backendType        = Aquamarine::eBackendType::AQ_BACKEND_HEADLESS;
    option.backendRequestMode = Aquamarine::eBackendRequestMode::AQ_BACKEND_REQUEST_MANDATORY;
    implementations.emplace_back(option);
    option.backendType        = Aquamarine::eBackendType::AQ_BACKEND_DRM;
    option.backendRequestMode = Aquamarine::eBackendRequestMode::AQ_BACKEND_REQUEST_IF_AVAILABLE;
    implementations.emplace_back(option);
    option.backendType        = Aquamarine::eBackendType::AQ_BACKEND_WAYLAND;
    option.backendRequestMode = Aquamarine::eBackendRequestMode::AQ_BACKEND_REQUEST_FALLBACK;
    implementations.emplace_back(option);

    m_pAqBackend = CBackend::create(implementations, options);

    if (!m_pAqBackend) {
        Debug::log(CRIT,
                   "m_pAqBackend was null! This usually means aquamarine could not find a GPU or encountered some issues. Make sure you're running either on a tty or on a Wayland "
                   "session, NOT an X11 one.");
        throwError("CBackend::create() failed!");
    }

    // TODO: headless only

    initAllSignals();

    if (!m_pAqBackend->start()) {
        Debug::log(CRIT,
                   "m_pAqBackend couldn't start! This usually means aquamarine could not find a GPU or encountered some issues. Make sure you're running either on a tty or on a "
                   "Wayland session, NOT an X11 one.");
        throwError("CBackend::create() failed!");
    }

    m_bInitialized = true;

    m_iDRMFD = m_pAqBackend->drmFD();
    Debug::log(LOG, "Running on DRMFD: {}", m_iDRMFD);

    if (!socketName.empty() && socketFd != -1) {
        fcntl(socketFd, F_SETFD, FD_CLOEXEC);
        const auto RETVAL = wl_display_add_socket_fd(m_sWLDisplay, socketFd);
        if (RETVAL >= 0) {
            m_szWLDisplaySocket = socketName;
            Debug::log(LOG, "wl_display_add_socket_fd for {} succeeded with {}", socketName, RETVAL);
        } else
            Debug::log(WARN, "wl_display_add_socket_fd for {} returned {}: skipping", socketName, RETVAL);
    } else {
        // get socket, avoid using 0
        for (int candidate = 1; candidate <= 32; candidate++) {
            const auto CANDIDATESTR = ("wayland-" + std::to_string(candidate));
            const auto RETVAL       = wl_display_add_socket(m_sWLDisplay, CANDIDATESTR.c_str());
            if (RETVAL >= 0) {
                m_szWLDisplaySocket = CANDIDATESTR;
                Debug::log(LOG, "wl_display_add_socket for {} succeeded with {}", CANDIDATESTR, RETVAL);
                break;
            } else
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
        throwError("m_szWLDisplaySocket was null! (wl_display_add_socket and wl_display_add_socket_auto failed)");
    }

    setenv("WAYLAND_DISPLAY", m_szWLDisplaySocket.c_str(), 1);
    if (!getenv("XDG_CURRENT_DESKTOP")) {
        setenv("XDG_CURRENT_DESKTOP", "Hyprland", 1);
        m_bDesktopEnvSet = true;
    }

    initManagers(STAGE_BASICINIT);

    initManagers(STAGE_LATE);

    for (auto const& o : pendingOutputs) {
        onNewMonitor(o);
    }
    pendingOutputs.clear();
}

void CCompositor::initAllSignals() {
    m_pAqBackend->events.newOutput.registerStaticListener(
        [this](void* p, std::any data) {
            auto output = std::any_cast<SP<Aquamarine::IOutput>>(data);
            Debug::log(LOG, "New aquamarine output with name {}", output->name);
            if (m_bInitialized)
                onNewMonitor(output);
            else
                pendingOutputs.emplace_back(output);
        },
        nullptr);

    m_pAqBackend->events.newPointer.registerStaticListener(
        [](void* data, std::any d) {
            auto dev = std::any_cast<SP<Aquamarine::IPointer>>(d);
            Debug::log(LOG, "New aquamarine pointer with name {}", dev->getName());
            g_pInputManager->newMouse(dev);
            g_pInputManager->updateCapabilities();
        },
        nullptr);

    m_pAqBackend->events.newKeyboard.registerStaticListener(
        [](void* data, std::any d) {
            auto dev = std::any_cast<SP<Aquamarine::IKeyboard>>(d);
            Debug::log(LOG, "New aquamarine keyboard with name {}", dev->getName());
            g_pInputManager->newKeyboard(dev);
            g_pInputManager->updateCapabilities();
        },
        nullptr);

    m_pAqBackend->events.newTouch.registerStaticListener(
        [](void* data, std::any d) {
            auto dev = std::any_cast<SP<Aquamarine::ITouch>>(d);
            Debug::log(LOG, "New aquamarine touch with name {}", dev->getName());
            g_pInputManager->newTouchDevice(dev);
            g_pInputManager->updateCapabilities();
        },
        nullptr);

    m_pAqBackend->events.newSwitch.registerStaticListener(
        [](void* data, std::any d) {
            auto dev = std::any_cast<SP<Aquamarine::ISwitch>>(d);
            Debug::log(LOG, "New aquamarine switch with name {}", dev->getName());
            g_pInputManager->newSwitch(dev);
        },
        nullptr);

    m_pAqBackend->events.newTablet.registerStaticListener(
        [](void* data, std::any d) {
            auto dev = std::any_cast<SP<Aquamarine::ITablet>>(d);
            Debug::log(LOG, "New aquamarine tablet with name {}", dev->getName());
            g_pInputManager->newTablet(dev);
        },
        nullptr);

    m_pAqBackend->events.newTabletPad.registerStaticListener(
        [](void* data, std::any d) {
            auto dev = std::any_cast<SP<Aquamarine::ITabletPad>>(d);
            Debug::log(LOG, "New aquamarine tablet pad with name {}", dev->getName());
            g_pInputManager->newTabletPad(dev);
        },
        nullptr);

    if (m_pAqBackend->hasSession()) {
        m_pAqBackend->session->events.changeActive.registerStaticListener(
            [this](void*, std::any) {
                if (m_pAqBackend->session->active) {
                    Debug::log(LOG, "Session got activated!");

                    m_bSessionActive = true;

                    for (auto const& m : m_vMonitors) {
                        scheduleFrameForMonitor(m);
                        m->applyMonitorRule(&m->activeMonitorRule, true);
                    }

                    g_pConfigManager->m_bWantsMonitorReload = true;
                    g_pCursorManager->syncGsettings();
                } else {
                    Debug::log(LOG, "Session got deactivated!");

                    m_bSessionActive = false;
                }
            },
            nullptr);
    }
}

void CCompositor::removeAllSignals() {
    ;
}

void CCompositor::cleanEnvironment() {
    // in compositor constructor
    unsetenv("WAYLAND_DISPLAY");
    // in startCompositor
    unsetenv("HYPRLAND_INSTANCE_SIGNATURE");

    // in main
    unsetenv("HYPRLAND_CMD");
    unsetenv("XDG_BACKEND");
    if (m_bDesktopEnvSet)
        unsetenv("XDG_CURRENT_DESKTOP");

    if (m_pAqBackend->hasSession() && !envEnabled("HYPRLAND_NO_SD_VARS")) {
        const auto CMD =
#ifdef USES_SYSTEMD
            "systemctl --user unset-environment DISPLAY WAYLAND_DISPLAY HYPRLAND_INSTANCE_SIGNATURE XDG_CURRENT_DESKTOP QT_QPA_PLATFORMTHEME PATH XDG_DATA_DIRS && hash "
            "dbus-update-activation-environment 2>/dev/null && "
#endif
            "dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP HYPRLAND_INSTANCE_SIGNATURE QT_QPA_PLATFORMTHEME PATH XDG_DATA_DIRS";
        CKeybindManager::spawn(CMD);
    }
}

void CCompositor::stopCompositor() {
    Debug::log(LOG, "Hyprland is stopping!");

    // this stops the wayland loop, wl_display_run
    wl_display_terminate(m_sWLDisplay);
    m_bIsShuttingDown = true;
}

void CCompositor::cleanup() {
    if (!m_sWLDisplay)
        return;

    signal(SIGABRT, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);

    removeLockFile();

    m_bIsShuttingDown   = true;
    Debug::shuttingDown = true;

#ifdef USES_SYSTEMD
    if (NSystemd::sdBooted() > 0 && !envEnabled("HYPRLAND_NO_SD_NOTIFY"))
        NSystemd::sdNotify(0, "STOPPING=1");
#endif

    cleanEnvironment();

    // unload all remaining plugins while the compositor is
    // still in a normal working state.
    g_pPluginSystem->unloadAllPlugins();

    m_pLastFocus.reset();
    m_pLastWindow.reset();

    m_vWorkspaces.clear();
    m_vWindows.clear();

    for (auto const& m : m_vMonitors) {
        g_pHyprOpenGL->destroyMonitorResources(m);

        m->output->state->setEnabled(false);
        m->state.commit();
    }

    g_pXWayland.reset();

    m_vMonitors.clear();

    wl_display_destroy_clients(g_pCompositor->m_sWLDisplay);
    removeAllSignals();

    g_pInputManager.reset();
    g_pDynamicPermissionManager.reset();
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
    g_pConfigManager.reset();
    g_pLayoutManager.reset();
    g_pHyprError.reset();
    g_pConfigManager.reset();
    g_pKeybindManager.reset();
    g_pHookSystem.reset();
    g_pXWaylandManager.reset();
    g_pPointerManager.reset();
    g_pSeatManager.reset();
    g_pHyprCtl.reset();
    g_pEventLoopManager.reset();
    g_pVersionKeeperMgr.reset();
    g_pDonationNagManager.reset();
    g_pANRManager.reset();
    g_pConfigWatcher.reset();

    if (m_pAqBackend)
        m_pAqBackend.reset();

    if (m_critSigSource)
        wl_event_source_remove(m_critSigSource);

    // this frees all wayland resources, including sockets
    wl_display_destroy(m_sWLDisplay);

    Debug::close();
}

void CCompositor::initManagers(eManagersInitStage stage) {
    switch (stage) {
        case STAGE_PRIORITY: {
            Debug::log(LOG, "Creating the EventLoopManager!");
            g_pEventLoopManager = makeUnique<CEventLoopManager>(m_sWLDisplay, m_sWLEventLoop);

            Debug::log(LOG, "Creating the HookSystem!");
            g_pHookSystem = makeUnique<CHookSystemManager>();

            Debug::log(LOG, "Creating the KeybindManager!");
            g_pKeybindManager = makeUnique<CKeybindManager>();

            Debug::log(LOG, "Creating the AnimationManager!");
            g_pAnimationManager = makeUnique<CHyprAnimationManager>();

            Debug::log(LOG, "Creating the DynamicPermissionManager!");
            g_pDynamicPermissionManager = makeUnique<CDynamicPermissionManager>();

            Debug::log(LOG, "Creating the ConfigManager!");
            g_pConfigManager = makeUnique<CConfigManager>();

            Debug::log(LOG, "Creating the CHyprError!");
            g_pHyprError = makeUnique<CHyprError>();

            Debug::log(LOG, "Creating the LayoutManager!");
            g_pLayoutManager = makeUnique<CLayoutManager>();

            Debug::log(LOG, "Creating the TokenManager!");
            g_pTokenManager = makeUnique<CTokenManager>();

            g_pConfigManager->init();

            Debug::log(LOG, "Creating the PointerManager!");
            g_pPointerManager = makeUnique<CPointerManager>();

            Debug::log(LOG, "Creating the EventManager!");
            g_pEventManager = makeUnique<CEventManager>();
        } break;
        case STAGE_BASICINIT: {
            Debug::log(LOG, "Creating the CHyprOpenGLImpl!");
            g_pHyprOpenGL = makeUnique<CHyprOpenGLImpl>();

            Debug::log(LOG, "Creating the ProtocolManager!");
            g_pProtocolManager = makeUnique<CProtocolManager>();

            Debug::log(LOG, "Creating the SeatManager!");
            g_pSeatManager = makeUnique<CSeatManager>();
        } break;
        case STAGE_LATE: {
            Debug::log(LOG, "Creating CHyprCtl");
            g_pHyprCtl = makeUnique<CHyprCtl>();

            Debug::log(LOG, "Creating the InputManager!");
            g_pInputManager = makeUnique<CInputManager>();

            Debug::log(LOG, "Creating the HyprRenderer!");
            g_pHyprRenderer = makeUnique<CHyprRenderer>();

            Debug::log(LOG, "Creating the XWaylandManager!");
            g_pXWaylandManager = makeUnique<CHyprXWaylandManager>();

            Debug::log(LOG, "Creating the SessionLockManager!");
            g_pSessionLockManager = makeUnique<CSessionLockManager>();

            Debug::log(LOG, "Creating the HyprDebugOverlay!");
            g_pDebugOverlay = makeUnique<CHyprDebugOverlay>();

            Debug::log(LOG, "Creating the HyprNotificationOverlay!");
            g_pHyprNotificationOverlay = makeUnique<CHyprNotificationOverlay>();

            Debug::log(LOG, "Creating the PluginSystem!");
            g_pPluginSystem = makeUnique<CPluginSystem>();
            g_pConfigManager->handlePluginLoads();

            Debug::log(LOG, "Creating the DecorationPositioner!");
            g_pDecorationPositioner = makeUnique<CDecorationPositioner>();

            Debug::log(LOG, "Creating the CursorManager!");
            g_pCursorManager = makeUnique<CCursorManager>();

            Debug::log(LOG, "Creating the VersionKeeper!");
            g_pVersionKeeperMgr = makeUnique<CVersionKeeperManager>();

            Debug::log(LOG, "Creating the DonationNag!");
            g_pDonationNagManager = makeUnique<CDonationNagManager>();

            Debug::log(LOG, "Creating the ANRManager!");
            g_pANRManager = makeUnique<CANRManager>();

            Debug::log(LOG, "Starting XWayland");
            g_pXWayland = makeUnique<CXWayland>(g_pCompositor->m_bWantsXwayland);
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
    SP<Aquamarine::IBackendImplementation> headless;
    for (auto const& impl : m_pAqBackend->getImplementations()) {
        if (impl->type() == Aquamarine::AQ_BACKEND_HEADLESS) {
            headless = impl;
            break;
        }
    }

    if (!headless) {
        Debug::log(WARN, "No headless in prepareFallbackOutput?!");
        return;
    }

    headless->createOutput();
}

void CCompositor::startCompositor() {
    signal(SIGPIPE, SIG_IGN);

    if (
        /* Session-less Hyprland usually means a nest, don't update the env in that case */
        m_pAqBackend->hasSession() &&
        /* Activation environment management is not disabled */
        !envEnabled("HYPRLAND_NO_SD_VARS")) {
        const auto CMD =
#ifdef USES_SYSTEMD
            "systemctl --user import-environment DISPLAY WAYLAND_DISPLAY HYPRLAND_INSTANCE_SIGNATURE XDG_CURRENT_DESKTOP QT_QPA_PLATFORMTHEME PATH XDG_DATA_DIRS && hash "
            "dbus-update-activation-environment 2>/dev/null && "
#endif
            "dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP HYPRLAND_INSTANCE_SIGNATURE QT_QPA_PLATFORMTHEME PATH XDG_DATA_DIRS";
        CKeybindManager::spawn(CMD);
    }

    Debug::log(LOG, "Running on WAYLAND_DISPLAY: {}", m_szWLDisplaySocket);

    prepareFallbackOutput();

    g_pHyprRenderer->setCursorFromName("left_ptr");

#ifdef USES_SYSTEMD
    if (NSystemd::sdBooted() > 0) {
        // tell systemd that we are ready so it can start other bond, following, related units
        if (!envEnabled("HYPRLAND_NO_SD_NOTIFY"))
            NSystemd::sdNotify(0, "READY=1");
    } else
        Debug::log(LOG, "systemd integration is baked in but system itself is not booted à la systemd!");
#endif

    createLockFile();

    EMIT_HOOK_EVENT("ready", nullptr);

    // This blocks until we are done.
    Debug::log(LOG, "Hyprland is ready, running the event loop!");
    g_pEventLoopManager->enterLoop();
}

PHLMONITOR CCompositor::getMonitorFromID(const MONITORID& id) {
    for (auto const& m : m_vMonitors) {
        if (m->ID == id) {
            return m;
        }
    }

    return nullptr;
}

PHLMONITOR CCompositor::getMonitorFromName(const std::string& name) {
    for (auto const& m : m_vMonitors) {
        if (m->szName == name) {
            return m;
        }
    }
    return nullptr;
}

PHLMONITOR CCompositor::getMonitorFromDesc(const std::string& desc) {
    for (auto const& m : m_vMonitors) {
        if (m->szDescription.starts_with(desc))
            return m;
    }
    return nullptr;
}

PHLMONITOR CCompositor::getMonitorFromCursor() {
    return getMonitorFromVector(g_pPointerManager->position());
}

PHLMONITOR CCompositor::getMonitorFromVector(const Vector2D& point) {
    if (m_vMonitors.empty()) {
        Debug::log(WARN, "getMonitorFromVector called with empty monitor list");
        return nullptr;
    }

    PHLMONITOR mon;
    for (auto const& m : m_vMonitors) {
        if (CBox{m->vecPosition, m->vecSize}.containsPoint(point)) {
            mon = m;
            break;
        }
    }

    if (!mon) {
        float      bestDistance = 0.f;
        PHLMONITOR pBestMon;

        for (auto const& m : m_vMonitors) {
            float dist = vecToRectDistanceSquared(point, m->vecPosition, m->vecPosition + m->vecSize);

            if (dist < bestDistance || !pBestMon) {
                bestDistance = dist;
                pBestMon     = m;
            }
        }

        if (!pBestMon) { // ?????
            Debug::log(WARN, "getMonitorFromVector no close mon???");
            return m_vMonitors.front();
        }

        return pBestMon;
    }

    return mon;
}

void CCompositor::removeWindowFromVectorSafe(PHLWINDOW pWindow) {
    if (!pWindow->m_bFadingOut) {
        EMIT_HOOK_EVENT("destroyWindow", pWindow);

        std::erase_if(m_vWindows, [&](SP<CWindow>& el) { return el == pWindow; });
        std::erase_if(m_vWindowsFadingOut, [&](PHLWINDOWREF el) { return el.lock() == pWindow; });
    }
}

bool CCompositor::monitorExists(PHLMONITOR pMonitor) {
    return std::ranges::any_of(m_vRealMonitors, [&](const PHLMONITOR& m) { return m == pMonitor; });
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
        for (auto const& w : m_vWindows | std::views::reverse) {
            if (w->m_bIsFloating && w->m_bIsMapped && !w->isHidden() && !w->m_bX11ShouldntFocus && w->m_bPinned && !w->m_sWindowData.noFocus.valueOrDefault() &&
                w != pIgnoreWindow) {
                const auto BB  = w->getWindowBoxUnified(properties);
                CBox       box = BB.copy().expand(!w->isX11OverrideRedirect() ? BORDER_GRAB_AREA : 0);
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
            for (auto const& w : m_vWindows | std::views::reverse) {

                if (special && !w->onSpecialWorkspace()) // because special floating may creep up into regular
                    continue;

                if (!w->m_pWorkspace)
                    continue;

                const auto PWINDOWMONITOR = w->m_pMonitor.lock();

                // to avoid focusing windows behind special workspaces from other monitors
                if (!*PSPECIALFALLTHRU && PWINDOWMONITOR && PWINDOWMONITOR->activeSpecialWorkspace && w->m_pWorkspace != PWINDOWMONITOR->activeSpecialWorkspace) {
                    const auto BB = w->getWindowBoxUnified(properties);
                    if (BB.x >= PWINDOWMONITOR->vecPosition.x && BB.y >= PWINDOWMONITOR->vecPosition.y &&
                        BB.x + BB.width <= PWINDOWMONITOR->vecPosition.x + PWINDOWMONITOR->vecSize.x &&
                        BB.y + BB.height <= PWINDOWMONITOR->vecPosition.y + PWINDOWMONITOR->vecSize.y)
                        continue;
                }

                if (w->m_bIsFloating && w->m_bIsMapped && w->m_pWorkspace->isVisible() && !w->isHidden() && !w->m_bPinned && !w->m_sWindowData.noFocus.valueOrDefault() &&
                    w != pIgnoreWindow && (!aboveFullscreen || w->m_bCreatedOverFullscreen)) {
                    // OR windows should add focus to parent
                    if (w->m_bX11ShouldntFocus && !w->isX11OverrideRedirect())
                        continue;

                    const auto BB  = w->getWindowBoxUnified(properties);
                    CBox       box = BB.copy().expand(!w->isX11OverrideRedirect() ? BORDER_GRAB_AREA : 0);
                    if (box.containsPoint(g_pPointerManager->position())) {

                        if (w->m_bIsX11 && w->isX11OverrideRedirect() && !w->m_pXWaylandSurface->wantsFocus()) {
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

        const WORKSPACEID WSPID      = special ? PMONITOR->activeSpecialWorkspaceID() : PMONITOR->activeWorkspaceID();
        const auto        PWORKSPACE = getWorkspaceByID(WSPID);

        if (PWORKSPACE->m_bHasFullscreenWindow && !(properties & SKIP_FULLSCREEN_PRIORITY))
            return PWORKSPACE->getFullscreenWindow();

        auto found = floating(false);
        if (found)
            return found;

        // for windows, we need to check their extensions too, first.
        for (auto const& w : m_vWindows) {
            if (special != w->onSpecialWorkspace())
                continue;

            if (!w->m_pWorkspace)
                continue;

            if (!w->m_bIsX11 && !w->m_bIsFloating && w->m_bIsMapped && w->workspaceID() == WSPID && !w->isHidden() && !w->m_bX11ShouldntFocus &&
                !w->m_sWindowData.noFocus.valueOrDefault() && w != pIgnoreWindow) {
                if (w->hasPopupAt(pos))
                    return w;
            }
        }

        for (auto const& w : m_vWindows) {
            if (special != w->onSpecialWorkspace())
                continue;

            if (!w->m_pWorkspace)
                continue;

            if (!w->m_bIsFloating && w->m_bIsMapped && w->workspaceID() == WSPID && !w->isHidden() && !w->m_bX11ShouldntFocus && !w->m_sWindowData.noFocus.valueOrDefault() &&
                w != pIgnoreWindow) {
                CBox box = (properties & USE_PROP_TILED) ? w->getWindowBoxUnified(properties) : CBox{w->m_vPosition, w->m_vSize};
                if (box.containsPoint(pos))
                    return w;
            }
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
        sl             = pos - pWindow->m_vRealPosition->goal() - OFF;
        return PPOPUP->m_pWLSurface->resource();
    }

    auto [surf, local] = pWindow->m_pWLSurface->resource()->at(pos - pWindow->m_vRealPosition->goal(), true);
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
        return vec - pWindow->m_vRealPosition->goal();

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
        return vec - pWindow->m_vRealPosition->goal();

    return vec - pWindow->m_vRealPosition->goal() - std::get<1>(iterData) + Vector2D{geom.x, geom.y};
}

PHLMONITOR CCompositor::getMonitorFromOutput(SP<Aquamarine::IOutput> out) {
    for (auto const& m : m_vMonitors) {
        if (m->output == out) {
            return m;
        }
    }

    return nullptr;
}

PHLMONITOR CCompositor::getRealMonitorFromOutput(SP<Aquamarine::IOutput> out) {
    for (auto const& m : m_vRealMonitors) {
        if (m->output == out) {
            return m;
        }
    }

    return nullptr;
}

void CCompositor::focusWindow(PHLWINDOW pWindow, SP<CWLSurfaceResource> pSurface, bool preserveFocusHistory) {

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

    if (pWindow && pWindow->m_bIsX11 && pWindow->isX11OverrideRedirect() && !pWindow->m_pXWaylandSurface->wantsFocus())
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

    if (pWindow->m_sWindowData.noFocus.valueOrDefault()) {
        Debug::log(LOG, "Ignoring focus to nofocus window!");
        return;
    }

    if (m_pLastWindow.lock() == pWindow && g_pSeatManager->state.keyboardFocus == pSurface && g_pSeatManager->state.keyboardFocus)
        return;

    if (pWindow->m_bPinned)
        pWindow->m_pWorkspace = m_pLastMonitor->activeWorkspace;

    const auto PMONITOR = pWindow->m_pMonitor.lock();

    if (!pWindow->m_pWorkspace || !pWindow->m_pWorkspace->isVisible()) {
        const auto PWORKSPACE = pWindow->m_pWorkspace;
        // This is to fix incorrect feedback on the focus history.
        PWORKSPACE->m_pLastFocusedWindow = pWindow;
        if (m_pLastMonitor->activeWorkspace)
            PWORKSPACE->rememberPrevWorkspace(m_pLastMonitor->activeWorkspace);
        if (PWORKSPACE->m_bIsSpecialWorkspace)
            m_pLastMonitor->changeWorkspace(PWORKSPACE, false, true); // if special ws, open on current monitor
        else if (PMONITOR)
            PMONITOR->changeWorkspace(PWORKSPACE, false, true);
        // changeworkspace already calls focusWindow
        return;
    }

    const auto PLASTWINDOW = m_pLastWindow.lock();
    m_pLastWindow          = pWindow;

    /* If special fallthrough is enabled, this behavior will be disabled, as I have no better idea of nicely tracking which
       window focuses are "via keybinds" and which ones aren't. */
    if (PMONITOR && PMONITOR->activeSpecialWorkspace && PMONITOR->activeSpecialWorkspace != pWindow->m_pWorkspace && !pWindow->m_bPinned && !*PSPECIALFALLTHROUGH)
        PMONITOR->setSpecialWorkspace(nullptr);

    // we need to make the PLASTWINDOW not equal to m_pLastWindow so that RENDERDATA is correct for an unfocused window
    if (PLASTWINDOW && PLASTWINDOW->m_bIsMapped) {
        PLASTWINDOW->updateDynamicRules();

        updateWindowAnimatedDecorationValues(PLASTWINDOW);

        if (!pWindow->m_bIsX11 || !pWindow->isX11OverrideRedirect())
            g_pXWaylandManager->activateWindow(PLASTWINDOW, false);
    }

    m_pLastWindow = PLASTWINDOW;

    const auto PWINDOWSURFACE = pSurface ? pSurface : pWindow->m_pWLSurface->resource();

    focusSurface(PWINDOWSURFACE, pWindow);

    g_pXWaylandManager->activateWindow(pWindow, true); // sets the m_pLastWindow

    pWindow->updateDynamicRules();
    pWindow->onFocusAnimUpdate();

    updateWindowAnimatedDecorationValues(pWindow);

    if (pWindow->m_bIsUrgent)
        pWindow->m_bIsUrgent = false;

    // Send an event
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "activewindow", .data = pWindow->m_szClass + "," + pWindow->m_szTitle});
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "activewindowv2", .data = std::format("{:x}", (uintptr_t)pWindow.get())});

    EMIT_HOOK_EVENT("activeWindow", pWindow);

    g_pLayoutManager->getCurrentLayout()->onWindowFocusChange(pWindow);

    g_pInputManager->recheckIdleInhibitorStatus();

    if (!preserveFocusHistory) {
        // move to front of the window history
        const auto HISTORYPIVOT = std::ranges::find_if(m_vWindowFocusHistory, [&](const auto& other) { return other.lock() == pWindow; });
        if (HISTORYPIVOT == m_vWindowFocusHistory.end())
            Debug::log(ERR, "BUG THIS: {} has no pivot in history", pWindow);
        else
            std::rotate(m_vWindowFocusHistory.begin(), HISTORYPIVOT, HISTORYPIVOT + 1);
    }

    if (*PFOLLOWMOUSE == 0)
        g_pInputManager->sendMotionEventsToFocused();

    if (pWindow->m_sGroupData.pNextWindow)
        pWindow->deactivateGroupMembers();
}

void CCompositor::focusSurface(SP<CWLSurfaceResource> pSurface, PHLWINDOW pWindowOwner) {

    if (g_pSeatManager->state.keyboardFocus == pSurface || (pWindowOwner && g_pSeatManager->state.keyboardFocus == pWindowOwner->m_pWLSurface->resource()))
        return; // Don't focus when already focused on this.

    if (g_pSessionLockManager->isSessionLocked() && pSurface && !g_pSessionLockManager->isSurfaceSessionLock(pSurface))
        return;

    if (g_pSeatManager->seatGrab && !g_pSeatManager->seatGrab->accepts(pSurface)) {
        Debug::log(LOG, "surface {:x} won't receive kb focus becuase grab rejected it", (uintptr_t)pSurface.get());
        return;
    }

    const auto PLASTSURF = m_pLastFocus.lock();

    // Unfocus last surface if should
    if (m_pLastFocus && !pWindowOwner)
        g_pXWaylandManager->activateSurface(m_pLastFocus.lock(), false);

    if (!pSurface) {
        g_pSeatManager->setKeyboardFocus(nullptr);
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "activewindow", .data = ","});
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "activewindowv2", .data = ""});
        EMIT_HOOK_EVENT("keyboardFocus", (SP<CWLSurfaceResource>)nullptr);
        m_pLastFocus.reset();
        return;
    }

    if (g_pSeatManager->keyboard)
        g_pSeatManager->setKeyboardFocus(pSurface);

    if (pWindowOwner)
        Debug::log(LOG, "Set keyboard focus to surface {:x}, with {}", (uintptr_t)pSurface.get(), pWindowOwner);
    else
        Debug::log(LOG, "Set keyboard focus to surface {:x}", (uintptr_t)pSurface.get());

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

SP<CWLSurfaceResource> CCompositor::vectorToLayerPopupSurface(const Vector2D& pos, PHLMONITOR monitor, Vector2D* sCoords, PHLLS* ppLayerSurfaceFound) {
    for (auto const& lsl : monitor->m_aLayerSurfaceLayers | std::views::reverse) {
        for (auto const& ls : lsl | std::views::reverse) {
            if (!ls->mapped || ls->fadingOut || !ls->layerSurface || (ls->layerSurface && !ls->layerSurface->mapped) || ls->alpha->value() == 0.f)
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

SP<CWLSurfaceResource> CCompositor::vectorToLayerSurface(const Vector2D& pos, std::vector<PHLLSREF>* layerSurfaces, Vector2D* sCoords, PHLLS* ppLayerSurfaceFound,
                                                         bool aboveLockscreen) {

    for (auto const& ls : *layerSurfaces | std::views::reverse) {
        if (!ls->mapped || ls->fadingOut || !ls->layerSurface || (ls->layerSurface && !ls->layerSurface->surface->mapped) || ls->alpha->value() == 0.f ||
            (aboveLockscreen && (!ls->aboveLockscreen || !ls->aboveLockscreenInteractable)))
            continue;

        auto [surf, local] = ls->layerSurface->surface->at(pos - ls->geometry.pos(), true);

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
    if (!pSurface || !pSurface->hlSurface)
        return nullptr;

    return pSurface->hlSurface->getWindow();
}

PHLWINDOW CCompositor::getWindowFromHandle(uint32_t handle) {
    for (auto const& w : m_vWindows) {
        if ((uint32_t)(((uint64_t)w.get()) & 0xFFFFFFFF) == handle) {
            return w;
        }
    }

    return nullptr;
}

PHLWORKSPACE CCompositor::getWorkspaceByID(const WORKSPACEID& id) {
    for (auto const& w : m_vWorkspaces) {
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

PHLWINDOW CCompositor::getUrgentWindow() {
    for (auto const& w : m_vWindows) {
        if (w->m_bIsMapped && w->m_bIsUrgent)
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

    if (top)
        pWindow->m_bCreatedOverFullscreen = true;

    if (pWindow == (top ? m_vWindows.back() : m_vWindows.front()))
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
            g_pHyprRenderer->damageMonitor(pw->m_pMonitor.lock());
    };

    if (!pWindow->m_bIsX11)
        moveToZ(pWindow, top);
    else {
        // move X11 window stack

        std::vector<PHLWINDOW> toMove;

        auto                   x11Stack = [&](PHLWINDOW pw, bool top, auto&& x11Stack) -> void {
            if (top)
                toMove.emplace_back(pw);
            else
                toMove.insert(toMove.begin(), pw);

            for (auto const& w : m_vWindows) {
                if (w->m_bIsMapped && !w->isHidden() && w->m_bIsX11 && w->x11TransientFor() == pw && w != pw && std::ranges::find(toMove, w) == toMove.end()) {
                    x11Stack(w, top, x11Stack);
                }
            }
        };

        x11Stack(pWindow, top, x11Stack);
        for (const auto& it : toMove) {
            moveToZ(it, top);
        }
    }
}

void CCompositor::cleanupFadingOut(const MONITORID& monid) {
    for (auto const& ww : m_vWindowsFadingOut) {

        auto w = ww.lock();

        if (w->monitorID() != monid && w->m_pMonitor)
            continue;

        if (!w->m_bFadingOut || w->m_fAlpha->value() == 0.f) {

            w->m_bFadingOut = false;

            if (!w->m_bReadyToDelete)
                continue;

            removeWindowFromVectorSafe(w);

            w.reset();

            Debug::log(LOG, "Cleanup: destroyed a window");
            return;
        }
    }

    bool layersDirty = false;

    for (auto const& lsr : m_vSurfacesFadingOut) {

        auto ls = lsr.lock();

        if (!ls) {
            layersDirty = true;
            continue;
        }

        if (ls->monitorID() != monid && ls->monitor)
            continue;

        // mark blur for recalc
        if (ls->layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND || ls->layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM)
            g_pHyprOpenGL->markBlurDirtyForMonitor(getMonitorFromID(monid));

        if (ls->fadingOut && ls->readyToDelete && ls->isFadedOut()) {
            for (auto const& m : m_vMonitors) {
                for (auto& lsl : m->m_aLayerSurfaceLayers) {
                    if (!lsl.empty() && std::ranges::find_if(lsl, [&](auto& other) { return other == ls; }) != lsl.end()) {
                        std::erase_if(lsl, [&](auto& other) { return other == ls || !other; });
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

    if (layersDirty)
        std::erase_if(m_vSurfacesFadingOut, [](const auto& el) { return el.expired(); });
}

void CCompositor::addToFadingOutSafe(PHLLS pLS) {
    const auto FOUND = std::ranges::find_if(m_vSurfacesFadingOut, [&](auto& other) { return other.lock() == pLS; });

    if (FOUND != m_vSurfacesFadingOut.end())
        return; // if it's already added, don't add it.

    m_vSurfacesFadingOut.emplace_back(pLS);
}

void CCompositor::removeFromFadingOutSafe(PHLLS ls) {
    std::erase(m_vSurfacesFadingOut, ls);
}

void CCompositor::addToFadingOutSafe(PHLWINDOW pWindow) {
    const auto FOUND = std::ranges::find_if(m_vWindowsFadingOut, [&](PHLWINDOWREF& other) { return other.lock() == pWindow; });

    if (FOUND != m_vWindowsFadingOut.end())
        return; // if it's already added, don't add it.

    m_vWindowsFadingOut.emplace_back(pWindow);
}

PHLWINDOW CCompositor::getWindowInDirection(PHLWINDOW pWindow, char dir) {
    if (!isDirection(dir))
        return nullptr;

    const auto PMONITOR = pWindow->m_pMonitor.lock();

    if (!PMONITOR)
        return nullptr; // ??

    const auto WINDOWIDEALBB = pWindow->isFullscreen() ? CBox{PMONITOR->vecPosition, PMONITOR->vecSize} : pWindow->getWindowIdealBoundingBoxIgnoreReserved();
    const auto PWORKSPACE    = pWindow->m_pWorkspace;

    return getWindowInDirection(WINDOWIDEALBB, PWORKSPACE, dir, pWindow, pWindow->m_bIsFloating);
}

PHLWINDOW CCompositor::getWindowInDirection(const CBox& box, PHLWORKSPACE pWorkspace, char dir, PHLWINDOW ignoreWindow, bool useVectorAngles) {
    if (!isDirection(dir))
        return nullptr;

    // 0 -> history, 1 -> shared length
    static auto PMETHOD          = CConfigValue<Hyprlang::INT>("binds:focus_preferred_method");
    static auto PMONITORFALLBACK = CConfigValue<Hyprlang::INT>("binds:window_direction_monitor_fallback");

    const auto  POSA  = box.pos();
    const auto  SIZEA = box.size();

    auto        leaderValue  = -1;
    PHLWINDOW   leaderWindow = nullptr;

    if (!useVectorAngles) {
        for (auto const& w : m_vWindows) {
            if (w == ignoreWindow || !w->m_pWorkspace || !w->m_bIsMapped || w->isHidden() || (!w->isFullscreen() && w->m_bIsFloating) || !w->m_pWorkspace->isVisible())
                continue;

            if (pWorkspace->m_pMonitor == w->m_pMonitor && pWorkspace != w->m_pWorkspace)
                continue;

            if (pWorkspace->m_bHasFullscreenWindow && !w->isFullscreen() && !w->m_bCreatedOverFullscreen)
                continue;

            if (!*PMONITORFALLBACK && pWorkspace->m_pMonitor != w->m_pMonitor)
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
        if (dir == 'u')
            dir = 't';
        if (dir == 'd')
            dir = 'b';

        static const std::unordered_map<char, Vector2D> VECTORS = {{'r', {1, 0}}, {'t', {0, -1}}, {'b', {0, 1}}, {'l', {-1, 0}}};

        //
        auto vectorAngles = [](const Vector2D& a, const Vector2D& b) -> double {
            double dot = (a.x * b.x) + (a.y * b.y);
            double ang = std::acos(dot / (a.size() * b.size()));
            return ang;
        };

        float           bestAngleAbs = 2.0 * M_PI;
        constexpr float THRESHOLD    = 0.3 * M_PI;

        for (auto const& w : m_vWindows) {
            if (w == ignoreWindow || !w->m_bIsMapped || !w->m_pWorkspace || w->isHidden() || (!w->isFullscreen() && !w->m_bIsFloating) || !w->m_pWorkspace->isVisible())
                continue;

            if (pWorkspace->m_pMonitor == w->m_pMonitor && pWorkspace != w->m_pWorkspace)
                continue;

            if (pWorkspace->m_bHasFullscreenWindow && !w->isFullscreen() && !w->m_bCreatedOverFullscreen)
                continue;

            if (!*PMONITORFALLBACK && pWorkspace->m_pMonitor != w->m_pMonitor)
                continue;

            const auto DIST  = w->middle().distance(box.middle());
            const auto ANGLE = vectorAngles(Vector2D{w->middle() - box.middle()}, VECTORS.at(dir));

            if (ANGLE > M_PI_2)
                continue; // if the angle is over 90 degrees, ignore. Wrong direction entirely.

            if ((bestAngleAbs < THRESHOLD && DIST < leaderValue && ANGLE < THRESHOLD) || (ANGLE < bestAngleAbs && bestAngleAbs > THRESHOLD) || leaderValue == -1) {
                leaderValue  = DIST;
                bestAngleAbs = ANGLE;
                leaderWindow = w;
            }
        }

        if (!leaderWindow && pWorkspace->m_bHasFullscreenWindow)
            leaderWindow = pWorkspace->getFullscreenWindow();
    }

    if (leaderValue != -1)
        return leaderWindow;

    return nullptr;
}

template <typename WINDOWPTR>
static bool isWorkspaceMatches(WINDOWPTR pWindow, const WINDOWPTR w, bool anyWorkspace) {
    return anyWorkspace ? w->m_pWorkspace && w->m_pWorkspace->isVisible() : w->m_pWorkspace == pWindow->m_pWorkspace;
}

template <typename WINDOWPTR>
static bool isFloatingMatches(WINDOWPTR w, std::optional<bool> floating) {
    return !floating.has_value() || w->m_bIsFloating == floating.value();
}

template <typename WINDOWPTR>
static bool isWindowAvailableForCycle(WINDOWPTR pWindow, WINDOWPTR w, bool focusableOnly, std::optional<bool> floating, bool anyWorkspace = false) {
    return isFloatingMatches(w, floating) &&
        (w != pWindow && isWorkspaceMatches(pWindow, w, anyWorkspace) && w->m_bIsMapped && !w->isHidden() && (!focusableOnly || !w->m_sWindowData.noFocus.valueOrDefault()));
}

template <typename Iterator>
static PHLWINDOW getWindowPred(Iterator cur, Iterator end, Iterator begin, const std::function<bool(const PHLWINDOW&)> PRED) {
    const auto IN_ONE_SIDE = std::find_if(cur, end, PRED);
    if (IN_ONE_SIDE != end)
        return *IN_ONE_SIDE;
    const auto IN_OTHER_SIDE = std::find_if(begin, cur, PRED);
    return *IN_OTHER_SIDE;
}

template <typename Iterator>
static PHLWINDOW getWeakWindowPred(Iterator cur, Iterator end, Iterator begin, const std::function<bool(const PHLWINDOWREF&)> PRED) {
    const auto IN_ONE_SIDE = std::find_if(cur, end, PRED);
    if (IN_ONE_SIDE != end)
        return IN_ONE_SIDE->lock();
    const auto IN_OTHER_SIDE = std::find_if(begin, cur, PRED);
    return IN_OTHER_SIDE->lock();
}

PHLWINDOW CCompositor::getWindowCycleHist(PHLWINDOWREF cur, bool focusableOnly, std::optional<bool> floating, bool visible, bool next) {
    const auto FINDER = [&](const PHLWINDOWREF& w) { return isWindowAvailableForCycle(cur, w, focusableOnly, floating, visible); };
    // also m_vWindowFocusHistory has reverse order, so when it is next - we need to reverse again
    return next ?
        getWeakWindowPred(std::ranges::find(std::ranges::reverse_view(m_vWindowFocusHistory), cur), m_vWindowFocusHistory.rend(), m_vWindowFocusHistory.rbegin(), FINDER) :
        getWeakWindowPred(std::ranges::find(m_vWindowFocusHistory, cur), m_vWindowFocusHistory.end(), m_vWindowFocusHistory.begin(), FINDER);
}

PHLWINDOW CCompositor::getWindowCycle(PHLWINDOW cur, bool focusableOnly, std::optional<bool> floating, bool visible, bool prev) {
    const auto FINDER = [&](const PHLWINDOW& w) { return isWindowAvailableForCycle(cur, w, focusableOnly, floating, visible); };
    return prev ? getWindowPred(std::ranges::find(std::ranges::reverse_view(m_vWindows), cur), m_vWindows.rend(), m_vWindows.rbegin(), FINDER) :
                  getWindowPred(std::ranges::find(m_vWindows, cur), m_vWindows.end(), m_vWindows.begin(), FINDER);
}

WORKSPACEID CCompositor::getNextAvailableNamedWorkspace() {
    WORKSPACEID lowest = -1337 + 1;
    for (auto const& w : m_vWorkspaces) {
        if (w->m_iID < -1 && w->m_iID < lowest)
            lowest = w->m_iID;
    }

    return lowest - 1;
}

PHLWORKSPACE CCompositor::getWorkspaceByName(const std::string& name) {
    for (auto const& w : m_vWorkspaces) {
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
        return getWorkspaceByID(getWorkspaceIDNameFromString(str).id);
    } catch (std::exception& e) { Debug::log(ERR, "Error in getWorkspaceByString, invalid id"); }

    return nullptr;
}

bool CCompositor::isPointOnAnyMonitor(const Vector2D& point) {
    return std::ranges::any_of(
        m_vMonitors, [&](const PHLMONITOR& m) { return VECINRECT(point, m->vecPosition.x, m->vecPosition.y, m->vecSize.x + m->vecPosition.x, m->vecSize.y + m->vecPosition.y); });
}

bool CCompositor::isPointOnReservedArea(const Vector2D& point, const PHLMONITOR pMonitor) {
    const auto PMONITOR = pMonitor ? pMonitor : getMonitorFromVector(point);

    const auto XY1 = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
    const auto XY2 = PMONITOR->vecPosition + PMONITOR->vecSize - PMONITOR->vecReservedBottomRight;

    return VECNOTINRECT(point, XY1.x, XY1.y, XY2.x, XY2.y);
}

PHLMONITOR CCompositor::getMonitorInDirection(const char& dir) {
    return getMonitorInDirection(m_pLastMonitor.lock(), dir);
}

PHLMONITOR CCompositor::getMonitorInDirection(PHLMONITOR pSourceMonitor, const char& dir) {
    if (!pSourceMonitor)
        return nullptr;

    const auto POSA  = pSourceMonitor->vecPosition;
    const auto SIZEA = pSourceMonitor->vecSize;

    auto       longestIntersect        = -1;
    PHLMONITOR longestIntersectMonitor = nullptr;

    for (auto const& m : m_vMonitors) {
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
                        longestIntersectMonitor = m;
                    }
                }
                break;
            case 'r':
                if (STICKS(POSA.x + SIZEA.x, POSB.x)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect        = INTERSECTLEN;
                        longestIntersectMonitor = m;
                    }
                }
                break;
            case 't':
            case 'u':
                if (STICKS(POSA.y, POSB.y + SIZEB.y)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect        = INTERSECTLEN;
                        longestIntersectMonitor = m;
                    }
                }
                break;
            case 'b':
            case 'd':
                if (STICKS(POSA.y + SIZEA.y, POSB.y)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect        = INTERSECTLEN;
                        longestIntersectMonitor = m;
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
    for (auto const& w : m_vWindows) {
        if (!w->m_bIsMapped)
            continue;

        updateWindowAnimatedDecorationValues(w);
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
    static auto PSHADOWCOL              = CConfigValue<Hyprlang::INT>("decoration:shadow:color");
    static auto PSHADOWCOLINACTIVE      = CConfigValue<Hyprlang::INT>("decoration:shadow:color_inactive");
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
        pWindow->m_fBorderFadeAnimationProgress->setValueAndWarp(0.f);
        *pWindow->m_fBorderFadeAnimationProgress = 1.f;
    };

    const bool IS_SHADOWED_BY_MODAL = pWindow->m_pXDGSurface && pWindow->m_pXDGSurface->toplevel && pWindow->m_pXDGSurface->toplevel->anyChildModal();

    // border
    const auto RENDERDATA = g_pLayoutManager->getCurrentLayout()->requestRenderHints(pWindow);
    if (RENDERDATA.isBorderGradient)
        setBorderColor(*RENDERDATA.borderGradient);
    else {
        const bool GROUPLOCKED = pWindow->m_sGroupData.pNextWindow.lock() ? pWindow->getGroupHead()->m_sGroupData.locked : false;
        if (pWindow == m_pLastWindow) {
            const auto* const ACTIVECOLOR =
                !pWindow->m_sGroupData.pNextWindow.lock() ? (!pWindow->m_sGroupData.deny ? ACTIVECOL : NOGROUPACTIVECOL) : (GROUPLOCKED ? GROUPACTIVELOCKEDCOL : GROUPACTIVECOL);
            setBorderColor(pWindow->m_sWindowData.activeBorderColor.valueOr(*ACTIVECOLOR));
        } else {
            const auto* const INACTIVECOLOR = !pWindow->m_sGroupData.pNextWindow.lock() ? (!pWindow->m_sGroupData.deny ? INACTIVECOL : NOGROUPINACTIVECOL) :
                                                                                          (GROUPLOCKED ? GROUPINACTIVELOCKEDCOL : GROUPINACTIVECOL);
            setBorderColor(pWindow->m_sWindowData.inactiveBorderColor.valueOr(*INACTIVECOLOR));
        }
    }

    // opacity
    const auto PWORKSPACE = pWindow->m_pWorkspace;
    if (pWindow->isEffectiveInternalFSMode(FSMODE_FULLSCREEN)) {
        *pWindow->m_fActiveInactiveAlpha = pWindow->m_sWindowData.alphaFullscreen.valueOrDefault().applyAlpha(*PFULLSCREENALPHA);
    } else {
        if (pWindow == m_pLastWindow)
            *pWindow->m_fActiveInactiveAlpha = pWindow->m_sWindowData.alpha.valueOrDefault().applyAlpha(*PACTIVEALPHA);
        else
            *pWindow->m_fActiveInactiveAlpha = pWindow->m_sWindowData.alphaInactive.valueOrDefault().applyAlpha(*PINACTIVEALPHA);
    }

    // dim
    float goalDim = 1.F;
    if (pWindow == m_pLastWindow.lock() || pWindow->m_sWindowData.noDim.valueOrDefault() || !*PDIMENABLED)
        goalDim = 0;
    else
        goalDim = *PDIMSTRENGTH;

    if (IS_SHADOWED_BY_MODAL)
        goalDim += (1.F - goalDim) / 2.F;

    *pWindow->m_fDimPercent = goalDim;

    // shadow
    if (!pWindow->isX11OverrideRedirect() && !pWindow->m_bX11DoesntWantBorders) {
        if (pWindow == m_pLastWindow)
            *pWindow->m_cRealShadowColor = CHyprColor(*PSHADOWCOL);
        else
            *pWindow->m_cRealShadowColor = CHyprColor(*PSHADOWCOLINACTIVE != INT64_MAX ? *PSHADOWCOLINACTIVE : *PSHADOWCOL);
    } else {
        pWindow->m_cRealShadowColor->setValueAndWarp(CHyprColor(0, 0, 0, 0)); // no shadow
    }

    pWindow->updateWindowDecos();
}

MONITORID CCompositor::getNextAvailableMonitorID(std::string const& name) {
    // reuse ID if it's already in the map, and the monitor with that ID is not being used by another monitor
    if (m_mMonitorIDMap.contains(name) && !std::ranges::any_of(m_vRealMonitors, [&](auto m) { return m->ID == m_mMonitorIDMap[name]; }))
        return m_mMonitorIDMap[name];

    // otherwise, find minimum available ID that is not in the map
    std::unordered_set<MONITORID> usedIDs;
    for (auto const& monitor : m_vRealMonitors) {
        usedIDs.insert(monitor->ID);
    }

    MONITORID nextID = 0;
    while (usedIDs.contains(nextID)) {
        nextID++;
    }
    m_mMonitorIDMap[name] = nextID;
    return nextID;
}

void CCompositor::swapActiveWorkspaces(PHLMONITOR pMonitorA, PHLMONITOR pMonitorB) {
    const auto PWORKSPACEA = pMonitorA->activeWorkspace;
    const auto PWORKSPACEB = pMonitorB->activeWorkspace;

    PWORKSPACEA->m_pMonitor = pMonitorB;
    PWORKSPACEA->moveToMonitor(pMonitorB->ID);

    for (auto const& w : m_vWindows) {
        if (w->m_pWorkspace == PWORKSPACEA) {
            if (w->m_bPinned) {
                w->m_pWorkspace = PWORKSPACEB;
                continue;
            }

            w->m_pMonitor = pMonitorB;

            // additionally, move floating and fs windows manually
            if (w->m_bIsFloating)
                *w->m_vRealPosition = w->m_vRealPosition->goal() - pMonitorA->vecPosition + pMonitorB->vecPosition;

            if (w->isFullscreen()) {
                *w->m_vRealPosition = pMonitorB->vecPosition;
                *w->m_vRealSize     = pMonitorB->vecSize;
            }

            w->updateToplevel();
        }
    }

    PWORKSPACEB->m_pMonitor = pMonitorA;
    PWORKSPACEB->moveToMonitor(pMonitorA->ID);

    for (auto const& w : m_vWindows) {
        if (w->m_pWorkspace == PWORKSPACEB) {
            if (w->m_bPinned) {
                w->m_pWorkspace = PWORKSPACEA;
                continue;
            }

            w->m_pMonitor = pMonitorA;

            // additionally, move floating and fs windows manually
            if (w->m_bIsFloating)
                *w->m_vRealPosition = w->m_vRealPosition->goal() - pMonitorB->vecPosition + pMonitorA->vecPosition;

            if (w->isFullscreen()) {
                *w->m_vRealPosition = pMonitorA->vecPosition;
                *w->m_vRealSize     = pMonitorA->vecSize;
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
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "workspace", .data = PNEWWORKSPACE->m_szName});
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "workspacev2", .data = std::format("{},{}", PNEWWORKSPACE->m_iID, PNEWWORKSPACE->m_szName)});
        EMIT_HOOK_EVENT("workspace", PNEWWORKSPACE);
    }

    // event
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspace", .data = PWORKSPACEA->m_szName + "," + pMonitorB->szName});
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspacev2", .data = std::format("{},{},{}", PWORKSPACEA->m_iID, PWORKSPACEA->m_szName, pMonitorB->szName)});
    EMIT_HOOK_EVENT("moveWorkspace", (std::vector<std::any>{PWORKSPACEA, pMonitorB}));
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspace", .data = PWORKSPACEB->m_szName + "," + pMonitorA->szName});
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspacev2", .data = std::format("{},{},{}", PWORKSPACEB->m_iID, PWORKSPACEB->m_szName, pMonitorA->szName)});

    EMIT_HOOK_EVENT("moveWorkspace", (std::vector<std::any>{PWORKSPACEB, pMonitorA}));
}

PHLMONITOR CCompositor::getMonitorFromString(const std::string& name) {
    if (name == "current")
        return g_pCompositor->m_pLastMonitor.lock();
    else if (isDirection(name))
        return getMonitorInDirection(name[0]);
    else if (name[0] == '+' || name[0] == '-') {
        // relative

        if (m_vMonitors.size() == 1)
            return *m_vMonitors.begin();

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

        return m_vMonitors[currentPlace];
    } else if (isNumber(name)) {
        // change by ID
        MONITORID monID = MONITOR_INVALID;
        try {
            monID = std::stoi(name);
        } catch (std::exception& e) {
            // shouldn't happen but jic
            Debug::log(ERR, "Error in getMonitorFromString: invalid num");
            return nullptr;
        }

        if (monID > -1 && monID < (MONITORID)m_vMonitors.size()) {
            return getMonitorFromID(monID);
        } else {
            Debug::log(ERR, "Error in getMonitorFromString: invalid arg 1");
            return nullptr;
        }
    } else {
        for (auto const& m : m_vMonitors) {
            if (!m->output)
                continue;

            if (m->matchesStaticSelector(name)) {
                return m;
            }
        }
    }

    return nullptr;
}

void CCompositor::moveWorkspaceToMonitor(PHLWORKSPACE pWorkspace, PHLMONITOR pMonitor, bool noWarpCursor) {
    static auto PHIDESPECIALONWORKSPACECHANGE = CConfigValue<Hyprlang::INT>("binds:hide_special_on_workspace_change");

    if (!pWorkspace || !pMonitor)
        return;

    if (pWorkspace->m_pMonitor == pMonitor)
        return;

    Debug::log(LOG, "moveWorkspaceToMonitor: Moving {} to monitor {}", pWorkspace->m_iID, pMonitor->ID);

    const auto POLDMON = pWorkspace->m_pMonitor.lock();

    const bool SWITCHINGISACTIVE = POLDMON ? POLDMON->activeWorkspace == pWorkspace : false;

    // fix old mon
    WORKSPACEID nextWorkspaceOnMonitorID = WORKSPACE_INVALID;
    if (!SWITCHINGISACTIVE)
        nextWorkspaceOnMonitorID = pWorkspace->m_iID;
    else {
        for (auto const& w : m_vWorkspaces) {
            if (w->m_pMonitor == POLDMON && w->m_iID != pWorkspace->m_iID && !w->m_bIsSpecialWorkspace) {
                nextWorkspaceOnMonitorID = w->m_iID;
                break;
            }
        }

        if (nextWorkspaceOnMonitorID == WORKSPACE_INVALID) {
            nextWorkspaceOnMonitorID = 1;

            while (getWorkspaceByID(nextWorkspaceOnMonitorID) || [&]() -> bool {
                const auto B = g_pConfigManager->getBoundMonitorForWS(std::to_string(nextWorkspaceOnMonitorID));
                return B && B != POLDMON;
            }())
                nextWorkspaceOnMonitorID++;

            Debug::log(LOG, "moveWorkspaceToMonitor: Plugging gap with new {}", nextWorkspaceOnMonitorID);

            if (POLDMON)
                g_pCompositor->createNewWorkspace(nextWorkspaceOnMonitorID, POLDMON->ID);
        }

        Debug::log(LOG, "moveWorkspaceToMonitor: Plugging gap with existing {}", nextWorkspaceOnMonitorID);
        if (POLDMON)
            POLDMON->changeWorkspace(nextWorkspaceOnMonitorID, false, true, true);
    }

    // move the workspace
    pWorkspace->m_pMonitor = pMonitor;
    pWorkspace->moveToMonitor(pMonitor->ID);

    for (auto const& w : m_vWindows) {
        if (w->m_pWorkspace == pWorkspace) {
            if (w->m_bPinned) {
                w->m_pWorkspace = g_pCompositor->getWorkspaceByID(nextWorkspaceOnMonitorID);
                continue;
            }

            w->m_pMonitor = pMonitor;

            // additionally, move floating and fs windows manually
            if (w->m_bIsMapped && !w->isHidden()) {
                if (POLDMON) {
                    if (w->m_bIsFloating)
                        *w->m_vRealPosition = w->m_vRealPosition->goal() - POLDMON->vecPosition + pMonitor->vecPosition;

                    if (w->isFullscreen()) {
                        *w->m_vRealPosition = pMonitor->vecPosition;
                        *w->m_vRealSize     = pMonitor->vecSize;
                    }
                } else
                    *w->m_vRealPosition = Vector2D{
                        (pMonitor->vecSize.x != 0) ? (int)w->m_vRealPosition->goal().x % (int)pMonitor->vecSize.x : 0,
                        (pMonitor->vecSize.y != 0) ? (int)w->m_vRealPosition->goal().y % (int)pMonitor->vecSize.y : 0,
                    };
            }

            w->updateToplevel();
        }
    }

    if (SWITCHINGISACTIVE && POLDMON == g_pCompositor->m_pLastMonitor) { // if it was active, preserve its' status. If it wasn't, don't.
        Debug::log(LOG, "moveWorkspaceToMonitor: SWITCHINGISACTIVE, active {} -> {}", pMonitor->activeWorkspaceID(), pWorkspace->m_iID);

        if (valid(pMonitor->activeWorkspace)) {
            pMonitor->activeWorkspace->m_bVisible = false;
            pMonitor->activeWorkspace->startAnim(false, false);
        }

        if (*PHIDESPECIALONWORKSPACECHANGE)
            pMonitor->setSpecialWorkspace(nullptr);

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
        if (valid(POLDMON->activeWorkspace))
            updateFullscreenFadeOnWorkspace(POLDMON->activeWorkspace);
        updateSuspendedStates();
    }

    updateFullscreenFadeOnWorkspace(pWorkspace);
    updateSuspendedStates();

    // event
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspace", .data = pWorkspace->m_szName + "," + pMonitor->szName});
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspacev2", .data = std::format("{},{},{}", pWorkspace->m_iID, pWorkspace->m_szName, pMonitor->szName)});
    EMIT_HOOK_EVENT("moveWorkspace", (std::vector<std::any>{pWorkspace, pMonitor}));
}

bool CCompositor::workspaceIDOutOfBounds(const WORKSPACEID& id) {
    WORKSPACEID lowestID  = INT64_MAX;
    WORKSPACEID highestID = INT64_MIN;

    for (auto const& w : m_vWorkspaces) {
        if (w->m_bIsSpecialWorkspace)
            continue;
        lowestID  = std::min(w->m_iID, lowestID);
        highestID = std::max(w->m_iID, highestID);
    }

    return std::clamp(id, lowestID, highestID) != id;
}

void CCompositor::updateFullscreenFadeOnWorkspace(PHLWORKSPACE pWorkspace) {

    if (!pWorkspace)
        return;

    const auto FULLSCREEN = pWorkspace->m_bHasFullscreenWindow;

    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->m_pWorkspace == pWorkspace) {

            if (w->m_bFadingOut || w->m_bPinned || w->isFullscreen())
                continue;

            if (!FULLSCREEN)
                *w->m_fAlpha = 1.f;
            else if (!w->isFullscreen())
                *w->m_fAlpha = !w->m_bCreatedOverFullscreen ? 0.f : 1.f;
        }
    }

    const auto PMONITOR = pWorkspace->m_pMonitor.lock();

    if (pWorkspace->m_iID == PMONITOR->activeWorkspaceID() || pWorkspace->m_iID == PMONITOR->activeSpecialWorkspaceID()) {
        for (auto const& ls : PMONITOR->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
            if (!ls->fadingOut)
                *ls->alpha = FULLSCREEN && pWorkspace->m_efFullscreenMode == FSMODE_FULLSCREEN ? 0.f : 1.f;
        }
    }
}

void CCompositor::changeWindowFullscreenModeClient(const PHLWINDOW PWINDOW, const eFullscreenMode MODE, const bool ON) {
    setWindowFullscreenClient(PWINDOW,
                              (eFullscreenMode)(ON ? (uint8_t)PWINDOW->m_sFullscreenState.client | (uint8_t)MODE : ((uint8_t)PWINDOW->m_sFullscreenState.client & (uint8_t)~MODE)));
}

void CCompositor::setWindowFullscreenInternal(const PHLWINDOW PWINDOW, const eFullscreenMode MODE) {
    if (PWINDOW->m_sWindowData.syncFullscreen.valueOrDefault())
        setWindowFullscreenState(PWINDOW, SFullscreenState{.internal = MODE, .client = MODE});
    else
        setWindowFullscreenState(PWINDOW, SFullscreenState{.internal = MODE, .client = PWINDOW->m_sFullscreenState.client});
}

void CCompositor::setWindowFullscreenClient(const PHLWINDOW PWINDOW, const eFullscreenMode MODE) {
    if (PWINDOW->m_sWindowData.syncFullscreen.valueOrDefault())
        setWindowFullscreenState(PWINDOW, SFullscreenState{.internal = MODE, .client = MODE});
    else
        setWindowFullscreenState(PWINDOW, SFullscreenState{.internal = PWINDOW->m_sFullscreenState.internal, .client = MODE});
}

void CCompositor::setWindowFullscreenState(const PHLWINDOW PWINDOW, SFullscreenState state) {
    static auto PDIRECTSCANOUT      = CConfigValue<Hyprlang::INT>("render:direct_scanout");
    static auto PALLOWPINFULLSCREEN = CConfigValue<Hyprlang::INT>("binds:allow_pin_fullscreen");

    if (!validMapped(PWINDOW) || g_pCompositor->m_bUnsafeState)
        return;

    state.internal = std::clamp(state.internal, (eFullscreenMode)0, FSMODE_MAX);
    state.client   = std::clamp(state.client, (eFullscreenMode)0, FSMODE_MAX);

    const auto            PMONITOR   = PWINDOW->m_pMonitor.lock();
    const auto            PWORKSPACE = PWINDOW->m_pWorkspace;

    const eFullscreenMode CURRENT_EFFECTIVE_MODE = (eFullscreenMode)std::bit_floor((uint8_t)PWINDOW->m_sFullscreenState.internal);
    const eFullscreenMode EFFECTIVE_MODE         = (eFullscreenMode)std::bit_floor((uint8_t)state.internal);

    if (PWINDOW->m_bIsFloating && CURRENT_EFFECTIVE_MODE == FSMODE_NONE && EFFECTIVE_MODE != FSMODE_NONE)
        g_pHyprRenderer->damageWindow(PWINDOW);

    if (*PALLOWPINFULLSCREEN && !PWINDOW->m_bPinFullscreened && !PWINDOW->isFullscreen() && PWINDOW->m_bPinned) {
        PWINDOW->m_bPinned          = false;
        PWINDOW->m_bPinFullscreened = true;
    }

    if (PWORKSPACE->m_bHasFullscreenWindow && !PWINDOW->isFullscreen())
        setWindowFullscreenInternal(PWORKSPACE->getFullscreenWindow(), FSMODE_NONE);

    const bool CHANGEINTERNAL = !PWINDOW->m_bPinned && CURRENT_EFFECTIVE_MODE != EFFECTIVE_MODE;

    if (*PALLOWPINFULLSCREEN && PWINDOW->m_bPinFullscreened && PWINDOW->isFullscreen() && !PWINDOW->m_bPinned && state.internal == FSMODE_NONE) {
        PWINDOW->m_bPinned          = true;
        PWINDOW->m_bPinFullscreened = false;
    }

    // TODO: update the state on syncFullscreen changes
    if (!CHANGEINTERNAL && PWINDOW->m_sWindowData.syncFullscreen.valueOrDefault())
        return;

    PWINDOW->m_sFullscreenState.client = state.client;
    g_pXWaylandManager->setWindowFullscreen(PWINDOW, state.client & FSMODE_FULLSCREEN);

    if (!CHANGEINTERNAL) {
        PWINDOW->updateDynamicRules();
        updateWindowAnimatedDecorationValues(PWINDOW);
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PWINDOW->monitorID());
        return;
    }

    g_pLayoutManager->getCurrentLayout()->fullscreenRequestForWindow(PWINDOW, CURRENT_EFFECTIVE_MODE, EFFECTIVE_MODE);

    PWINDOW->m_sFullscreenState.internal = state.internal;
    PWORKSPACE->m_efFullscreenMode       = EFFECTIVE_MODE;
    PWORKSPACE->m_bHasFullscreenWindow   = EFFECTIVE_MODE != FSMODE_NONE;

    g_pEventManager->postEvent(SHyprIPCEvent{.event = "fullscreen", .data = std::to_string((int)EFFECTIVE_MODE != FSMODE_NONE)});
    EMIT_HOOK_EVENT("fullscreen", PWINDOW);

    PWINDOW->updateDynamicRules();
    updateWindowAnimatedDecorationValues(PWINDOW);
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PWINDOW->monitorID());

    // make all windows on the same workspace under the fullscreen window
    for (auto const& w : m_vWindows) {
        if (w->m_pWorkspace == PWORKSPACE && !w->isFullscreen() && !w->m_bFadingOut && !w->m_bPinned)
            w->m_bCreatedOverFullscreen = false;
    }

    updateFullscreenFadeOnWorkspace(PWORKSPACE);

    PWINDOW->sendWindowSize(true);

    PWORKSPACE->forceReportSizesToWindows();

    g_pInputManager->recheckIdleInhibitorStatus();

    // further updates require a monitor
    if (!PMONITOR)
        return;

    // send a scanout tranche if we are entering fullscreen, and send a regular one if we aren't.
    // ignore if DS is disabled.
    if (*PDIRECTSCANOUT == 1 || (*PDIRECTSCANOUT == 2 && PWINDOW->getContentType() == CONTENT_TYPE_GAME))
        g_pHyprRenderer->setSurfaceScanoutMode(PWINDOW->m_pWLSurface->resource(), EFFECTIVE_MODE != FSMODE_NONE ? PMONITOR->self.lock() : nullptr);

    g_pConfigManager->ensureVRR(PMONITOR);
}

PHLWINDOW CCompositor::getX11Parent(PHLWINDOW pWindow) {
    if (!pWindow->m_bIsX11)
        return nullptr;

    for (auto const& w : m_vWindows) {
        if (!w->m_bIsX11)
            continue;

        if (w->m_pXWaylandSurface == pWindow->m_pXWaylandSurface->parent)
            return w;
    }

    return nullptr;
}

void CCompositor::scheduleFrameForMonitor(PHLMONITOR pMonitor, IOutput::scheduleFrameReason reason) {
    if ((m_pAqBackend->hasSession() && !m_pAqBackend->session->active) || !m_bSessionActive)
        return;

    if (!pMonitor->m_bEnabled)
        return;

    if (pMonitor->renderingActive)
        pMonitor->pendingFrame = true;

    pMonitor->output->scheduleFrame(reason);
}

PHLWINDOW CCompositor::getWindowByRegex(const std::string& regexp_) {
    auto regexp = trim(regexp_);

    if (regexp.starts_with("active"))
        return m_pLastWindow.lock();
    else if (regexp.starts_with("floating") || regexp.starts_with("tiled")) {
        // first floating on the current ws
        if (!valid(m_pLastWindow))
            return nullptr;

        const bool FLOAT = regexp.starts_with("floating");

        for (auto const& w : m_vWindows) {
            if (!w->m_bIsMapped || w->m_bIsFloating != FLOAT || w->m_pWorkspace != m_pLastWindow->m_pWorkspace || w->isHidden())
                continue;

            return w;
        }

        return nullptr;
    }

    eFocusWindowMode mode = MODE_CLASS_REGEX;

    std::string      regexCheck;
    std::string      matchCheck;
    if (regexp.starts_with("class:")) {
        regexCheck = regexp.substr(6);
    } else if (regexp.starts_with("initialclass:")) {
        mode       = MODE_INITIAL_CLASS_REGEX;
        regexCheck = regexp.substr(13);
    } else if (regexp.starts_with("title:")) {
        mode       = MODE_TITLE_REGEX;
        regexCheck = regexp.substr(6);
    } else if (regexp.starts_with("initialtitle:")) {
        mode       = MODE_INITIAL_TITLE_REGEX;
        regexCheck = regexp.substr(13);
    } else if (regexp.starts_with("tag:")) {
        mode       = MODE_TAG_REGEX;
        regexCheck = regexp.substr(4);
    } else if (regexp.starts_with("address:")) {
        mode       = MODE_ADDRESS;
        matchCheck = regexp.substr(8);
    } else if (regexp.starts_with("pid:")) {
        mode       = MODE_PID;
        matchCheck = regexp.substr(4);
    }

    for (auto const& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped || (w->isHidden() && !g_pLayoutManager->getCurrentLayout()->isWindowReachable(w)))
            continue;

        switch (mode) {
            case MODE_CLASS_REGEX: {
                const auto windowClass = w->m_szClass;
                if (!RE2::FullMatch(windowClass, regexCheck))
                    continue;
                break;
            }
            case MODE_INITIAL_CLASS_REGEX: {
                const auto initialWindowClass = w->m_szInitialClass;
                if (!RE2::FullMatch(initialWindowClass, regexCheck))
                    continue;
                break;
            }
            case MODE_TITLE_REGEX: {
                const auto windowTitle = w->m_szTitle;
                if (!RE2::FullMatch(windowTitle, regexCheck))
                    continue;
                break;
            }
            case MODE_INITIAL_TITLE_REGEX: {
                const auto initialWindowTitle = w->m_szInitialTitle;
                if (!RE2::FullMatch(initialWindowTitle, regexCheck))
                    continue;
                break;
            }
            case MODE_TAG_REGEX: {
                bool tagMatched = false;
                for (auto const& t : w->m_tags.getTags()) {
                    if (RE2::FullMatch(t, regexCheck)) {
                        tagMatched = true;
                        break;
                    }
                }
                if (!tagMatched)
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
        if (PMONITORNEW != m_pLastMonitor)
            setActiveMonitor(PMONITORNEW);
        return;
    }

    g_pPointerManager->warpTo(pos);

    const auto PMONITORNEW = getMonitorFromVector(pos);
    if (PMONITORNEW != m_pLastMonitor)
        setActiveMonitor(PMONITORNEW);
}

void CCompositor::closeWindow(PHLWINDOW pWindow) {
    if (pWindow && validMapped(pWindow)) {
        g_pXWaylandManager->sendCloseWindow(pWindow);
    }
}

PHLLS CCompositor::getLayerSurfaceFromSurface(SP<CWLSurfaceResource> pSurface) {
    std::pair<SP<CWLSurfaceResource>, bool> result = {pSurface, false};

    for (auto const& ls : m_vLayers) {
        if (ls->layerSurface && ls->layerSurface->surface == pSurface)
            return ls;

        if (!ls->layerSurface || !ls->mapped)
            continue;

        ls->layerSurface->surface->breadthfirst(
            [&result](SP<CWLSurfaceResource> surf, const Vector2D& offset, void* data) {
                if (surf == result.first) {
                    result.second = true;
                    return;
                }
            },
            nullptr);

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
        X = xIsPercent ? (std::stof(x) * 0.01 * relativeTo.x) + relativeTo.x : std::stoi(x) + relativeTo.x;
        Y = yIsPercent ? (std::stof(y) * 0.01 * relativeTo.y) + relativeTo.y : std::stoi(y) + relativeTo.y;
    }

    return Vector2D(X, Y);
}

PHLWORKSPACE CCompositor::createNewWorkspace(const WORKSPACEID& id, const MONITORID& monid, const std::string& name, bool isEmpty) {
    const auto NAME  = name.empty() ? std::to_string(id) : name;
    auto       monID = monid;

    // check if bound
    if (const auto PMONITOR = g_pConfigManager->getBoundMonitorForWS(NAME); PMONITOR)
        monID = PMONITOR->ID;

    const bool SPECIAL = id >= SPECIAL_WORKSPACE_START && id <= -2;

    const auto PMONITOR = getMonitorFromID(monID);
    if (!PMONITOR) {
        Debug::log(ERR, "BUG THIS: No pMonitor for new workspace in createNewWorkspace");
        return nullptr;
    }

    const auto PWORKSPACE = m_vWorkspaces.emplace_back(CWorkspace::create(id, PMONITOR, NAME, SPECIAL, isEmpty));

    PWORKSPACE->m_fAlpha->setValueAndWarp(0);

    return PWORKSPACE;
}

void CCompositor::setActiveMonitor(PHLMONITOR pMonitor) {
    if (m_pLastMonitor == pMonitor)
        return;

    if (!pMonitor) {
        m_pLastMonitor.reset();
        return;
    }

    const auto PWORKSPACE = pMonitor->activeWorkspace;

    const auto WORKSPACE_ID   = PWORKSPACE ? std::to_string(PWORKSPACE->m_iID) : std::to_string(WORKSPACE_INVALID);
    const auto WORKSPACE_NAME = PWORKSPACE ? PWORKSPACE->m_szName : "?";

    g_pEventManager->postEvent(SHyprIPCEvent{.event = "focusedmon", .data = pMonitor->szName + "," + WORKSPACE_NAME});
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "focusedmonv2", .data = pMonitor->szName + "," + WORKSPACE_ID});

    EMIT_HOOK_EVENT("focusedMon", pMonitor);
    m_pLastMonitor = pMonitor->self;
}

bool CCompositor::isWorkspaceSpecial(const WORKSPACEID& id) {
    return id >= SPECIAL_WORKSPACE_START && id <= -2;
}

WORKSPACEID CCompositor::getNewSpecialID() {
    WORKSPACEID highest = SPECIAL_WORKSPACE_START;
    for (auto const& ws : m_vWorkspaces) {
        if (ws->m_bIsSpecialWorkspace && ws->m_iID > highest) {
            highest = ws->m_iID;
        }
    }

    return highest + 1;
}

void CCompositor::performUserChecks() {
    static auto PNOCHECKXDG     = CConfigValue<Hyprlang::INT>("misc:disable_xdg_env_checks");
    static auto PNOCHECKQTUTILS = CConfigValue<Hyprlang::INT>("misc:disable_hyprland_qtutils_check");

    if (!*PNOCHECKXDG) {
        const auto CURRENT_DESKTOP_ENV = getenv("XDG_CURRENT_DESKTOP");
        if (!CURRENT_DESKTOP_ENV || std::string{CURRENT_DESKTOP_ENV} != "Hyprland") {
            g_pHyprNotificationOverlay->addNotification(
                std::format("Your XDG_CURRENT_DESKTOP environment seems to be managed externally, and the current value is {}.\nThis might cause issues unless it's intentional.",
                            CURRENT_DESKTOP_ENV ? CURRENT_DESKTOP_ENV : "unset"),
                CHyprColor{}, 15000, ICON_WARNING);
        }
    }

    if (!*PNOCHECKQTUTILS) {
        if (!NFsUtils::executableExistsInPath("hyprland-dialog")) {
            g_pHyprNotificationOverlay->addNotification(
                "Your system does not have hyprland-qtutils installed. This is a runtime dependency for some dialogs. Consider installing it.", CHyprColor{}, 15000, ICON_WARNING);
        }
    }

    if (g_pHyprOpenGL->failedAssetsNo > 0) {
        g_pHyprNotificationOverlay->addNotification(std::format("Hyprland failed to load {} essential asset{}, blame your distro's packager for doing a bad job at packaging!",
                                                                g_pHyprOpenGL->failedAssetsNo, g_pHyprOpenGL->failedAssetsNo > 1 ? "s" : ""),
                                                    CHyprColor{1.0, 0.1, 0.1, 1.0}, 15000, ICON_ERROR);
    }
}

void CCompositor::moveWindowToWorkspaceSafe(PHLWINDOW pWindow, PHLWORKSPACE pWorkspace) {
    if (!pWindow || !pWorkspace)
        return;

    if (pWindow->m_bPinned && pWorkspace->m_bIsSpecialWorkspace)
        return;

    if (pWindow->m_pWorkspace == pWorkspace)
        return;

    const bool FULLSCREEN     = pWindow->isFullscreen();
    const auto FULLSCREENMODE = pWindow->m_sFullscreenState.internal;
    const bool WASVISIBLE     = pWindow->m_pWorkspace && pWindow->m_pWorkspace->isVisible();

    if (FULLSCREEN)
        setWindowFullscreenInternal(pWindow, FSMODE_NONE);

    const PHLWINDOW pFirstWindowOnWorkspace   = pWorkspace->getFirstWindow();
    const int       visibleWindowsOnWorkspace = pWorkspace->getWindows(std::nullopt, true);
    const auto      POSTOMON                  = pWindow->m_vRealPosition->goal() - (pWindow->m_pMonitor ? pWindow->m_pMonitor->vecPosition : Vector2D{});
    const auto      PWORKSPACEMONITOR         = pWorkspace->m_pMonitor.lock();

    if (!pWindow->m_bIsFloating)
        g_pLayoutManager->getCurrentLayout()->onWindowRemovedTiling(pWindow);

    pWindow->moveToWorkspace(pWorkspace);
    pWindow->m_pMonitor = pWorkspace->m_pMonitor;

    static auto PGROUPONMOVETOWORKSPACE = CConfigValue<Hyprlang::INT>("group:group_on_movetoworkspace");
    if (*PGROUPONMOVETOWORKSPACE && visibleWindowsOnWorkspace == 1 && pFirstWindowOnWorkspace && pFirstWindowOnWorkspace != pWindow &&
        pFirstWindowOnWorkspace->m_sGroupData.pNextWindow.lock() && pWindow->canBeGroupedInto(pFirstWindowOnWorkspace)) {

        pWindow->m_bIsFloating = pFirstWindowOnWorkspace->m_bIsFloating; // match the floating state. Needed to group tiled into floated and vice versa.
        if (!pWindow->m_sGroupData.pNextWindow.expired()) {
            PHLWINDOW next = pWindow->m_sGroupData.pNextWindow.lock();
            while (next != pWindow) {
                next->m_bIsFloating = pFirstWindowOnWorkspace->m_bIsFloating; // match the floating state of group members
                next                = next->m_sGroupData.pNextWindow.lock();
            }
        }

        static auto USECURRPOS = CConfigValue<Hyprlang::INT>("group:insert_after_current");
        (*USECURRPOS ? pFirstWindowOnWorkspace : pFirstWindowOnWorkspace->getGroupTail())->insertWindowToGroup(pWindow);

        pFirstWindowOnWorkspace->setGroupCurrent(pWindow);
        pWindow->updateWindowDecos();
        g_pLayoutManager->getCurrentLayout()->recalculateWindow(pWindow);

        if (!pWindow->getDecorationByType(DECORATION_GROUPBAR))
            pWindow->addWindowDeco(makeUnique<CHyprGroupBarDecoration>(pWindow));

    } else {
        if (!pWindow->m_bIsFloating)
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedTiling(pWindow);

        if (pWindow->m_bIsFloating)
            *pWindow->m_vRealPosition = POSTOMON + PWORKSPACEMONITOR->vecPosition;
    }

    pWindow->updateToplevel();
    pWindow->updateDynamicRules();
    pWindow->uncacheWindowDecos();
    pWindow->updateGroupOutputs();

    if (!pWindow->m_sGroupData.pNextWindow.expired()) {
        PHLWINDOW next = pWindow->m_sGroupData.pNextWindow.lock();
        while (next != pWindow) {
            next->updateToplevel();
            next = next->m_sGroupData.pNextWindow.lock();
        }
    }

    if (FULLSCREEN)
        setWindowFullscreenInternal(pWindow, FULLSCREENMODE);

    pWorkspace->updateWindows();
    if (pWindow->m_pWorkspace)
        pWindow->m_pWorkspace->updateWindows();
    g_pCompositor->updateSuspendedStates();

    if (!WASVISIBLE && pWindow->m_pWorkspace && pWindow->m_pWorkspace->isVisible()) {
        pWindow->m_fMovingFromWorkspaceAlpha->setValueAndWarp(0.F);
        *pWindow->m_fMovingFromWorkspaceAlpha = 1.F;
    }
}

PHLWINDOW CCompositor::getForceFocus() {
    for (auto const& w : m_vWindows) {
        if (!w->m_bIsMapped || w->isHidden() || !w->m_pWorkspace || !w->m_pWorkspace->isVisible())
            continue;

        if (!w->m_bStayFocused)
            continue;

        return w;
    }

    return nullptr;
}

void CCompositor::arrangeMonitors() {
    static auto* const      PXWLFORCESCALEZERO = (Hyprlang::INT* const*)g_pConfigManager->getConfigValuePtr("xwayland:force_zero_scaling");

    std::vector<PHLMONITOR> toArrange(m_vMonitors.begin(), m_vMonitors.end());
    std::vector<PHLMONITOR> arranged;
    arranged.reserve(toArrange.size());

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
    int  maxXOffsetRight = 0;
    int  maxXOffsetLeft  = 0;
    int  maxYOffsetUp    = 0;
    int  maxYOffsetDown  = 0;

    auto recalcMaxOffsets = [&]() {
        maxXOffsetRight = 0;
        maxXOffsetLeft  = 0;
        maxYOffsetUp    = 0;
        maxYOffsetDown  = 0;

        // Finds the max and min values of explicitely placed monitors.
        for (auto const& m : arranged) {
            maxXOffsetRight = std::max<double>(m->vecPosition.x + m->vecSize.x, maxXOffsetRight);
            maxXOffsetLeft  = std::min<double>(m->vecPosition.x, maxXOffsetLeft);
            maxYOffsetDown  = std::max<double>(m->vecPosition.y + m->vecSize.y, maxYOffsetDown);
            maxYOffsetUp    = std::min<double>(m->vecPosition.y, maxYOffsetUp);
        }
    };

    // Iterates through all non-explicitly placed monitors.
    for (auto const& m : toArrange) {
        recalcMaxOffsets();

        // Moves the monitor to their appropriate position on the x/y axis and
        // increments/decrements the corresponding max offset.
        Vector2D newPosition = {0, 0};
        switch (m->activeMonitorRule.autoDir) {
            case eAutoDirs::DIR_AUTO_UP: newPosition.y = maxYOffsetUp - m->vecSize.y; break;
            case eAutoDirs::DIR_AUTO_DOWN: newPosition.y = maxYOffsetDown; break;
            case eAutoDirs::DIR_AUTO_LEFT: newPosition.x = maxXOffsetLeft - m->vecSize.x; break;
            case eAutoDirs::DIR_AUTO_RIGHT:
            case eAutoDirs::DIR_AUTO_NONE: newPosition.x = maxXOffsetRight; break;
            default: UNREACHABLE();
        }
        Debug::log(LOG, "arrangeMonitors: {} auto {:j}", m->szName, m->vecPosition);
        m->moveTo(newPosition);
        arranged.emplace_back(m);
    }

    // reset maxXOffsetRight (reuse)
    // and set xwayland positions aka auto for all
    maxXOffsetRight = 0;
    for (auto const& m : m_vMonitors) {
        Debug::log(LOG, "arrangeMonitors: {} xwayland [{}, {}]", m->szName, maxXOffsetRight, 0);
        m->vecXWaylandPosition = {maxXOffsetRight, 0};
        maxXOffsetRight += (*PXWLFORCESCALEZERO ? m->vecTransformedSize.x : m->vecSize.x);

        if (*PXWLFORCESCALEZERO)
            m->xwaylandScale = m->scale;
        else
            m->xwaylandScale = 1.f;
    }

    PROTO::xdgOutput->updateAllOutputs();
}

void CCompositor::enterUnsafeState() {
    if (m_bUnsafeState)
        return;

    Debug::log(LOG, "Entering unsafe state");

    if (!m_pUnsafeOutput->m_bEnabled)
        m_pUnsafeOutput->onConnect(false);

    m_bUnsafeState = true;

    setActiveMonitor(m_pUnsafeOutput.lock());
}

void CCompositor::leaveUnsafeState() {
    if (!m_bUnsafeState)
        return;

    Debug::log(LOG, "Leaving unsafe state");

    m_bUnsafeState = false;

    PHLMONITOR pNewMonitor = nullptr;
    for (auto const& pMonitor : m_vMonitors) {
        if (pMonitor->output != m_pUnsafeOutput->output) {
            pNewMonitor = pMonitor;
            break;
        }
    }

    RASSERT(pNewMonitor, "Tried to leave unsafe without a monitor");

    if (m_pUnsafeOutput->m_bEnabled)
        m_pUnsafeOutput->onDisconnect();

    for (auto const& m : m_vMonitors) {
        scheduleFrameForMonitor(m);
    }
}

void CCompositor::setPreferredScaleForSurface(SP<CWLSurfaceResource> pSurface, double scale) {
    PROTO::fractional->sendScale(pSurface, scale);
    pSurface->sendPreferredScale(std::ceil(scale));

    const auto PSURFACE = CWLSurface::fromResource(pSurface);
    if (!PSURFACE) {
        Debug::log(WARN, "Orphaned CWLSurfaceResource {:x} in setPreferredScaleForSurface", (uintptr_t)pSurface.get());
        return;
    }

    PSURFACE->m_fLastScale = scale;
    PSURFACE->m_iLastScale = static_cast<int32_t>(std::ceil(scale));
}

void CCompositor::setPreferredTransformForSurface(SP<CWLSurfaceResource> pSurface, wl_output_transform transform) {
    pSurface->sendPreferredTransform(transform);

    const auto PSURFACE = CWLSurface::fromResource(pSurface);
    if (!PSURFACE) {
        Debug::log(WARN, "Orphaned CWLSurfaceResource {:x} in setPreferredTransformForSurface", (uintptr_t)pSurface.get());
        return;
    }

    PSURFACE->m_eLastTransform = transform;
}

void CCompositor::updateSuspendedStates() {
    for (auto const& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped)
            continue;

        w->setSuspended(w->isHidden() || !w->m_pWorkspace || !w->m_pWorkspace->isVisible());
    }
}

static void checkDefaultCursorWarp(PHLMONITOR monitor) {
    static auto PCURSORMONITOR    = CConfigValue<std::string>("cursor:default_monitor");
    static bool cursorDefaultDone = false;
    static bool firstLaunch       = true;

    const auto  POS = monitor->middle();

    // by default, cursor should be set to first monitor detected
    // this is needed as a default if the monitor given in config above doesn't exist
    if (firstLaunch) {
        firstLaunch = false;
        g_pCompositor->warpCursorTo(POS, true);
        g_pInputManager->refocus();
        return;
    }

    if (!cursorDefaultDone && *PCURSORMONITOR != STRVAL_EMPTY) {
        if (*PCURSORMONITOR == monitor->szName) {
            cursorDefaultDone = true;
            g_pCompositor->warpCursorTo(POS, true);
            g_pInputManager->refocus();
            return;
        }
    }

    // modechange happend check if cursor is on that monitor and warp it to middle to not place it out of bounds if resolution changed.
    if (g_pCompositor->getMonitorFromCursor() == monitor) {
        g_pCompositor->warpCursorTo(POS, true);
        g_pInputManager->refocus();
    }
}

void CCompositor::onNewMonitor(SP<Aquamarine::IOutput> output) {
    // add it to real
    auto PNEWMONITOR = g_pCompositor->m_vRealMonitors.emplace_back(makeShared<CMonitor>(output));
    if (std::string("HEADLESS-1") == output->name) {
        g_pCompositor->m_pUnsafeOutput = PNEWMONITOR;
        output->name                   = "FALLBACK"; // we are allowed to do this :)
    }

    Debug::log(LOG, "New output with name {}", output->name);

    PNEWMONITOR->szName           = output->name;
    PNEWMONITOR->self             = PNEWMONITOR;
    const bool FALLBACK           = g_pCompositor->m_pUnsafeOutput ? output == g_pCompositor->m_pUnsafeOutput->output : false;
    PNEWMONITOR->ID               = FALLBACK ? MONITOR_INVALID : g_pCompositor->getNextAvailableMonitorID(output->name);
    PNEWMONITOR->isUnsafeFallback = FALLBACK;

    EMIT_HOOK_EVENT("newMonitor", PNEWMONITOR);

    if (!FALLBACK)
        PNEWMONITOR->onConnect(false);

    if (!PNEWMONITOR->m_bEnabled || FALLBACK)
        return;

    // ready to process if we have a real monitor

    if ((!g_pHyprRenderer->m_pMostHzMonitor || PNEWMONITOR->refreshRate > g_pHyprRenderer->m_pMostHzMonitor->refreshRate) && PNEWMONITOR->m_bEnabled)
        g_pHyprRenderer->m_pMostHzMonitor = PNEWMONITOR;

    g_pCompositor->m_bReadyToProcess = true;

    g_pConfigManager->m_bWantsMonitorReload = true;
    g_pCompositor->scheduleFrameForMonitor(PNEWMONITOR, IOutput::AQ_SCHEDULE_NEW_MONITOR);

    checkDefaultCursorWarp(PNEWMONITOR);

    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->m_pMonitor == PNEWMONITOR) {
            w->m_iLastSurfaceMonitorID = MONITOR_INVALID;
            w->updateSurfaceScaleTransformDetails();
        }
    }

    g_pHyprRenderer->damageMonitor(PNEWMONITOR);
    PNEWMONITOR->onMonitorFrame();

    if (PROTO::colorManagement && shouldChangePreferredImageDescription()) {
        Debug::log(ERR, "FIXME: color management protocol is enabled, need a preferred image description id");
        PROTO::colorManagement->onImagePreferredChanged(0);
    }
}

SImageDescription CCompositor::getPreferredImageDescription() {
    if (!PROTO::colorManagement) {
        Debug::log(ERR, "FIXME: color management protocol is not enabled, returning empty image description");
        return SImageDescription{};
    }
    Debug::log(WARN, "FIXME: color management protocol is enabled, determine correct preferred image description");
    // should determine some common settings to avoid unnecessary transformations while keeping maximum displayable precision
    return SImageDescription{.primaries = NColorPrimaries::BT709};
}

bool CCompositor::shouldChangePreferredImageDescription() {
    Debug::log(WARN, "FIXME: color management protocol is enabled and outputs changed, check preferred image description changes");
    return false;
}

void CCompositor::ensurePersistentWorkspacesPresent(const std::vector<SWorkspaceRule>& rules, PHLWORKSPACE pWorkspace) {
    if (!m_pLastMonitor)
        return;

    for (const auto& rule : rules) {
        if (!rule.isPersistent)
            continue;

        PHLWORKSPACE PWORKSPACE = nullptr;
        if (pWorkspace) {
            if (pWorkspace->matchesStaticSelector(rule.workspaceString))
                PWORKSPACE = pWorkspace;
            else
                continue;
        }

        const auto PMONITOR = getMonitorFromString(rule.monitor);

        if (!rule.monitor.empty() && !PMONITOR)
            continue; // don't do anything yet, as the monitor is not yet present.

        if (!PWORKSPACE) {
            WORKSPACEID id     = rule.workspaceId;
            std::string wsname = rule.workspaceName;

            if (id == WORKSPACE_INVALID) {
                const auto R = getWorkspaceIDNameFromString(rule.workspaceString);
                id           = R.id;
                wsname       = R.name;
            }

            if (id == WORKSPACE_INVALID) {
                Debug::log(ERR, "ensurePersistentWorkspacesPresent: couldn't resolve id for workspace {}", rule.workspaceString);
                continue;
            }
            PWORKSPACE = getWorkspaceByID(id);
            if (!PWORKSPACE)
                createNewWorkspace(id, PMONITOR ? PMONITOR->ID : m_pLastMonitor->ID, wsname, false);
        }

        if (PWORKSPACE)
            PWORKSPACE->m_bPersistent = true;

        if (!PMONITOR) {
            Debug::log(ERR, "ensurePersistentWorkspacesPresent: couldn't resolve monitor for {}, skipping", rule.monitor);
            continue;
        }

        if (PWORKSPACE) {
            if (PWORKSPACE->m_pMonitor == PMONITOR) {
                Debug::log(LOG, "ensurePersistentWorkspacesPresent: workspace persistent {} already on {}", rule.workspaceString, PMONITOR->szName);

                continue;
            }

            Debug::log(LOG, "ensurePersistentWorkspacesPresent: workspace persistent {} not on {}, moving", rule.workspaceString, PMONITOR->szName);
            moveWorkspaceToMonitor(PWORKSPACE, PMONITOR);
            continue;
        }
    }

    // cleanup old
    sanityCheckWorkspaces();
}
