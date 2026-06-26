#include <ranges>

#include "Compositor.hpp"
#include "config/supplementary/executor/Executor.hpp"
#include "debug/log/Logger.hpp"
#include "desktop/DesktopTypes.hpp"
#include "desktop/state/FocusState.hpp"
#include "desktop/history/WindowHistoryTracker.hpp"
#include "desktop/history/WorkspaceHistoryTracker.hpp"
#include "desktop/view/Group.hpp"
#include "helpers/Splashes.hpp"
#include "helpers/SystemInfo.hpp"
#include "config/ConfigValue.hpp"
#include "config/legacy/ConfigManager.hpp"
#include "config/shared/inotify/ConfigWatcher.hpp"
#include "config/shared/monitor/MonitorRuleManager.hpp"
#include "managers/CursorManager.hpp"
#include "managers/TokenManager.hpp"
#include "managers/PointerManager.hpp"
#include "managers/SeatManager.hpp"
#include "managers/VersionKeeperManager.hpp"
#include "managers/DonationNagManager.hpp"
#include "managers/ANRManager.hpp"
#include "managers/eventLoop/EventLoopManager.hpp"
#include "managers/permissions/DynamicPermissionManager.hpp"
#include "managers/screenshare/ScreenshareManager.hpp"
#include "state/FallbackState.hpp"
#include "state/MonitorPositionController.hpp"
#include "state/MonitorState.hpp"
#include "state/WorkspaceState.hpp"
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
#include "debug/crash/CrashReporter.hpp"
#include "render/GLRenderer.hpp"
#include "render/ShaderLoader.hpp"
#ifdef USES_SYSTEMD
#include <helpers/SdDaemon.hpp> // for SdNotify
#endif
#include "helpers/fs/FsUtils.hpp"
#include "helpers/env/Env.hpp"
#include "protocols/FractionalScale.hpp"
#include "protocols/PointerConstraints.hpp"
#include "protocols/LayerShell.hpp"
#include "protocols/XDGShell.hpp"
#include "protocols/XDGOutput.hpp"
#include "protocols/SecurityContext.hpp"
#include "protocols/ColorManagement.hpp"
#include "protocols/core/Compositor.hpp"
#include "protocols/core/Subcompositor.hpp"
#include "desktop/view/LayerSurface.hpp"
#include "layout/space/Space.hpp"
#include "render/Renderer.hpp"
#include "xwayland/XWayland.hpp"
#include "helpers/ByteOperations.hpp"
#include "render/decorations/CHyprGroupBarDecoration.hpp"

#include "managers/KeybindManager.hpp"
#include "managers/SessionLockManager.hpp"
#include "managers/XWaylandManager.hpp"

#include "config/ConfigManager.hpp"
#include "config/shared/workspace/WorkspaceRuleManager.hpp"
#include "render/OpenGL.hpp"
#include "managers/input/InputManager.hpp"
#include "managers/animation/AnimationManager.hpp"
#include "managers/animation/DesktopAnimationManager.hpp"
#include "managers/EventManager.hpp"
#include "managers/ProtocolManager.hpp"
#include "managers/WelcomeManager.hpp"
#include "render/AsyncResourceGatherer.hpp"
#include "plugins/PluginSystem.hpp"
#include "errorOverlay/Overlay.hpp"
#include "notification/NotificationOverlay.hpp"
#include "debug/Overlay.hpp"
#include "output/MonitorFrameScheduler.hpp"
#include "i18n/Engine.hpp"
#include "layout/LayoutManager.hpp"
#include "layout/target/WindowTarget.hpp"
#include "event/EventBus.hpp"

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList2.hpp>
#include <hyprutils/string/Numeric.hpp>
#include <aquamarine/input/Input.hpp>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <malloc.h>
#include <unistd.h>
#include <xf86drm.h>

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
        g_pKeybindManager   = makeUnique<CKeybindManager>();
        g_pAnimationManager = makeUnique<CHyprAnimationManager>();
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
                if (g_pAnimationManager)
                    g_pAnimationManager->resetTickState();

                for (auto const& m : State::monitorState()->monitors()) {
                    m->m_activeMonitorRule = {}; // rules were lost
                }

                Config::monitorRuleMgr()->scheduleReload();
                g_pCursorManager->syncGsettings();
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

    if (m_watchdogWriteFd.isValid()) [[maybe_unused]]
        auto w = write(m_watchdogWriteFd.get(), "end", 3);

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

    removeAllSignals();

    g_pInputManager.reset();
    g_pDynamicPermissionManager.reset();
    g_pDecorationPositioner.reset();
    g_pCursorManager.reset();
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
    g_pPointerManager.reset();
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
            g_pAnimationManager = makeUnique<CHyprAnimationManager>();

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
            g_pPointerManager = makeUnique<CPointerManager>();

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
            g_pCursorManager = makeUnique<CCursorManager>();

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
        if (write(m_watchdogWriteFd.get(), "vax", 3) < 0)
            Log::logger->log(Log::ERR, "startCompositor: failed to write to watchdogWriteFd {}: {}", m_watchdogWriteFd.get(), strerror(errno));
    }

    // This blocks until we are done.
    Log::logger->log(Log::DEBUG, "Hyprland is ready, running the event loop!");
    g_pEventLoopManager->enterLoop();
}

bool CCompositor::isWindowActive(PHLWINDOW pWindow) {
    if (!Desktop::focusState()->window() && !Desktop::focusState()->surface())
        return false;

    if (!pWindow->m_isMapped)
        return false;

    const auto PSURFACE = pWindow->wlSurface()->resource();

    return PSURFACE == Desktop::focusState()->surface() || pWindow == Desktop::focusState()->window();
}

void CCompositor::changeWindowZOrder(PHLWINDOW pWindow, bool top) {
    if (!validMapped(pWindow))
        return;

    if (top)
        pWindow->m_createdOverFullscreen = true;
    else
        pWindow->m_createdOverFullscreen = false;

    pWindow->updateFullscreenInputState();
    *pWindow->alpha(WINDOW_ALPHA_FULLSCREEN) = pWindow->isBlockedByFullscreen() ? 0.F : 1.F;

    const auto& WINDOWS = Desktop::windowState()->windows();

    if (pWindow == (top ? WINDOWS.back() : WINDOWS.front()))
        return;

    auto moveToZ = [&](PHLWINDOW pw, bool top) -> void {
        if (top)
            Desktop::windowState()->moveToTop(pw);
        else
            Desktop::windowState()->moveToBottom(pw);

        if (pw->m_isMapped)
            g_pHyprRenderer->damageMonitor(pw->m_monitor.lock());
    };

    if (!pWindow->m_isX11)
        moveToZ(pWindow, top);
    else {
        // move X11 window stack

        std::vector<PHLWINDOW> toMove;

        auto                   x11Stack = [&](PHLWINDOW pw, bool top, auto&& x11Stack) -> void {
            if (top)
                toMove.emplace_back(pw);
            else
                toMove.insert(toMove.begin(), pw);

            for (auto const& w : WINDOWS) {
                if (w->m_isMapped && !w->isHidden() && w->m_isX11 && w->x11TransientFor() == pw && w != pw && std::ranges::find(toMove, w) == toMove.end()) {
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

PHLWINDOW CCompositor::getWindowInDirection(PHLWINDOW pWindow, Math::eDirection dir) {
    if (dir == Math::DIRECTION_DEFAULT)
        return nullptr;

    const auto PMONITOR = pWindow->m_monitor.lock();

    if (!PMONITOR)
        return nullptr; // ??

    const auto WINDOWIDEALBB = pWindow->isFullscreen() ? CBox{PMONITOR->m_position, PMONITOR->m_size} : pWindow->getWindowIdealBoundingBoxIgnoreReserved();
    const auto PWORKSPACE    = pWindow->m_workspace;

    if (!PWORKSPACE)
        return nullptr; // ??

    return getWindowInDirection(WINDOWIDEALBB, PWORKSPACE, dir, pWindow->m_isFloating, pWindow, pWindow->m_isFloating);
}

PHLWINDOW CCompositor::getWindowInDirection(const CBox& box, PHLWORKSPACE pWorkspace, Math::eDirection dir, bool floatingPreference, PHLWINDOW ignoreWindow, bool useVectorAngles) {
    if (dir == Math::DIRECTION_DEFAULT)
        return nullptr;

    // 0 -> history, 1 -> shared length
    static auto PMETHOD          = CConfigValue<Config::INTEGER>("binds:focus_preferred_method");
    static auto PMONITORFALLBACK = CConfigValue<Config::INTEGER>("binds:window_direction_monitor_fallback");

    const auto  POSA  = box.pos();
    const auto  SIZEA = box.size();

    auto        leaderValue  = -1;
    PHLWINDOW   leaderWindow = nullptr;

    if (!useVectorAngles) {
        // helper to check if two rectangles are adjacent along an axis, considering slight overlaps.
        // returns true if: STICKS (delta <= 2) OR rectangles overlap but no more than 50% of the smaller dimension.
        static auto isAdjacent = [](const double aMin, const double aMax, const double bMin, const double bMax) -> bool {
            constexpr double STICK_THRESHOLD   = 2.0;
            constexpr double MAX_OVERLAP_RATIO = 0.5;

            const double     aEdge = aMin;
            const double     bEdge = bMax;
            const double     delta = aEdge - bEdge;

            // old STICKS check for 2px
            if (std::abs(delta) < STICK_THRESHOLD)
                return true;

            if (delta >= 0)
                return false;

            const double overlap = -delta;
            const double sizeA   = aMax - aMin;
            const double sizeB   = bMax - bMin;

            // reject if one rectangle fully contains the other
            if ((bMin <= aMin && bMax >= aMax) || (aMin <= bMin && aMax >= bMax))
                return false;

            // accept if overlap is at most 50% of the smaller dimension
            return overlap <= std::min(sizeA, sizeB) * MAX_OVERLAP_RATIO;
        };

        auto find = [&]() {
            for (auto const& w : Desktop::windowState()->windows()) {
                if (w == ignoreWindow || !w->m_workspace || !w->m_isMapped || (!w->isFullscreen() && w->m_isFloating) || !w->m_workspace->isVisible())
                    continue;

                if (w->isHidden())
                    continue;

                // check if the input is blocked by anything except BELOW_FULLSCREEN
                if (w->isInputBlocked(INPUT_BLOCK_ALL & (~INPUT_BLOCK_BELOW_FULLSCREEN)))
                    continue;

                if (pWorkspace->m_monitor == w->m_monitor && pWorkspace != w->m_workspace)
                    continue;

                if (pWorkspace->m_hasFullscreenWindow && !w->isAllowedOverFullscreen())
                    continue;

                if (!*PMONITORFALLBACK && pWorkspace->m_monitor != w->m_monitor)
                    continue;

                if (w->m_isFloating != floatingPreference)
                    continue;

                // prioritize windows on the same workspace.
                // this is especially important for scrolling layouts - we want to first move to a window
                // on the same workspace before moving onto another.
                const auto LEADER_IS_ON_SAME_WORKSPACE = leaderWindow && leaderWindow->m_workspace == pWorkspace;

                if (LEADER_IS_ON_SAME_WORKSPACE && w->m_workspace != pWorkspace)
                    continue;

                const auto BWINDOWIDEALBB = w->getWindowIdealBoundingBoxIgnoreReserved();

                const auto POSB  = Vector2D(BWINDOWIDEALBB.x, BWINDOWIDEALBB.y);
                const auto SIZEB = Vector2D(BWINDOWIDEALBB.width, BWINDOWIDEALBB.height);

                double     intersectLength = -1;

                switch (dir) {
                    case Math::DIRECTION_LEFT:
                        if (isAdjacent(POSA.x, POSA.x + SIZEA.x, POSB.x, POSB.x + SIZEB.x))
                            intersectLength = std::max(0.0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                        break;
                    case Math::DIRECTION_RIGHT:
                        if (isAdjacent(POSB.x, POSB.x + SIZEB.x, POSA.x, POSA.x + SIZEA.x))
                            intersectLength = std::max(0.0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                        break;
                    case Math::DIRECTION_UP:
                        if (isAdjacent(POSA.y, POSA.y + SIZEA.y, POSB.y, POSB.y + SIZEB.y))
                            intersectLength = std::max(0.0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                        break;
                    case Math::DIRECTION_DOWN:
                        if (isAdjacent(POSB.y, POSB.y + SIZEB.y, POSA.y, POSA.y + SIZEA.y))
                            intersectLength = std::max(0.0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                        break;
                    default: break;
                }

                // if we have a leader on another workspace, and this window is on the same workspace,
                // override minimum requirements and always select this as the new leader
                const bool OVERRIDE_MIN_REQ = leaderWindow && !LEADER_IS_ON_SAME_WORKSPACE && w->m_workspace == pWorkspace;

                // ...as long as there is any intersect.
                if (intersectLength <= 1)
                    continue;

                if (*PMETHOD == 0 /* history */) {
                    // get idx
                    int         windowIDX = -1;
                    const auto& HISTORY   = Desktop::History::windowTracker()->fullHistory();
                    for (int64_t i = HISTORY.size() - 1; i >= 0; --i) {
                        if (HISTORY[i] == w) {
                            windowIDX = i;
                            break;
                        }
                    }

                    if (windowIDX > leaderValue || OVERRIDE_MIN_REQ) {
                        leaderValue  = windowIDX;
                        leaderWindow = w;
                    }
                } else /* length */ {
                    if (intersectLength > leaderValue || OVERRIDE_MIN_REQ) {
                        leaderValue  = intersectLength;
                        leaderWindow = w;
                    }
                }
            }
        };

        // Find the window, then if we don't find one with preferred
        // float status, try the opposite.
        find();

        if (!leaderWindow) {
            floatingPreference = !floatingPreference;
            find();
        }

    } else {
        static const std::unordered_map<Math::eDirection, Vector2D> VECTORS = {
            {Math::DIRECTION_RIGHT, {1, 0}}, {Math::DIRECTION_UP, {0, -1}}, {Math::DIRECTION_DOWN, {0, 1}}, {Math::DIRECTION_LEFT, {-1, 0}}};

        //
        auto vectorAngles = [](const Vector2D& a, const Vector2D& b) -> double {
            double dot = (a.x * b.x) + (a.y * b.y);
            double ang = std::acos(dot / (a.size() * b.size()));
            return ang;
        };

        float           bestAngleAbs = 2.0 * M_PI;
        constexpr float THRESHOLD    = 0.3 * M_PI;

        for (auto const& w : Desktop::windowState()->windows()) {
            if (w == ignoreWindow || !w->m_isMapped || !w->m_workspace || !w->acceptsInput() || (!w->isFullscreen() && !w->m_isFloating) || !w->m_workspace->isVisible())
                continue;

            if (pWorkspace->m_monitor == w->m_monitor && pWorkspace != w->m_workspace)
                continue;

            if (pWorkspace->m_hasFullscreenWindow && !w->isAllowedOverFullscreen())
                continue;

            if (!*PMONITORFALLBACK && pWorkspace->m_monitor != w->m_monitor)
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

        if (!leaderWindow && pWorkspace->m_hasFullscreenWindow)
            leaderWindow = pWorkspace->getFullscreenWindow();
    }

    if (leaderValue != -1)
        return leaderWindow;

    return nullptr;
}

template <typename WINDOWPTR>
static bool isWorkspaceMatches(WINDOWPTR pWindow, const WINDOWPTR w, bool anyWorkspace) {
    return anyWorkspace ? w->m_workspace && w->m_workspace->isVisible() : w->m_workspace == pWindow->m_workspace;
}

template <typename WINDOWPTR>
static bool isFloatingMatches(WINDOWPTR w, std::optional<bool> floating) {
    return !floating.has_value() || w->m_isFloating == floating.value();
}

template <typename WINDOWPTR>
static bool acceptsInputForCycle(WINDOWPTR w, bool allowFullscreenBlocked) {
    if (w->acceptsInput())
        return true;

    return allowFullscreenBlocked && !w->isHidden() && w->isInputBlockedOnly(INPUT_BLOCK_BELOW_FULLSCREEN);
}

template <typename WINDOWPTR>
static bool isWindowAvailableForCycle(WINDOWPTR pWindow, WINDOWPTR w, bool focusableOnly, std::optional<bool> floating, bool anyWorkspace = false,
                                      bool allowFullscreenBlocked = false) {
    return isFloatingMatches(w, floating) &&
        (w != pWindow && isWorkspaceMatches(pWindow, w, anyWorkspace) && w->m_isMapped && acceptsInputForCycle(w, allowFullscreenBlocked) &&
         (!focusableOnly || !w->m_ruleApplicator->noFocus().valueOrDefault()));
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

PHLWINDOW CCompositor::getWindowCycleHist(PHLWINDOWREF cur, bool focusableOnly, std::optional<bool> floating, bool visible, bool next, bool allowFullscreenBlocked) {
    const auto FINDER = [&](const PHLWINDOWREF& w) { return isWindowAvailableForCycle(cur, w, focusableOnly, floating, visible, allowFullscreenBlocked); };
    // also m_vWindowFocusHistory has reverse order, so when it is next - we need to reverse again
    const auto& HISTORY = Desktop::History::windowTracker()->fullHistory();
    return next ? getWeakWindowPred(std::ranges::find(HISTORY, cur), HISTORY.end(), HISTORY.begin(), FINDER) :
                  getWeakWindowPred(std::ranges::find(HISTORY | std::views::reverse, cur), HISTORY.rend(), HISTORY.rbegin(), FINDER);
}

PHLWINDOW CCompositor::getWindowCycle(PHLWINDOW cur, bool focusableOnly, std::optional<bool> floating, bool visible, bool prev, bool allowFullscreenBlocked) {
    const auto  FINDER  = [&](const PHLWINDOW& w) { return isWindowAvailableForCycle(cur, w, focusableOnly, floating, visible, allowFullscreenBlocked); };
    const auto& WINDOWS = Desktop::windowState()->windows();
    return prev ? getWindowPred(std::ranges::find(WINDOWS | std::views::reverse, cur), WINDOWS.rend(), WINDOWS.rbegin(), FINDER) :
                  getWindowPred(std::ranges::find(WINDOWS, cur), WINDOWS.end(), WINDOWS.begin(), FINDER);
}

bool CCompositor::isPointOnAnyMonitor(const Vector2D& point) {
    return std::ranges::any_of(State::monitorState()->monitors(), [&](const PHLMONITOR& m) {
        return VECINRECT(point, m->m_position.x, m->m_position.y, m->m_size.x + m->m_position.x, m->m_size.y + m->m_position.y);
    });
}

bool CCompositor::isPointOnReservedArea(const Vector2D& point, const PHLMONITOR pMonitor) {
    const auto PMONITOR = pMonitor ? pMonitor : State::monitorState()->query().vec(point).run();

    auto       box = PMONITOR->logicalBox();
    if (VECNOTINRECT(point, box.x - 1, box.y - 1, box.x + box.w + 1, box.y + box.h + 1))
        return false;

    PMONITOR->m_reservedArea.applyip(box);

    return VECNOTINRECT(point, box.x, box.y, box.x + box.w, box.y + box.h);
}

std::optional<CBox> CCompositor::calculateX11WorkArea() {
    static auto PXWLFORCESCALEZERO = CConfigValue<Config::INTEGER>("xwayland:force_zero_scaling");
    // We more than likely won't be able to calculate one
    // and even if we could this is minor
    if (State::monitorState()->monitors().size() > 1 || State::monitorState()->monitors().empty())
        return std::nullopt;

    const auto M = State::monitorState()->monitors().front();

    // we ignore monitor->m_position on purpose
    CBox box = M->logicalBoxMinusReserved().translate(-M->m_position);
    if ((*PXWLFORCESCALEZERO))
        box.scale(M->m_scale);

    return box.translate(M->m_xwaylandPosition);
}

void CCompositor::updateAllWindowsAnimatedDecorationValues() {
    for (auto const& w : Desktop::windowState()->windows()) {
        if (!w->m_isMapped)
            continue;

        w->updateDecorationValues();
    }
}

void CCompositor::swapActiveWorkspaces(PHLMONITOR pMonitorA, PHLMONITOR pMonitorB) {
    const auto PWORKSPACEA = pMonitorA->m_activeWorkspace;
    const auto PWORKSPACEB = pMonitorB->m_activeWorkspace;

    PWORKSPACEA->m_monitor = pMonitorB;
    PWORKSPACEA->m_events.monitorChanged.emit();

    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->m_workspace == PWORKSPACEA) {
            if (w->m_pinned) {
                w->m_workspace = PWORKSPACEB;
                continue;
            }

            w->m_monitor = pMonitorB;

            // additionally, move floating and fs windows manually
            if (w->m_isFloating)
                w->layoutTarget()->setPositionGlobal(w->layoutTarget()->position().translate(-pMonitorA->m_position + pMonitorB->m_position));

            if (w->isFullscreen()) {
                *w->m_realPosition = pMonitorB->m_position;
                *w->m_realSize     = pMonitorB->m_size;
            }

            w->updateToplevel();
        }
    }

    PWORKSPACEB->m_monitor = pMonitorA;
    PWORKSPACEB->m_events.monitorChanged.emit();

    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->m_workspace == PWORKSPACEB) {
            if (w->m_pinned) {
                w->m_workspace = PWORKSPACEA;
                continue;
            }

            w->m_monitor = pMonitorA;

            // additionally, move floating and fs windows manually
            if (w->m_isFloating)
                w->layoutTarget()->setPositionGlobal(w->layoutTarget()->position().translate(-pMonitorB->m_position + pMonitorA->m_position));

            if (w->isFullscreen()) {
                *w->m_realPosition = pMonitorA->m_position;
                *w->m_realSize     = pMonitorA->m_size;
            }

            w->updateToplevel();
        }
    }

    pMonitorA->m_activeWorkspace = PWORKSPACEB;
    pMonitorB->m_activeWorkspace = PWORKSPACEA;

    g_layoutManager->recalculateMonitor(pMonitorA);
    g_layoutManager->recalculateMonitor(pMonitorB);

    g_pHyprRenderer->damageMonitor(pMonitorB);
    g_pHyprRenderer->damageMonitor(pMonitorA);

    g_pDesktopAnimationManager->setFullscreenFadeAnimation(
        PWORKSPACEB, PWORKSPACEB->m_hasFullscreenWindow ? CDesktopAnimationManager::ANIMATION_TYPE_IN : CDesktopAnimationManager::ANIMATION_TYPE_OUT);
    g_pDesktopAnimationManager->setFullscreenFadeAnimation(
        PWORKSPACEA, PWORKSPACEA->m_hasFullscreenWindow ? CDesktopAnimationManager::ANIMATION_TYPE_IN : CDesktopAnimationManager::ANIMATION_TYPE_OUT);

    if (pMonitorA->m_id == Desktop::focusState()->monitor()->m_id || pMonitorB->m_id == Desktop::focusState()->monitor()->m_id) {
        const auto LASTWIN = pMonitorA->m_id == Desktop::focusState()->monitor()->m_id ? PWORKSPACEB->getLastFocusedWindow() : PWORKSPACEA->getLastFocusedWindow();
        Desktop::focusState()->fullWindowFocus(
            LASTWIN ? LASTWIN :
                      (Desktop::viewState()->hitTest().windowAt(g_pInputManager->getMouseCoordsInternal(),
                                                                Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING)),
            Desktop::FOCUS_REASON_DESKTOP_STATE_CHANGE);

        const auto PNEWWORKSPACE = pMonitorA->m_id == Desktop::focusState()->monitor()->m_id ? PWORKSPACEB : PWORKSPACEA;
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "workspace", .data = PNEWWORKSPACE->m_name});
        g_pEventManager->postEvent(SHyprIPCEvent{.event = "workspacev2", .data = std::format("{},{}", PNEWWORKSPACE->m_id, PNEWWORKSPACE->m_name)});
        Event::bus()->m_events.workspace.active.emit(PNEWWORKSPACE);
    }

    // events
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspace", .data = PWORKSPACEA->m_name + "," + pMonitorB->m_name});
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspacev2", .data = std::format("{},{},{}", PWORKSPACEA->m_id, PWORKSPACEA->m_name, pMonitorB->m_name)});
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspace", .data = PWORKSPACEB->m_name + "," + pMonitorA->m_name});
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspacev2", .data = std::format("{},{},{}", PWORKSPACEB->m_id, PWORKSPACEB->m_name, pMonitorA->m_name)});

    Event::bus()->m_events.workspace.moveToMonitor.emit(PWORKSPACEA, pMonitorB);
    Event::bus()->m_events.workspace.moveToMonitor.emit(PWORKSPACEB, pMonitorA);
}

void CCompositor::moveWorkspaceToMonitor(PHLWORKSPACE pWorkspace, PHLMONITOR pMonitor, bool noWarpCursor) {
    static auto PHIDESPECIALONWORKSPACECHANGE = CConfigValue<Config::INTEGER>("binds:hide_special_on_workspace_change");

    if (!pWorkspace || !pMonitor)
        return;

    if (pWorkspace->m_monitor == pMonitor)
        return;

    Log::logger->log(Log::DEBUG, "moveWorkspaceToMonitor: Moving {} to monitor {}", pWorkspace->m_id, pMonitor->m_id);

    const auto POLDMON = pWorkspace->m_monitor.lock();

    const bool SWITCHINGISACTIVE = POLDMON ? POLDMON->m_activeWorkspace == pWorkspace : false;

    // fix old mon
    WORKSPACEID nextWorkspaceOnMonitorID = WORKSPACE_INVALID;
    if (!SWITCHINGISACTIVE)
        nextWorkspaceOnMonitorID = pWorkspace->m_id;
    else {
        PHLWORKSPACE newWorkspace; // for holding a ref to the new workspace that might be created

        for (auto const& w : State::workspaceState()->workspaces()) {
            if (w->m_monitor == POLDMON && w->m_id != pWorkspace->m_id && !w->m_isSpecialWorkspace) {
                nextWorkspaceOnMonitorID = w->m_id;
                break;
            }
        }

        if (nextWorkspaceOnMonitorID == WORKSPACE_INVALID) {
            nextWorkspaceOnMonitorID = 1;

            while (State::workspaceState()->query().id(nextWorkspaceOnMonitorID).run() || [&]() -> bool {
                const auto B = Config::workspaceRuleMgr()->getBoundMonitorForWS(std::to_string(nextWorkspaceOnMonitorID));
                return B && B != POLDMON;
            }())
                nextWorkspaceOnMonitorID++;

            Log::logger->log(Log::DEBUG, "moveWorkspaceToMonitor: Plugging gap with new {}", nextWorkspaceOnMonitorID);

            if (POLDMON)
                newWorkspace = State::workspaceState()->create(nextWorkspaceOnMonitorID, POLDMON->m_id);
        }

        Log::logger->log(Log::DEBUG, "moveWorkspaceToMonitor: Plugging gap with existing {}", nextWorkspaceOnMonitorID);
        if (POLDMON)
            POLDMON->changeWorkspace(nextWorkspaceOnMonitorID, false, true, true);
    }

    // move the workspace
    pWorkspace->m_monitor = pMonitor;
    pWorkspace->m_space->recheckWorkArea();
    pWorkspace->m_events.monitorChanged.emit();

    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->m_workspace == pWorkspace) {
            if (w->m_pinned) {
                w->m_workspace = State::workspaceState()->query().id(nextWorkspaceOnMonitorID).run();
                continue;
            }

            w->m_monitor = pMonitor;

            // additionally, move floating and fs windows manually
            if (w->m_isMapped && !w->isHidden()) {
                if (POLDMON) {
                    if (w->m_isFloating)
                        w->layoutTarget()->setPositionGlobal(w->layoutTarget()->position().translate(-POLDMON->m_position + pMonitor->m_position));

                    if (w->isFullscreen()) {
                        *w->m_realPosition = pMonitor->m_position;
                        *w->m_realSize     = pMonitor->m_size;
                    }
                } else
                    w->layoutTarget()->setPositionGlobal(CBox{Vector2D{
                                                                  (pMonitor->m_size.x != 0) ? sc<int>(w->m_realPosition->goal().x) % sc<int>(pMonitor->m_size.x) : 0,
                                                                  (pMonitor->m_size.y != 0) ? sc<int>(w->m_realPosition->goal().y) % sc<int>(pMonitor->m_size.y) : 0,
                                                              },
                                                              w->layoutTarget()->position().size()});
            }

            w->updateToplevel();
        }
    }

    if (SWITCHINGISACTIVE && POLDMON == Desktop::focusState()->monitor()) { // if it was active, preserve its' status. If it wasn't, don't.
        Log::logger->log(Log::DEBUG, "moveWorkspaceToMonitor: SWITCHINGISACTIVE, active {} -> {}", pMonitor->activeWorkspaceID(), pWorkspace->m_id);

        if (valid(pMonitor->m_activeWorkspace)) {
            pMonitor->m_activeWorkspace->m_visible = false;
            g_pDesktopAnimationManager->startAnimation(pWorkspace, CDesktopAnimationManager::ANIMATION_TYPE_OUT, false);
        }

        if (*PHIDESPECIALONWORKSPACECHANGE)
            pMonitor->setSpecialWorkspace(nullptr);

        Desktop::focusState()->rawMonitorFocus(pMonitor);

        auto oldWorkspace           = pMonitor->m_activeWorkspace;
        pMonitor->m_activeWorkspace = pWorkspace;

        if (oldWorkspace)
            oldWorkspace->m_events.activeChanged.emit();

        pWorkspace->m_events.activeChanged.emit();

        g_layoutManager->recalculateMonitor(pMonitor);
        g_pHyprRenderer->damageMonitor(pMonitor);

        g_pDesktopAnimationManager->startAnimation(pWorkspace, CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);
        pWorkspace->m_visible = true;

        if (!noWarpCursor)
            g_pPointerManager->warpTo(pMonitor->m_position + pMonitor->m_transformedSize / 2.F);

        g_pInputManager->sendMotionEventsToFocused();
    }

    // finalize
    if (POLDMON) {
        g_layoutManager->recalculateMonitor(POLDMON);
        if (valid(POLDMON->m_activeWorkspace))
            g_pDesktopAnimationManager->setFullscreenFadeAnimation(POLDMON->m_activeWorkspace,
                                                                   POLDMON->m_activeWorkspace->m_hasFullscreenWindow ? CDesktopAnimationManager::ANIMATION_TYPE_IN :
                                                                                                                       CDesktopAnimationManager::ANIMATION_TYPE_OUT);
        updateSuspendedStates();
    }

    g_pDesktopAnimationManager->setFullscreenFadeAnimation(
        pWorkspace, pWorkspace->m_hasFullscreenWindow ? CDesktopAnimationManager::ANIMATION_TYPE_IN : CDesktopAnimationManager::ANIMATION_TYPE_OUT);
    updateSuspendedStates();

    // event
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspace", .data = pWorkspace->m_name + "," + pMonitor->m_name});
    g_pEventManager->postEvent(SHyprIPCEvent{.event = "moveworkspacev2", .data = std::format("{},{},{}", pWorkspace->m_id, pWorkspace->m_name, pMonitor->m_name)});

    Event::bus()->m_events.workspace.moveToMonitor.emit(pWorkspace, pMonitor);
}

void CCompositor::changeWindowFullscreenModeClient(const PHLWINDOW PWINDOW, const eFullscreenMode MODE, const bool ON) {
    setWindowFullscreenClient(
        PWINDOW,
        sc<eFullscreenMode>(ON ? sc<uint8_t>(PWINDOW->m_fullscreenState.client) | sc<uint8_t>(MODE) : (sc<uint8_t>(PWINDOW->m_fullscreenState.client) & sc<uint8_t>(~MODE))));
}

// TODO: move fs functions to Desktop::
void CCompositor::setWindowFullscreenInternal(const PHLWINDOW PWINDOW, const eFullscreenMode MODE) {
    if (!PWINDOW)
        return;
    if (PWINDOW->m_ruleApplicator->syncFullscreen().valueOrDefault())
        setWindowFullscreenState(PWINDOW, Desktop::View::SFullscreenState{.internal = MODE, .client = MODE});
    else
        setWindowFullscreenState(PWINDOW, Desktop::View::SFullscreenState{.internal = MODE, .client = PWINDOW->m_fullscreenState.client});
}

void CCompositor::setWindowFullscreenClient(const PHLWINDOW PWINDOW, const eFullscreenMode MODE) {
    if (PWINDOW->m_ruleApplicator->syncFullscreen().valueOrDefault())
        setWindowFullscreenState(PWINDOW, Desktop::View::SFullscreenState{.internal = MODE, .client = MODE});
    else
        setWindowFullscreenState(PWINDOW, Desktop::View::SFullscreenState{.internal = PWINDOW->m_fullscreenState.internal, .client = MODE});
}

void CCompositor::setWindowFullscreenState(const PHLWINDOW PWINDOW, Desktop::View::SFullscreenState state) {
    static auto PDIRECTSCANOUT      = CConfigValue<Config::INTEGER>("render:direct_scanout");
    static auto PALLOWPINFULLSCREEN = CConfigValue<Config::INTEGER>("binds:allow_pin_fullscreen");

    if (!validMapped(PWINDOW))
        return;

    state.internal = std::clamp(state.internal, sc<eFullscreenMode>(0), FSMODE_MAX);
    state.client   = std::clamp(state.client, sc<eFullscreenMode>(0), FSMODE_MAX);

    const auto PMONITOR   = PWINDOW->m_monitor.lock();
    const auto PWORKSPACE = PWINDOW->m_workspace;

    if (PWINDOW->m_isFloating && PWINDOW->m_fullscreenState.internal == FSMODE_NONE && state.internal != FSMODE_NONE)
        g_pHyprRenderer->damageWindow(PWINDOW);

    if (*PALLOWPINFULLSCREEN && !PWINDOW->m_pinFullscreened && !PWINDOW->isFullscreen() && PWINDOW->m_pinned) {
        PWINDOW->m_pinned          = false;
        PWINDOW->m_pinFullscreened = true;
    }

    if (PWORKSPACE->m_hasFullscreenWindow && !PWINDOW->isFullscreen())
        setWindowFullscreenInternal(PWORKSPACE->getFullscreenWindow(), FSMODE_NONE);

    const bool CHANGEINTERNAL = !PWINDOW->m_pinned && PWINDOW->m_fullscreenState.internal != state.internal;

    if (*PALLOWPINFULLSCREEN && PWINDOW->m_pinFullscreened && PWINDOW->isFullscreen() && !PWINDOW->m_pinned && state.internal == FSMODE_NONE) {
        PWINDOW->m_pinned          = true;
        PWINDOW->m_pinFullscreened = false;
    }

    // TODO: update the state on syncFullscreen changes
    if (!CHANGEINTERNAL && PWINDOW->m_ruleApplicator->syncFullscreen().valueOrDefault())
        return;

    PWINDOW->m_fullscreenState.client = state.client;
    g_pXWaylandManager->setWindowFullscreen(PWINDOW, state.client & FSMODE_FULLSCREEN);

    if (!CHANGEINTERNAL) {
        PWINDOW->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_FULLSCREEN | Desktop::Rule::RULE_PROP_FULLSCREENSTATE_CLIENT |
                                                     Desktop::Rule::RULE_PROP_FULLSCREENSTATE_INTERNAL | Desktop::Rule::RULE_PROP_ON_WORKSPACE);
        PWINDOW->updateDecorationValues();
        g_layoutManager->recalculateMonitor(PMONITOR);
        return;
    }

    // "Effective mode" is the fullscreen mode according to which a window is rendered.
    // For fullscreen modes `FSMODE_NONE` (0), `FSMODE_MAXIMIZED` (1), and `FSMODE_FULLSCREEN` (2),
    // the effective mode is the same as the fullscreen mode;
    // for fullscreen mode `FSMODE_MAXIMIZED|FSMODE_FULLSCREEN` (a window is maximized then fullscreened),
    // the effective mode is `FSMODE_FULLSCREEN` (2), since the window is rendered as a fullscreen window.
    // But when the latter window exists fullscreen, it will return to `FSMODE_MAXIMIZED`, rather than `FSMODE_NONE`.
    const eFullscreenMode OLD_EFFECTIVE_MODE = sc<eFullscreenMode>(std::bit_floor(sc<uint8_t>(PWINDOW->m_fullscreenState.internal)));
    const eFullscreenMode NEW_EFFECTIVE_MODE = sc<eFullscreenMode>(std::bit_floor(sc<uint8_t>(state.internal)));

    PWORKSPACE->m_fullscreenMode      = NEW_EFFECTIVE_MODE;
    PWORKSPACE->m_hasFullscreenWindow = NEW_EFFECTIVE_MODE != FSMODE_NONE;

    PWORKSPACE->setNoMembersAboveFullscreen();

    const auto FULLSCREEN_REQUEST_RESULT = g_layoutManager->fullscreenRequestForTarget(PWINDOW->layoutTarget(), OLD_EFFECTIVE_MODE, NEW_EFFECTIVE_MODE);
    const bool LAYOUT_HANDLED_FULLSCREEN = FULLSCREEN_REQUEST_RESULT == Layout::FULLSCREEN_REQUEST_HANDLED_BY_LAYOUT;

    if (LAYOUT_HANDLED_FULLSCREEN) {
        PWORKSPACE->m_fullscreenMode      = FSMODE_NONE;
        PWORKSPACE->m_hasFullscreenWindow = false;
    } else
        PWINDOW->m_fullscreenState.internal = state.internal;

    g_pEventManager->postEvent(SHyprIPCEvent{.event = "fullscreen", .data = std::to_string(sc<int>(NEW_EFFECTIVE_MODE) != FSMODE_NONE)});
    Event::bus()->m_events.window.fullscreen.emit(PWINDOW);

    PWINDOW->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_FULLSCREEN | Desktop::Rule::RULE_PROP_FULLSCREENSTATE_CLIENT |
                                                 Desktop::Rule::RULE_PROP_FULLSCREENSTATE_INTERNAL | Desktop::Rule::RULE_PROP_ON_WORKSPACE);

    PWINDOW->updateDecorationValues();
    g_layoutManager->recalculateMonitor(PMONITOR, Layout::CLayoutManager::RECALCULATE_MONITOR_REASON_TOGGLE_FULLSCREEN);

    // make all windows and layers on the same workspace under the fullscreen window
    for (auto const& w : Desktop::windowState()->windows()) {
        if (w->m_workspace == PWORKSPACE) {
            if (!w->isFullscreen() && !w->m_fadingOut && !w->m_pinned)
                w->m_createdOverFullscreen = false;

            w->updateFullscreenInputState();
        }
    }
    for (auto const& ls : Desktop::layerState()->layers()) {
        if (ls->m_monitor == PMONITOR)
            ls->m_aboveFullscreen = false;
    }

    if (!LAYOUT_HANDLED_FULLSCREEN)
        g_pDesktopAnimationManager->setFullscreenFadeAnimation(
            PWORKSPACE, PWORKSPACE->m_hasFullscreenWindow ? CDesktopAnimationManager::ANIMATION_TYPE_IN : CDesktopAnimationManager::ANIMATION_TYPE_OUT);

    PWINDOW->sendWindowSize(true);

    // recheck the work area again because visibility checks report 1 window on fs / maximize as tiled + visible
    // because the windows below fs are not visible obviously but because we update fullscreen fade which sets that
    // state later, it does it wrong
    PWORKSPACE->updateWindows();
    PWORKSPACE->m_space->recalculate(FULLSCREEN_REQUEST_RESULT == Layout::FULLSCREEN_REQUEST_DEFAULT ? Layout::RECALCULATE_REASON_TOGGLE_DEFAULT_HANDLED_FULLSCREEN :
                                                                                                       Layout::RECALCULATE_REASON_TOGGLE_LAYOUT_HANDLED_FULLSCREEN);
    PWORKSPACE->forceReportSizesToWindows();

    g_pInputManager->recheckIdleInhibitorStatus();

    // further updates require a monitor
    if (!PMONITOR)
        return;

    // send a scanout tranche if we are entering fullscreen, and send a regular one if we aren't.
    // ignore if DS is disabled.
    if (!LAYOUT_HANDLED_FULLSCREEN && (*PDIRECTSCANOUT == 1 || (*PDIRECTSCANOUT == 2 && PWINDOW->getContentType() == CONTENT_TYPE_GAME))) {
        auto surf = PWINDOW->getSolitaryResource();
        if (surf)
            g_pHyprRenderer->setSurfaceScanoutMode(surf, NEW_EFFECTIVE_MODE != FSMODE_NONE ? PMONITOR->m_self.lock() : nullptr);
    }

    Config::monitorRuleMgr()->ensureVRR(PMONITOR);
}

PHLWINDOW CCompositor::getX11Parent(PHLWINDOW pWindow) {
    if (!pWindow->m_isX11)
        return nullptr;

    for (auto const& w : Desktop::windowState()->windows()) {
        if (!w->m_isX11)
            continue;

        if (w->m_xwaylandSurface == pWindow->m_xwaylandSurface->m_parent)
            return w;
    }

    return nullptr;
}

void CCompositor::warpCursorTo(const Vector2D& pos, bool force) {

    // warpCursorTo should only be used for warps that
    // should be disabled with no_warps

    static auto PNOWARPS = CConfigValue<Config::INTEGER>("cursor:no_warps");

    if (*PNOWARPS && !force) {
        const auto PMONITORNEW = State::monitorState()->query().vec(pos).run();
        Desktop::focusState()->rawMonitorFocus(PMONITORNEW);
        return;
    }

    g_pPointerManager->warpTo(pos);

    const auto PMONITORNEW = State::monitorState()->query().vec(pos).run();
    Desktop::focusState()->rawMonitorFocus(PMONITORNEW);
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

    box->open()->then([OPT_LOAD, OPT_OK, OPT_OPEN, this](SP<CPromiseResult<std::string>> result) {
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

void CCompositor::moveWindowToWorkspaceSafe(PHLWINDOW pWindow, PHLWORKSPACE pWorkspace) {
    if (!pWindow || !pWorkspace)
        return;

    if (pWindow->m_pinned && pWorkspace->m_isSpecialWorkspace)
        return;

    if (pWindow->m_workspace == pWorkspace)
        return;

    const bool FULLSCREEN     = pWindow->isFullscreen();
    const auto FULLSCREENMODE = pWindow->m_fullscreenState.internal;
    const bool WASVISIBLE     = pWindow->m_workspace && pWindow->m_workspace->isVisible();

    if (FULLSCREEN)
        setWindowFullscreenInternal(pWindow, FSMODE_NONE);

    const PHLWINDOW pFirstWindowOnWorkspace   = pWorkspace->getFirstWindow();
    const int       visibleWindowsOnWorkspace = pWorkspace->getWindows(true, std::nullopt, true);
    const auto      POSTOMON                  = pWindow->m_realPosition->goal() - (pWindow->m_monitor ? pWindow->m_monitor->m_position : Vector2D{});
    const auto      PWORKSPACEMONITOR         = pWorkspace->m_monitor.lock();

    pWindow->moveToWorkspace(pWorkspace);
    pWindow->m_monitor = pWorkspace->m_monitor;

    static auto PGROUPONMOVETOWORKSPACE = CConfigValue<Config::INTEGER>("group:group_on_movetoworkspace");
    if (*PGROUPONMOVETOWORKSPACE && visibleWindowsOnWorkspace == 1 && pFirstWindowOnWorkspace && pFirstWindowOnWorkspace != pWindow && pFirstWindowOnWorkspace->m_group &&
        pWindow->canBeGroupedInto(pFirstWindowOnWorkspace->m_group)) {
        pFirstWindowOnWorkspace->m_group->add(pWindow);
    } else {
        if (pWindow->m_isFloating)
            pWindow->layoutTarget()->setPositionGlobal(CBox{POSTOMON + PWORKSPACEMONITOR->m_position, pWindow->layoutTarget()->position().size()});
    }

    pWindow->updateToplevel();
    pWindow->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_ON_WORKSPACE);
    pWindow->uncacheWindowDecos();

    if (pWindow->m_group)
        pWindow->m_group->updateWorkspace(pWorkspace);

    g_layoutManager->newTarget(pWindow->layoutTarget(), pWorkspace->m_space);

    if (FULLSCREEN)
        setWindowFullscreenInternal(pWindow, FULLSCREENMODE);

    pWorkspace->updateWindows();
    if (pWindow->m_workspace)
        pWindow->m_workspace->updateWindows();
    g_pCompositor->updateSuspendedStates();

    if (!WASVISIBLE && pWindow->m_workspace && pWindow->m_workspace->isVisible()) {
        pWindow->alpha(WINDOW_ALPHA_MOVE_FROM_WORKSPACE)->setValueAndWarp(0.F);
        *pWindow->alpha(WINDOW_ALPHA_MOVE_FROM_WORKSPACE) = 1.F;
    }
}

void CCompositor::setPreferredScaleForSurface(SP<CWLSurfaceResource> pSurface, double scale) {
    PROTO::fractional->sendScale(pSurface, scale);
    pSurface->sendPreferredScale(std::ceil(scale));

    const auto PSURFACE = Desktop::View::CWLSurface::fromResource(pSurface);
    if (!PSURFACE) {
        Log::logger->log(Log::WARN, "Orphaned CWLSurfaceResource {:x} in setPreferredScaleForSurface", rc<uintptr_t>(pSurface.get()));
        return;
    }

    PSURFACE->m_lastScaleFloat = scale;
    PSURFACE->m_lastScaleInt   = sc<int32_t>(std::ceil(scale));
}

void CCompositor::setPreferredTransformForSurface(SP<CWLSurfaceResource> pSurface, wl_output_transform transform) {
    pSurface->sendPreferredTransform(transform);

    const auto PSURFACE = Desktop::View::CWLSurface::fromResource(pSurface);
    if (!PSURFACE) {
        Log::logger->log(Log::WARN, "Orphaned CWLSurfaceResource {:x} in setPreferredTransformForSurface", rc<uintptr_t>(pSurface.get()));
        return;
    }

    PSURFACE->m_lastTransform = transform;
}

void CCompositor::updateSuspendedStates() {
    for (auto const& w : Desktop::windowState()->windows()) {
        if (!w->m_isMapped)
            continue;

        w->setSuspended(w->isHidden() || !w->m_workspace || !w->m_workspace->isVisible());
    }
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

    return ttynum;
}

bool CCompositor::isVRRActiveOnAnyMonitor() const {
    return std::ranges::any_of(State::monitorState()->monitors(), [](const PHLMONITOR& m) { return m->m_vrrActive; });
}
