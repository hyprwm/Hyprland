
#include "Compositor.hpp"
#include "config/supplementary/executor/Executor.hpp"
#include "debug/log/Logger.hpp"
#include "desktop/DesktopTypes.hpp"
#include "desktop/state/FocusState.hpp"
#include "desktop/history/WindowHistoryTracker.hpp"
#include "desktop/history/WorkspaceHistoryTracker.hpp"
#include "helpers/Splashes.hpp"
#include "helpers/SystemInfo.hpp"
#include "config/ConfigValue.hpp"
#include "config/shared/inotify/ConfigWatcher.hpp"
#include "config/shared/monitor/MonitorRuleManager.hpp"
#include "pointer/cursor/CursorManager.hpp"
#include "managers/TokenManager.hpp"
#include "pointer/PointerManager.hpp"
#include "managers/SeatManager.hpp"
#include "managers/VersionKeeperManager.hpp"
#include "managers/DonationNagManager.hpp"
#include "managers/ANRManager.hpp"
#include "managers/eventLoop/EventLoopManager.hpp"
#include "managers/permissions/DynamicPermissionManager.hpp"
#include "state/FallbackState.hpp"
#include "state/MonitorState.hpp"
#include "state/WorkspaceState.hpp"
#include <aquamarine/output/Output.hpp>
#include <ctime>
#include <random>
#include <print>
#include <cstring>
#include <filesystem>
#include "debug/HyprCtl.hpp"
#include "debug/crash/CrashReporter.hpp"
#include "render/GLRenderer.hpp"
#include "render/ShaderLoader.hpp"
#ifdef USES_SYSTEMD
#include <helpers/SdDaemon.hpp> // for SdNotify
#endif
#include "helpers/fs/FsUtils.hpp"
#include "helpers/env/Env.hpp"
#include "protocols/SecurityContext.hpp"
#include "protocols/ColorManagement.hpp"
#include "render/Renderer.hpp"
#include "xwayland/XWayland.hpp"
#include "helpers/ByteOperations.hpp"

#include "managers/KeybindManager.hpp"
#include "managers/SessionLockManager.hpp"
#include "managers/XWaylandManager.hpp"

#include "config/ConfigManager.hpp"
#include "render/OpenGL.hpp"
#include "managers/input/InputManager.hpp"
#include "animation/AnimationManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/ProtocolManager.hpp"
#include "managers/WelcomeManager.hpp"
#include "render/AsyncResourceGatherer.hpp"
#include "plugins/PluginSystem.hpp"
#include "errorOverlay/Overlay.hpp"
#include "notification/NotificationOverlay.hpp"
#include "debug/Overlay.hpp"
#include "i18n/Engine.hpp"
#include "layout/LayoutManager.hpp"
#include "event/EventBus.hpp"

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList2.hpp>
#include <hyprutils/string/Numeric.hpp>
#include <aquamarine/input/Input.hpp>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <unistd.h>
#include <xf86drm.h>

#if defined(__linux__)
#include <linux/vt.h>
#endif

using namespace Hyprutils::String;
using namespace Aquamarine;
using enum NContentType::eContentType;
using namespace NColorManagement;
using namespace Desktop::View;
using namespace Render::GL;

static int handleCritSignal(int signo, void* data) {
    Log::logger->log(Log::DEBUG, "Hyprland received signal {}", signo);

    if (signo == SIGTERM || signo == SIGINT || signo == SIGKILL)
        g_pCompositor->stopCompositor();

    return 0;
}

static void handleUnrecoverableSignal(int sig) {

    // remove our handlers
    signal(SIGABRT, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);

    // Kill the program if the crash-reporter is caught in a deadlock.
    signal(SIGALRM, [](int _) {
        char const*           msg = "\nCrashReporter exceeded timeout, forcefully exiting\n";
        [[maybe_unused]] auto w   = write(2, msg, strlen(msg));
        abort();
    });
    alarm(15);

    CrashReporter::createAndSaveCrash(sig);

    abort();
}

static void handleUserSignal(int sig) {
    if (sig == SIGUSR1) {
        // means we have to unwind a timed out event
        throw std::exception();
    }
}

bool CCompositor::setWatchdogFd(int fd) {
    m_watchdogWriteFd = Hyprutils::OS::CFileDescriptor{fd};
    m_watchdogWriteFd.setFlags(m_watchdogWriteFd.getFlags() | FD_CLOEXEC);
    return m_watchdogWriteFd.isValid() && !m_watchdogWriteFd.isClosed();
}

bool CCompositor::writeWatchdogFd(std::string str) {
    if (!m_watchdogWriteFd.isValid())
        return false;
    str += '\n';
    auto w = write(m_watchdogWriteFd.get(), str.c_str(), str.size());
    return w >= 0;
}

void CCompositor::bumpNofile() {
    if (!getrlimit(RLIMIT_NOFILE, &m_originalNofile))
        Log::logger->log(Log::DEBUG, "Old rlimit: soft -> {}, hard -> {}", m_originalNofile.rlim_cur, m_originalNofile.rlim_max);
    else {
        Log::logger->log(Log::ERR, "Failed to get NOFILE rlimits");
        m_originalNofile.rlim_max = 0;
        return;
    }

    rlimit newLimit = m_originalNofile;

    newLimit.rlim_cur = newLimit.rlim_max;

    if (setrlimit(RLIMIT_NOFILE, &newLimit) < 0) {
        Log::logger->log(Log::ERR, "Failed bumping NOFILE limits higher");
        m_originalNofile.rlim_max = 0;
        return;
    }

    if (!getrlimit(RLIMIT_NOFILE, &newLimit))
        Log::logger->log(Log::DEBUG, "New rlimit: soft -> {}, hard -> {}", newLimit.rlim_cur, newLimit.rlim_max);
}

void CCompositor::restoreNofile() {
    if (m_originalNofile.rlim_max <= 0)
        return;

    if (setrlimit(RLIMIT_NOFILE, &m_originalNofile) < 0)
        Log::logger->log(Log::ERR, "Failed restoring NOFILE limits");
}

bool CCompositor::supportsDrmSyncobjTimeline() const {
    return m_drm.syncobjSupport || m_drmRenderNode.syncObjSupport;
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

CCompositor::CCompositor(bool onlyConfig) : m_onlyConfigVerification(onlyConfig), m_hyprlandPID(getpid()) {
    if (onlyConfig)
        return;

    setMallocThreshold();

    m_hyprTempDataRoot = std::string{getenv("XDG_RUNTIME_DIR")} + "/hypr";

    if (m_hyprTempDataRoot.starts_with("/hypr")) {
        std::println("Bailing out, $XDG_RUNTIME_DIR is invalid");
        throw std::runtime_error("CCompositor() failed");
    }

    if (!m_hyprTempDataRoot.starts_with("/run/user"))
        std::println("[!!WARNING!!] XDG_RUNTIME_DIR looks non-standard. Proceeding anyways...");

    std::random_device              dev;
    std::mt19937                    engine(dev());
    std::uniform_int_distribution<> distribution(0, INT32_MAX);

    m_instanceSignature = std::format("{}_{}_{}", GIT_COMMIT_HASH, std::time(nullptr), distribution(engine));

    setenv("HYPRLAND_INSTANCE_SIGNATURE", m_instanceSignature.c_str(), true);

    if (!std::filesystem::exists(m_hyprTempDataRoot))
        mkdir(m_hyprTempDataRoot.c_str(), S_IRWXU);
    else if (!std::filesystem::is_directory(m_hyprTempDataRoot)) {
        std::println("Bailing out, {} is not a directory", m_hyprTempDataRoot);
        throw std::runtime_error("CCompositor() failed");
    }

    m_instancePath = m_hyprTempDataRoot + "/" + m_instanceSignature;

    if (std::filesystem::exists(m_instancePath)) {
        std::println("Bailing out, {} exists??", m_instancePath);
        throw std::runtime_error("CCompositor() failed");
    }

    if (mkdir(m_instancePath.c_str(), S_IRWXU) < 0) {
        std::println("Bailing out, couldn't create {}", m_instancePath);
        throw std::runtime_error("CCompositor() failed");
    }

    Log::logger->initIS(m_instancePath);

    setRandomSplash();
    bumpNofile();
}

CCompositor::~CCompositor() {
    if (!m_isShuttingDown && !m_onlyConfigVerification)
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

    m_currentSplash = SPLASHES->at(distribution(engine));
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
    if (m_onlyConfigVerification) {
        g_pKeybindManager = makeUnique<CKeybindManager>();
        Animation::mgr();
        Config::initConfigManager();
        Config::mgr()->init();

        std::println("\n\n======== Config parsing result:\n\n{}", Config::mgr()->verify());
        return;
    }

    m_wlDisplay = wl_display_create();

    wl_display_set_global_filter(m_wlDisplay, ::filterGlobals, nullptr);

    m_wlEventLoop = wl_display_get_event_loop(m_wlDisplay);

    // register crit signal handler
    m_critSigSource = wl_event_loop_add_signal(m_wlEventLoop, SIGTERM, handleCritSignal, nullptr);

    if (!Env::envEnabled("HYPRLAND_NO_CRASHREPORTER")) {
        signal(SIGSEGV, handleUnrecoverableSignal);
        signal(SIGABRT, handleUnrecoverableSignal);
    }
    signal(SIGUSR1, handleUserSignal);

    initManagers(STAGE_PRIORITY);

    Log::logger->initCallbacks();

    // set the buffer size to 1MB to avoid disconnects due to an app hanging for a short while
    wl_display_set_default_max_buffer_size(m_wlDisplay, 1_MB);

    Aquamarine::SBackendOptions           options{};
    SP<Hyprutils::CLI::CLoggerConnection> conn = makeShared<Hyprutils::CLI::CLoggerConnection>(Log::logger->hu());
    conn->setLogLevel(Log::DEBUG);
    conn->setName("aquamarine");
    options.logConnection = std::move(conn);

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

    m_aqBackend = CBackend::create(implementations, options);

    if (!m_aqBackend) {
        Log::logger->log(
            Log::CRIT,
            "m_pAqBackend was null! This usually means aquamarine could not find a GPU or encountered some issues. Make sure you're running either on a tty or on a Wayland "
            "session, NOT an X11 one.");
        throwError("CBackend::create() failed!");
    }

    // TODO: headless only

    initAllSignals();

    if (!m_aqBackend->start()) {
        Log::logger->log(
            Log::CRIT,
            "m_pAqBackend couldn't start! This usually means aquamarine could not find a GPU or encountered some issues. Make sure you're running either on a tty or on a "
            "Wayland session, NOT an X11 one.");
        throwError("CBackend::create() failed!");
    }

    m_initialized = true;

    Log::logger->log(Log::DEBUG, "Instance Signature: {}", m_instanceSignature);
    Log::logger->log(Log::DEBUG, "Runtime directory: {}", m_instancePath);
    Log::logger->log(Log::DEBUG, "Hyprland PID: {}", m_hyprlandPID);
    Log::logger->log(Log::DEBUG, "===== SYSTEM INFO: =====");
    Log::logger->log(Log::DEBUG, "{}", Helpers::SystemInfo::getSystemInfo());
    Log::logger->log(Log::DEBUG, "========================");
    Log::logger->log(Log::DEBUG, "\n\n"); // pad
    Log::logger->log(Log::INFO, "If you are crashing, or encounter any bugs, please consult https://wiki.hypr.land/Crashes-and-Bugs/\n\n");
    Log::logger->log(Log::DEBUG, "\nCurrent splash: {}\n\n", m_currentSplash);

    m_drm.fd = m_aqBackend->drmFD();
    Log::logger->log(Log::DEBUG, "Running on DRMFD: {}", m_drm.fd);

    m_drmRenderNode.fd = m_aqBackend->drmRenderNodeFD();
    Log::logger->log(Log::DEBUG, "Using RENDERNODEFD: {}", m_drmRenderNode.fd);

#if defined(__linux__)
    auto syncObjSupport = [](auto fd) {
        if (fd < 0)
            return false;

        uint64_t cap = 0;
        int      ret = drmGetCap(fd, DRM_CAP_SYNCOBJ_TIMELINE, &cap);
        return ret == 0 && cap != 0;
    };

    m_drm.syncobjSupport = syncObjSupport(m_drm.fd);
    Log::logger->log(Log::DEBUG, "DRM DisplayNode syncobj timeline support: {}", m_drm.syncobjSupport ? "yes" : "no");

    m_drmRenderNode.syncObjSupport = syncObjSupport(m_drmRenderNode.fd);
    Log::logger->log(Log::DEBUG, "DRM RenderNode syncobj timeline support: {}", m_drmRenderNode.syncObjSupport ? "yes" : "no");

    if (!m_drm.syncobjSupport && !m_drmRenderNode.syncObjSupport)
        Log::logger->log(Log::DEBUG, "DRM no syncobj support, disabling explicit sync");
#else
    Log::logger->log(Log::DEBUG, "DRM syncobj timeline support: no (not linux)");
#endif

    if (!socketName.empty() && socketFd != -1) {
        fcntl(socketFd, F_SETFD, FD_CLOEXEC);
        const auto RETVAL = wl_display_add_socket_fd(m_wlDisplay, socketFd);
        if (RETVAL >= 0) {
            m_wlDisplaySocket = socketName;
            Log::logger->log(Log::DEBUG, "wl_display_add_socket_fd for {} succeeded with {}", socketName, RETVAL);
        } else
            Log::logger->log(Log::WARN, "wl_display_add_socket_fd for {} returned {}: skipping", socketName, RETVAL);
    } else {
        // get socket, avoid using 0
        for (int candidate = 1; candidate <= 32; candidate++) {
            const auto CANDIDATESTR = ("wayland-" + std::to_string(candidate));
            const auto RETVAL       = wl_display_add_socket(m_wlDisplay, CANDIDATESTR.c_str());
            if (RETVAL >= 0) {
                m_wlDisplaySocket = CANDIDATESTR;
                Log::logger->log(Log::DEBUG, "wl_display_add_socket for {} succeeded with {}", CANDIDATESTR, RETVAL);
                break;
            } else
                Log::logger->log(Log::WARN, "wl_display_add_socket for {} returned {}: skipping candidate {}", CANDIDATESTR, RETVAL, candidate);
        }
    }

    if (m_wlDisplaySocket.empty()) {
        Log::logger->log(Log::WARN, "All candidates failed, trying wl_display_add_socket_auto");
        const auto SOCKETSTR = wl_display_add_socket_auto(m_wlDisplay);
        if (SOCKETSTR)
            m_wlDisplaySocket = SOCKETSTR;
    }

    if (m_wlDisplaySocket.empty()) {
        Log::logger->log(Log::CRIT, "m_szWLDisplaySocket NULL!");
        throwError("m_szWLDisplaySocket was null! (wl_display_add_socket and wl_display_add_socket_auto failed)");
    }

    setenv("WAYLAND_DISPLAY", m_wlDisplaySocket.c_str(), 1);
    if (!getenv("XDG_CURRENT_DESKTOP")) {
        setenv("XDG_CURRENT_DESKTOP", "Hyprland", 1);
        m_desktopEnvSet = true;
    }

    initManagers(STAGE_BASICINIT);

    initManagers(STAGE_LATE);

    m_listeners.lock = g_pSessionLockManager->m_events.lock.listen([this] {
        static int lock_count = 0;
        // lock_count used to avoid triggering condition on initial forceLock()
        if (m_startLocked && lock_count >= 1) {
            // Lock manager has taken over
            m_startLocked = false;
            m_startLockedCommand.clear();
        }
        lock_count++;
        writeWatchdogFd("lock");
    });

    m_listeners.unlock = g_pSessionLockManager->m_events.unlock.listen([this] {
        m_startLocked = false;
        writeWatchdogFd("unlock");
    });

    if (m_startLocked) {
        g_pSessionLockManager->forceLock();

        if (!m_startLockedCommand.empty())
            Config::Supplementary::executor()->spawn(m_startLockedCommand);
    }

    for (auto const& o : pendingOutputs) {
        State::monitorState()->add(o);
    }
    pendingOutputs.clear();
}

void CCompositor::initAllSignals() {
    m_aqBackend->events.newOutput.listenStatic([this](const SP<Aquamarine::IOutput>& output) {
        Log::logger->log(Log::DEBUG, "New aquamarine output with name {}", output->name);
        if (m_initialized)
            State::monitorState()->add(output);
        else
            pendingOutputs.emplace_back(output);
    });

    m_aqBackend->events.newPointer.listenStatic([](const SP<Aquamarine::IPointer>& dev) {
        Log::logger->log(Log::DEBUG, "New aquamarine pointer with name {}", dev->getName());
        g_pInputManager->newMouse(dev);
        g_pInputManager->updateCapabilities();
    });

    m_aqBackend->events.newKeyboard.listenStatic([](const SP<Aquamarine::IKeyboard>& dev) {
        Log::logger->log(Log::DEBUG, "New aquamarine keyboard with name {}", dev->getName());
        g_pInputManager->newKeyboard(dev);
        g_pInputManager->updateCapabilities();
    });

    m_aqBackend->events.newTouch.listenStatic([](const SP<Aquamarine::ITouch>& dev) {
        Log::logger->log(Log::DEBUG, "New aquamarine touch with name {}", dev->getName());
        g_pInputManager->newTouchDevice(dev);
        g_pInputManager->updateCapabilities();
    });

    m_aqBackend->events.newSwitch.listenStatic([](const SP<Aquamarine::ISwitch>& dev) {
        Log::logger->log(Log::DEBUG, "New aquamarine switch with name {}", dev->getName());
        g_pInputManager->newSwitch(dev);
    });

    m_aqBackend->events.newTablet.listenStatic([](const SP<Aquamarine::ITablet>& dev) {
        Log::logger->log(Log::DEBUG, "New aquamarine tablet with name {}", dev->getName());
        g_pInputManager->newTablet(dev);
    });

    m_aqBackend->events.newTabletPad.listenStatic([](const SP<Aquamarine::ITabletPad>& dev) {
        Log::logger->log(Log::DEBUG, "New aquamarine tablet pad with name {}", dev->getName());
        g_pInputManager->newTabletPad(dev);
    });

    if (m_aqBackend->hasSession()) {
        m_aqBackend->session->events.changeActive.listenStatic([this] {
            if (m_aqBackend->session->active) {
                Log::logger->log(Log::DEBUG, "Session got activated!");

                m_sessionActive = true;

                // Reset animation tick state to avoid stale timer issues after suspend/wake
                if (Animation::mgr())
                    Animation::mgr()->resetTickState();

                for (auto const& m : State::monitorState()->monitors()) {
                    m->m_activeMonitorRule = {}; // rules were lost
                }

                Config::monitorRuleMgr()->scheduleReload();
                Pointer::Cursor::mgr()->syncGsettings();
            } else {
                Log::logger->log(Log::DEBUG, "Session got deactivated!");

                m_sessionActive = false;
            }
        });
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
    if (m_desktopEnvSet)
        unsetenv("XDG_CURRENT_DESKTOP");

    if (m_aqBackend->hasSession() && !Env::envEnabled("HYPRLAND_NO_SD_VARS")) {
        const auto CMD =
#ifdef USES_SYSTEMD
            "systemctl --user unset-environment DISPLAY WAYLAND_DISPLAY HYPRLAND_INSTANCE_SIGNATURE XDG_CURRENT_DESKTOP QT_QPA_PLATFORMTHEME PATH XDG_DATA_DIRS && hash "
            "dbus-update-activation-environment 2>/dev/null && "
#endif
            "dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP HYPRLAND_INSTANCE_SIGNATURE QT_QPA_PLATFORMTHEME PATH XDG_DATA_DIRS";
        Config::Supplementary::executor()->spawn(CMD);
    }
}

void CCompositor::stopCompositor() {
    Log::logger->log(Log::DEBUG, "Hyprland is stopping!");

    // this stops the wayland loop, wl_display_run
    wl_display_terminate(m_wlDisplay);
    m_isShuttingDown = true;
}

void CCompositor::cleanup() {
    if (!m_wlDisplay)
        return;

    writeWatchdogFd("end");

    signal(SIGABRT, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);

    removeLockFile();

    m_isShuttingDown = true;

#ifdef USES_SYSTEMD
    if (NSystemd::sdBooted() > 0 && !Env::envEnabled("HYPRLAND_NO_SD_NOTIFY"))
        NSystemd::sdNotify(0, "STOPPING=1");
#endif

    cleanEnvironment();

    // unload all remaining plugins while the compositor is
    // still in a normal working state.
    g_pPluginSystem->unloadAllPlugins();

    State::workspaceState()->clear();
    Desktop::windowState()->clear();
    Desktop::layerState()->clear();
    Desktop::fadingOutState()->clear();
    Desktop::otherViewState()->clear();

    for (auto const& m : State::monitorState()->monitors()) {
        g_pHyprOpenGL->destroyMonitorResources(m);
    }

    g_pXWayland.reset();

    wl_display_destroy_clients(g_pCompositor->m_wlDisplay);

    State::monitorState()->finish();
    State::fallbackState().reset();
    State::monitorState().reset();

    removeAllSignals();

    g_pInputManager.reset();
    g_pDynamicPermissionManager.reset();
    g_pDecorationPositioner.reset();
    Pointer::Cursor::mgr().reset();
    g_pPluginSystem.reset();
    Notification::overlay().reset();
    Debug::overlay().reset();
    g_pEventManager.reset();
    g_pSessionLockManager.reset();
    g_pHyprRenderer.reset();
    g_pProtocolManager.reset();
    g_pHyprOpenGL.reset();
    Render::g_pShaderLoader.reset();
    Config::mgr().reset();
    g_layoutManager.reset();
    ErrorOverlay::overlay().reset();
    g_pKeybindManager.reset();
    g_pXWaylandManager.reset();
    Pointer::mgr().reset();
    g_pSeatManager.reset();
    g_pHyprCtl.reset();
    g_pEventLoopManager.reset();
    g_pVersionKeeperMgr.reset();
    g_pDonationNagManager.reset();
    g_pWelcomeManager.reset();
    g_pANRManager.reset();
    Config::watcher().reset();
    g_pAsyncResourceGatherer.reset();

    if (m_aqBackend)
        m_aqBackend.reset();

    if (m_critSigSource)
        wl_event_source_remove(m_critSigSource);

    // this frees all wayland resources, including sockets
    wl_display_destroy(m_wlDisplay);
}

void CCompositor::initManagers(eManagersInitStage stage) {
    switch (stage) {
        case STAGE_PRIORITY: {
            Log::logger->log(Log::DEBUG, "Creating the EventLoopManager!");
            g_pEventLoopManager = makeUnique<CEventLoopManager>(m_wlDisplay, m_wlEventLoop);

            Log::logger->log(Log::DEBUG, "Creating the KeybindManager!");
            g_pKeybindManager = makeUnique<CKeybindManager>();

            Log::logger->log(Log::DEBUG, "Creating the AnimationManager!");
            Animation::mgr();

            Log::logger->log(Log::DEBUG, "Creating the DynamicPermissionManager!");
            g_pDynamicPermissionManager = makeUnique<CDynamicPermissionManager>();

            Log::logger->log(Log::DEBUG, "Creating the MonitorState!");
            State::monitorState();

            Log::logger->log(Log::DEBUG, "Creating the WorkspaceState!");
            State::workspaceState();

            Log::logger->log(Log::DEBUG, "Creating the ConfigManager!");
            if (!Config::initConfigManager())
                exit(1);

            Log::logger->log(Log::DEBUG, "Creating the Error Overlay!");
            ErrorOverlay::overlay();

            Log::logger->log(Log::DEBUG, "Creating the LayoutManager!");
            g_layoutManager = makeUnique<Layout::CLayoutManager>();

            Log::logger->log(Log::DEBUG, "Creating the TokenManager!");
            g_pTokenManager = makeUnique<CTokenManager>();

            // create executor
            Config::Supplementary::executor();

            Config::mgr()->init();

            Log::logger->log(Log::DEBUG, "Creating the PointerManager!");
            Pointer::mgr() = makeUnique<Pointer::CPointerManager>();

            Log::logger->log(Log::DEBUG, "Creating the EventManager!");
            g_pEventManager = makeUnique<CEventManager>();

            Log::logger->log(Log::DEBUG, "Creating the AsyncResourceGatherer!");
            g_pAsyncResourceGatherer = makeUnique<Hyprgraphics::CAsyncResourceGatherer>();
        } break;
        case STAGE_BASICINIT: {
            Log::logger->log(Log::DEBUG, "Creating the CHyprOpenGLImpl!");
            g_pHyprOpenGL = makeUnique<CHyprOpenGLImpl>();

            Log::logger->log(Log::DEBUG, "Creating the HyprRenderer!");
            g_pHyprRenderer = makeUnique<CHyprGLRenderer>();

            Log::logger->log(Log::DEBUG, "Creating the ProtocolManager!");
            g_pProtocolManager = makeUnique<CProtocolManager>();

            Log::logger->log(Log::DEBUG, "Creating the SeatManager!");
            g_pSeatManager = makeUnique<CSeatManager>();

            Log::logger->log(Log::DEBUG, "Creating the SessionLockManager!");
            g_pSessionLockManager = makeUnique<CSessionLockManager>();

            // init focus state els
            Desktop::History::windowTracker();
            Desktop::History::workspaceTracker();

            // init states
            Desktop::windowState();
            Desktop::layerState();
            Desktop::fadingOutState();
            Desktop::otherViewState();
            Desktop::viewState();
            State::fallbackState();

        } break;
        case STAGE_LATE: {
            Log::logger->log(Log::DEBUG, "Creating CHyprCtl");
            g_pHyprCtl = makeUnique<CHyprCtl>();

            Log::logger->log(Log::DEBUG, "Creating the InputManager!");
            g_pInputManager = makeUnique<CInputManager>();

            Log::logger->log(Log::DEBUG, "Creating the XWaylandManager!");
            g_pXWaylandManager = makeUnique<CHyprXWaylandManager>();

            Log::logger->log(Log::DEBUG, "Creating the Debug Overlay!");
            Debug::overlay();

            Log::logger->log(Log::DEBUG, "Creating the NotificationOverlay!");
            Notification::overlay();

            Log::logger->log(Log::DEBUG, "Creating the PluginSystem!");
            g_pPluginSystem = makeUnique<CPluginSystem>();
            Config::mgr()->handlePluginLoads();

            Log::logger->log(Log::DEBUG, "Creating the DecorationPositioner!");
            g_pDecorationPositioner = makeUnique<CDecorationPositioner>();

            Log::logger->log(Log::DEBUG, "Creating the CursorManager!");
            Pointer::Cursor::mgr() = makeUnique<Pointer::Cursor::CCursorManager>();

            Log::logger->log(Log::DEBUG, "Creating the VersionKeeper!");
            g_pVersionKeeperMgr = makeUnique<CVersionKeeperManager>();

            Log::logger->log(Log::DEBUG, "Creating the DonationNag!");
            g_pDonationNagManager = makeUnique<CDonationNagManager>();

            Log::logger->log(Log::DEBUG, "Creating the WelcomeManager!");
            g_pWelcomeManager = makeUnique<CWelcomeManager>();

            Log::logger->log(Log::DEBUG, "Creating the ANRManager!");
            g_pANRManager = makeUnique<CANRManager>();

            Log::logger->log(Log::DEBUG, "Starting XWayland");
            g_pXWayland = makeUnique<CXWayland>(g_pCompositor->m_wantsXwayland);
        } break;
        default: UNREACHABLE();
    }
}

void CCompositor::createLockFile() {
    const auto    PATH = m_instancePath + "/hyprland.lock";

    std::ofstream ofs(PATH, std::ios::trunc);

    ofs << m_hyprlandPID << "\n" << m_wlDisplaySocket << "\n";

    ofs.close();
}

void CCompositor::removeLockFile() {
    const auto PATH = m_instancePath + "/hyprland.lock";

    if (std::filesystem::exists(PATH))
        std::filesystem::remove(PATH);
}

void CCompositor::startCompositor() {
    signal(SIGPIPE, SIG_IGN);

    if (
        /* Session-less Hyprland usually means a nest, don't update the env in that case */
        m_aqBackend->hasSession() &&
        /* Activation environment management is not disabled */
        !Env::envEnabled("HYPRLAND_NO_SD_VARS")) {
        const auto CMD =
#ifdef USES_SYSTEMD
            "systemctl --user import-environment DISPLAY WAYLAND_DISPLAY HYPRLAND_INSTANCE_SIGNATURE XDG_CURRENT_DESKTOP QT_QPA_PLATFORMTHEME PATH XDG_DATA_DIRS && hash "
            "dbus-update-activation-environment 2>/dev/null && "
#endif
            "dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP HYPRLAND_INSTANCE_SIGNATURE QT_QPA_PLATFORMTHEME PATH XDG_DATA_DIRS";
        Config::Supplementary::executor()->spawn(CMD);
    }

    Log::logger->log(Log::DEBUG, "Running on WAYLAND_DISPLAY: {}", m_wlDisplaySocket);

    g_pHyprRenderer->setCursorFromName("left_ptr");

#ifdef USES_SYSTEMD
    if (NSystemd::sdBooted() > 0) {
        // tell systemd that we are ready so it can start other bond, following, related units
        if (!Env::envEnabled("HYPRLAND_NO_SD_NOTIFY"))
            NSystemd::sdNotify(0, "READY=1");
    } else
        Log::logger->log(Log::DEBUG, "systemd integration is baked in but system itself is not booted à la systemd!");
#endif

    createLockFile();

    Event::bus()->m_events.ready.emit();

    if (m_watchdogWriteFd.isValid()) {
        if (!writeWatchdogFd("vax"))
            Log::logger->log(Log::ERR, "startCompositor: failed to write to watchdogWriteFd {}: {}", m_watchdogWriteFd.get(), strerror(errno));
    }

    // This blocks until we are done.
    Log::logger->log(Log::DEBUG, "Hyprland is ready, running the event loop!");
    g_pEventLoopManager->enterLoop();
}

// returns a delta
Vector2D CCompositor::parseWindowVectorArgsRelative(const std::string& args, const Vector2D& relativeTo) {
    if (!args.contains(' ') && !args.contains('\t'))
        return relativeTo;

    const auto PMONITOR = Desktop::focusState()->monitor();

    bool       xIsPercent = false;
    bool       yIsPercent = false;
    bool       isExact    = false;

    CVarList2  varList(std::string{args}, 0, 's', true);
    auto       x = varList[0];
    auto       y = varList[1];

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

    if (!isNumber2(x) || !isNumber2(y)) {
        Log::logger->log(Log::ERR, "parseWindowVectorArgsRelative: args not numbers");
        return relativeTo;
    }

    int X = 0;
    int Y = 0;

    if (isExact) {
        X = xIsPercent ? *strToNumber<float>(x) * 0.01 * PMONITOR->m_size.x : *strToNumber<int64_t>(x);
        Y = yIsPercent ? *strToNumber<float>(y) * 0.01 * PMONITOR->m_size.y : *strToNumber<int64_t>(y);
    } else {
        X = xIsPercent ? (*strToNumber<float>(x) * 0.01 * relativeTo.x) + relativeTo.x : *strToNumber<int64_t>(x) + relativeTo.x;
        Y = yIsPercent ? (*strToNumber<float>(y) * 0.01 * relativeTo.y) + relativeTo.y : *strToNumber<int64_t>(y) + relativeTo.y;
    }

    return Vector2D(X, Y);
}

void CCompositor::performUserChecks() {
    static auto PNOCHECKXDG      = CConfigValue<Config::INTEGER>("misc:disable_xdg_env_checks");
    static auto PNOCHECKGUIUTILS = CConfigValue<Config::INTEGER>("misc:disable_hyprland_guiutils_check");
    static auto PNOWATCHDOG      = CConfigValue<Config::INTEGER>("misc:disable_watchdog_warning");

    if (!*PNOCHECKXDG) {
        const auto CURRENT_DESKTOP_ENV = getenv("XDG_CURRENT_DESKTOP");
        if (!CURRENT_DESKTOP_ENV || std::string{CURRENT_DESKTOP_ENV} != "Hyprland") {
            Notification::overlay()->addNotification(
                I18n::i18nEngine()->localize(I18n::TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP, {{"value", CURRENT_DESKTOP_ENV ? CURRENT_DESKTOP_ENV : "unset"}}), CHyprColor{}, 15000,
                ICON_WARNING);
        }
    }

    if (!*PNOCHECKGUIUTILS) {
        if (!NFsUtils::executableExistsInPath("hyprland-dialog"))
            Notification::overlay()->addNotification(I18n::i18nEngine()->localize(I18n::TXT_KEY_NOTIF_NO_GUIUTILS), CHyprColor{}, 15000, ICON_WARNING);
    }

    if (g_pHyprRenderer->m_failedAssetsNo > 0) {
        Notification::overlay()->addNotification(I18n::i18nEngine()->localize(I18n::TXT_KEY_NOTIF_FAILED_ASSETS, {{"count", std::to_string(g_pHyprRenderer->m_failedAssetsNo)}}),
                                                 CHyprColor{1.0, 0.1, 0.1, 1.0}, 15000, ICON_ERROR);
    }

    if (!m_watchdogWriteFd.isValid() && !*PNOWATCHDOG)
        Notification::overlay()->addNotification(I18n::i18nEngine()->localize(I18n::TXT_KEY_NOTIF_NO_WATCHDOG), CHyprColor{1.0, 0.1, 0.1, 1.0}, 15000, ICON_WARNING);

    if (m_safeMode)
        openSafeModeBox();
}

void CCompositor::openSafeModeBox() {
    const auto OPT_LOAD = I18n::i18nEngine()->localize(I18n::TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG);
    const auto OPT_OPEN = I18n::i18nEngine()->localize(I18n::TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR);
    const auto OPT_OK   = I18n::i18nEngine()->localize(I18n::TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD);

    auto       box = CAsyncDialogBox::create(I18n::i18nEngine()->localize(I18n::TXT_KEY_SAFE_MODE_TITLE), I18n::i18nEngine()->localize(I18n::TXT_KEY_SAFE_MODE_DESCRIPTION),
                                             {
                                                 OPT_LOAD,
                                                 OPT_OPEN,
                                                 OPT_OK,
                                             });

    if (!box) {
        Log::logger->log(Log::ERR, "CCompositor::openSafeModeBox: failed to create safe mode dialog");
        return;
    }

    const auto promise = box->open();
    if (!promise) {
        Log::logger->log(Log::ERR, "CCompositor::openSafeModeBox: failed to open safe mode dialog");
        return;
    }

    promise->then([OPT_LOAD, OPT_OK, OPT_OPEN, this](SP<CPromiseResult<std::string>> result) {
        if (result->hasError())
            return;

        const auto RES = result->result();

        if (RES.starts_with(OPT_LOAD)) {
            m_safeMode = false;
            Config::mgr()->reload();
        } else if (RES.starts_with(OPT_OPEN)) {
            std::string reportPath;
            const auto  HOME       = getenv("HOME");
            const auto  CACHE_HOME = getenv("XDG_CACHE_HOME");

            if (CACHE_HOME && CACHE_HOME[0] != '\0') {
                reportPath += CACHE_HOME;
                reportPath += "/hyprland/";
            } else if (HOME && HOME[0] != '\0') {
                reportPath += HOME;
                reportPath += "/.cache/hyprland/";
            }
            Hyprutils::OS::CProcess proc("xdg-open", {reportPath});

            proc.runAsync();

            // reopen
            openSafeModeBox();
        }
    });
}

PImageDescription CCompositor::getPreferredImageDescription() {
    if (!PROTO::colorManagement) {
        Log::logger->log(Log::ERR, "FIXME: color management protocol is not enabled, returning empty image description");
        return getDefaultImageDescription();
    }
    Log::logger->log(Log::WARN, "FIXME: color management protocol is enabled, determine correct preferred image description");
    // should determine some common settings to avoid unnecessary transformations while keeping maximum displayable precision
    return State::monitorState()->monitors().size() == 1 ? State::monitorState()->monitors()[0]->m_imageDescription :
                                                           CImageDescription::from(SImageDescription{.primaries = NColorPrimaries::BT709});
}

PImageDescription CCompositor::getHDRImageDescription() {
    if (!PROTO::colorManagement) {
        Log::logger->log(Log::ERR, "FIXME: color management protocol is not enabled, returning empty image description");
        return getDefaultImageDescription();
    }

    return State::monitorState()->monitors().size() == 1 && State::monitorState()->monitors()[0]->m_output &&
            State::monitorState()->monitors()[0]->m_output->parsedEDID.hdrMetadata.has_value() ?
        CImageDescription::from(SImageDescription{.transferFunction    = NColorManagement::CM_TRANSFER_FUNCTION_ST2084_PQ,
                                                  .primariesNameSet    = true,
                                                  .primariesNamed      = NColorManagement::CM_PRIMARIES_BT2020,
                                                  .primaries           = NColorManagement::getPrimaries(NColorManagement::CM_PRIMARIES_BT2020),
                                                  .masteringPrimaries  = State::monitorState()->monitors()[0]->getMasteringPrimaries(),
                                                  .luminances          = {.min       = State::monitorState()->monitors()[0]->minLuminance(HDR_MIN_LUMINANCE),
                                                                          .max       = State::monitorState()->monitors()[0]->maxLuminance(HDR_MAX_LUMINANCE),
                                                                          .reference = HDR_REF_LUMINANCE},
                                                  .masteringLuminances = State::monitorState()->monitors()[0]->getMasteringLuminances(),
                                                  .maxCLL              = State::monitorState()->monitors()[0]->maxCLL(),
                                                  .maxFALL             = State::monitorState()->monitors()[0]->maxFALL()}) :
        DEFAULT_HDR_IMAGE_DESCRIPTION;
}

bool CCompositor::shouldChangePreferredImageDescription() {
    Log::logger->log(Log::WARN, "FIXME: color management protocol is enabled and outputs changed, check preferred image description changes");
    return false;
}

std::optional<unsigned int> CCompositor::getVTNr() {
    if (!m_aqBackend->hasSession())
        return std::nullopt;

    unsigned int                   ttynum = 0;
    Hyprutils::OS::CFileDescriptor fd{open("/dev/tty", O_RDONLY | O_NOCTTY)};
    if (fd.isValid()) {
#if defined(VT_GETSTATE)
        struct vt_stat st;
        if (!ioctl(fd.get(), VT_GETSTATE, &st))
            ttynum = st.v_active;
#elif defined(VT_GETACTIVE)
        int vt;
        if (!ioctl(fd.get(), VT_GETACTIVE, &vt))
            ttynum = vt;
#endif
    }

    if (ttynum == 0)
        return std::nullopt;

    return ttynum;
}
