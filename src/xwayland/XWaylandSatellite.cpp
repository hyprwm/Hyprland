#ifdef USE_XWAYLAND_SATELLITE

#include "XWaylandSatellite.hpp"
#include "XWayland.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../debug/log/Logger.hpp"

#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <filesystem>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

using namespace Hyprutils::OS;

// Constants
constexpr int SOCKET_DIR_PERMISSIONS = 0755;
constexpr int SOCKET_BACKLOG         = 1;
constexpr int MAX_DISPLAY_RETRIES    = 50;
constexpr int LOCK_FILE_MODE         = 0444;

static bool   safeRemove(const std::string& path) {
    try {
        return std::filesystem::remove(path);
    } catch (const std::exception& e) { Log::logger->log(Log::ERR, "[XWayland-Satellite] Failed to remove {}", path); }

    return false;
}

static std::string getSocketPath(int display, bool isLinux) {
    if (isLinux)
        return std::format("/tmp/.X11-unix/X{}", display);

    return std::format("/tmp/.X11-unix/X{}_", display);
}

static CFileDescriptor createSocket(struct sockaddr_un* addr, size_t pathSize) {
    const bool        isRegularSocket(addr->sun_path[0]);
    const char        dbgSocketPathPrefix = isRegularSocket ? addr->sun_path[0] : '@';
    const char* const dbgSocketPathRem    = addr->sun_path + 1;

    socklen_t         size = offsetof(struct sockaddr_un, sun_path) + pathSize + 1;
    CFileDescriptor   fd{socket(AF_UNIX, SOCK_STREAM, 0)};
    if (!fd.isValid()) {
        Log::logger->log(Log::ERR, "[XWayland-Satellite] Failed to create socket {}{}", dbgSocketPathPrefix, dbgSocketPathRem);
        return {};
    }

    if (!fd.setFlags(fd.getFlags() | FD_CLOEXEC)) {
        Log::logger->log(Log::ERR, "[XWayland-Satellite] Failed to set flags for socket {}{}", dbgSocketPathPrefix, dbgSocketPathRem);
        return {};
    }

    if (isRegularSocket)
        unlink(addr->sun_path);

    if (bind(fd.get(), rc<struct sockaddr*>(addr), size) < 0) {
        Log::logger->log(Log::ERR, "[XWayland-Satellite] Failed to bind socket {}{}", dbgSocketPathPrefix, dbgSocketPathRem);
        if (isRegularSocket)
            unlink(addr->sun_path);
        return {};
    }

    if (isRegularSocket && chmod(addr->sun_path, 0666) < 0)
        Log::logger->log(Log::ERR, "[XWayland-Satellite] Failed to set permission mode for socket {}{}", dbgSocketPathPrefix, dbgSocketPathRem);

    if (listen(fd.get(), SOCKET_BACKLOG) < 0) {
        Log::logger->log(Log::ERR, "[XWayland-Satellite] Failed to listen to socket {}{}", dbgSocketPathPrefix, dbgSocketPathRem);
        if (isRegularSocket)
            unlink(addr->sun_path);
        return {};
    }

    return fd;
}

static bool checkPermissionsForSocketDir() {
    struct stat buf;

    if (lstat("/tmp/.X11-unix", &buf)) {
        Log::logger->log(Log::ERR, "[XWayland-Satellite] Failed to stat X11 socket dir");
        return false;
    }

    if (!(buf.st_mode & S_IFDIR)) {
        Log::logger->log(Log::ERR, "[XWayland-Satellite] X11 socket dir is not a directory");
        return false;
    }

    if ((buf.st_uid != 0) && (buf.st_uid != getuid())) {
        Log::logger->log(Log::ERR, "[XWayland-Satellite] X11 socket dir is not owned by root or current user");
        return false;
    }

    if (!(buf.st_mode & S_ISVTX)) {
        if ((buf.st_mode & (S_IWGRP | S_IWOTH))) {
            Log::logger->log(Log::ERR, "[XWayland-Satellite] X11 socket dir is writable by others without sticky bit");
            return false;
        }
    }

    return true;
}

static bool ensureSocketDirExists() {
    if (mkdir("/tmp/.X11-unix", SOCKET_DIR_PERMISSIONS) != 0) {
        if (errno == EEXIST)
            return checkPermissionsForSocketDir();

        Log::logger->log(Log::ERR, "[XWayland-Satellite] Couldn't create socket dir /tmp/.X11-unix");
        return false;
    }

    return true;
}

static bool openSockets(std::array<CFileDescriptor, 2>& sockets, int display) {
    static auto CREATEABSTRACTSOCKET = CConfigValue<Config::INTEGER>("xwayland:create_abstract_socket");

    if (!ensureSocketDirExists())
        return false;

    sockaddr_un addr = {.sun_family = AF_UNIX};
    std::string path;

#ifdef __linux__
    if (*CREATEABSTRACTSOCKET) {
        addr.sun_path[0] = '\0';
        path             = getSocketPath(display, true);

        strncpy(addr.sun_path + 1, path.c_str(), sizeof(addr.sun_path) - 2);
    } else {
        path = getSocketPath(display, false);

        strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
    }
#else
    if (*CREATEABSTRACTSOCKET)
        Log::logger->log(Log::WARN, "[XWayland-Satellite] Abstract X11 socket is only available on Linux. A regular one will be created instead.");

    path = getSocketPath(display, false);

    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
#endif

    sockets[0] = CFileDescriptor{createSocket(&addr, path.length())};
    if (!sockets[0].isValid())
        return false;

    path = getSocketPath(display, true);
    strncpy(addr.sun_path, path.c_str(), path.length() + 1);

    sockets[1] = CFileDescriptor{createSocket(&addr, path.length())};
    if (!sockets[1].isValid()) {
        sockets[0].reset();
        return false;
    }

    return true;
}

CXWaylandSatellite::CXWaylandSatellite() {
    ;
}

CXWaylandSatellite::~CXWaylandSatellite() {
    removeWatches();

    if (m_display < 0)
        return;

    std::string lockPath = std::format("/tmp/.X{}-lock", m_display);
    safeRemove(lockPath);

    std::string path;
    for (bool isLinux : {true, false}) {
        path = getSocketPath(m_display, isLinux);
        safeRemove(path);
    }

    unsetenv("DISPLAY");
}

bool CXWaylandSatellite::tryOpenSockets() {
    for (int i = 0; i <= MAX_DISPLAY_RETRIES; ++i) {
        const std::string lockPath = std::format("/tmp/.X{}-lock", i);

        CFileDescriptor   fd{open(lockPath.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, LOCK_FILE_MODE)};
        if (fd.isValid()) {
            if (!openSockets(m_xFDs, i)) {
                safeRemove(lockPath);
                continue;
            }

            const std::string pidStr = std::format("{:010d}\n", getpid());
            if (write(fd.get(), pidStr.c_str(), 11) != 11L) {
                m_xFDs[0].reset();
                m_xFDs[1].reset();
                safeRemove(lockPath);
                continue;
            }

            m_display     = i;
            m_displayName = std::format(":{}", m_display);
            break;
        }

        fd = CFileDescriptor{open(lockPath.c_str(), O_RDONLY | O_CLOEXEC)};
        if (!fd.isValid())
            continue;

        char pidstr[12] = {0};
        if (read(fd.get(), pidstr, sizeof(pidstr) - 1) < 0)
            continue;

        int32_t pid = 0;
        try {
            pid = std::stoi(std::string{pidstr, 11});
        } catch (...) { continue; }

        if (kill(pid, 0) != 0 && errno == ESRCH) {
            if (!safeRemove(lockPath))
                continue;
            i--;
        }
    }

    if (m_display < 0) {
        Log::logger->log(Log::ERR, "[XWayland-Satellite] Failed to find a suitable display socket");
        return false;
    }

    Log::logger->log(Log::DEBUG, "[XWayland-Satellite] Found suitable display socket at DISPLAY: {}", m_displayName);
    return true;
}

bool CXWaylandSatellite::testOnDemand() {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        Log::logger->log(Log::ERR, "[XWayland-Satellite] pipe failed for test: {}", strerror(errno));
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        Log::logger->log(Log::ERR, "[XWayland-Satellite] fork failed for test: {}", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    if (pid == 0) {
        close(pipefd[0]);

        struct sigaction act;
        act.sa_handler = SIG_DFL;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        sigaction(SIGCHLD, &act, nullptr);

        pid_t pid2 = fork();
        if (pid2 == 0) {
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }

            unsetenv("DISPLAY");
            unsetenv("RUST_BACKTRACE");
            unsetenv("RUST_LIB_BACKTRACE");

            execlp("xwayland-satellite", "xwayland-satellite", ":0", "--test-listenfd-support", nullptr);
            _exit(127);
        }

        int status = 0;
        waitpid(pid2, &status, 0);
        int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 255;
        if (write(pipefd[1], &exitCode, sizeof(exitCode)) < 0) {
            // nothing we can do
        }
        _exit(0);
    }

    close(pipefd[1]);

    int     exitCode  = -1;
    ssize_t bytesRead = read(pipefd[0], &exitCode, sizeof(exitCode));
    close(pipefd[0]);

    if (bytesRead != sizeof(exitCode)) {
        Log::logger->log(Log::INFO, "[XWayland-Satellite] Error waiting for xwayland-satellite test: Failed to read from pipe");
        return false;
    }

    if (exitCode != 0) {
        if (exitCode == 127)
            Log::logger->log(Log::INFO, "[XWayland-Satellite] xwayland-satellite not found, disabling integration");
        else
            Log::logger->log(Log::INFO, "[XWayland-Satellite] xwayland-satellite doesn't support on-demand activation, disabling integration");

        return false;
    }

    return true;
}

bool CXWaylandSatellite::setup(wl_event_loop* eventLoop) {
    m_eventLoop = eventLoop;

    if (!testOnDemand())
        return false;

    if (!tryOpenSockets())
        return false;

    setenv("DISPLAY", m_displayName.c_str(), true);
    Log::logger->log(Log::INFO, "[XWayland-Satellite] Listening on X11 socket: {}", m_displayName);

    m_enabled = true;
    setupWatch();
    return true;
}

void CXWaylandSatellite::clearPendingConnections(CFileDescriptor& fd) {
    int flags = fcntl(fd.get(), F_GETFL);
    if (flags < 0)
        return;

    fcntl(fd.get(), F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_un addr;
    socklen_t          len = sizeof(addr);
    while (true) {
        int clientFd = accept(fd.get(), rc<struct sockaddr*>(&addr), &len);
        if (clientFd < 0)
            break;

        close(clientFd);
    }

    fcntl(fd.get(), F_SETFL, flags);
}

void CXWaylandSatellite::removeWatches() {
    if (m_abstractWatch) {
        wl_event_source_remove(m_abstractWatch);
        m_abstractWatch = nullptr;
    }

    if (m_unixWatch) {
        wl_event_source_remove(m_unixWatch);
        m_unixWatch = nullptr;
    }
}

int CXWaylandSatellite::onSocketActivity(int fd, uint32_t mask, void* data) {
    auto* self = static_cast<CXWaylandSatellite*>(data);

    Log::logger->log(Log::DEBUG, "[XWayland-Satellite] Connection on X11 socket; spawning xwayland-satellite");

    self->removeWatches();
    self->spawn();

    return 0;
}

void CXWaylandSatellite::setupWatch() {
    removeWatches();

    if (!m_eventLoop || !m_xFDs[0].isValid() || !m_xFDs[1].isValid())
        return;

    clearPendingConnections(m_xFDs[0]);
    clearPendingConnections(m_xFDs[1]);

    m_abstractWatch = wl_event_loop_add_fd(m_eventLoop, m_xFDs[0].get(), WL_EVENT_READABLE, onSocketActivity, this);
    m_unixWatch     = wl_event_loop_add_fd(m_eventLoop, m_xFDs[1].get(), WL_EVENT_READABLE, onSocketActivity, this);
}

void CXWaylandSatellite::spawn() {
    CFileDescriptor abstractFd{dup(m_xFDs[0].get())};
    CFileDescriptor unixFd{dup(m_xFDs[1].get())};

    if (!abstractFd.isValid() || !unixFd.isValid()) {
        Log::logger->log(Log::ERR, "[XWayland-Satellite] Failed to dup socket fds");
        setupWatch();
        return;
    }

    int abstractRaw = abstractFd.get();
    int unixRaw     = unixFd.get();

    std::thread([this, abstractRaw, unixRaw, abstractFd = std::move(abstractFd), unixFd = std::move(unixFd)]() mutable {
        pid_t pid = fork();
        if (pid < 0) {
            Log::logger->log(Log::ERR, "[XWayland-Satellite] fork failed: {}", strerror(errno));
            wl_event_loop_add_idle(
                m_eventLoop,
                [](void* data) {
                    auto* self = static_cast<CXWaylandSatellite*>(data);
                    self->setupWatch();
                },
                this);
            return;
        }

        if (pid == 0) {
            fcntl(abstractRaw, F_SETFD, 0);
            fcntl(unixRaw, F_SETFD, 0);

            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }

            unsetenv("DISPLAY");
            unsetenv("RUST_BACKTRACE");
            unsetenv("RUST_LIB_BACKTRACE");

            std::string abstractStr = std::to_string(abstractRaw);
            std::string unixStr     = std::to_string(unixRaw);

            execlp("xwayland-satellite", "xwayland-satellite", m_displayName.c_str(), "-listenfd", abstractStr.c_str(), "-listenfd", unixStr.c_str(), nullptr);

            Log::logger->log(Log::ERR, "[XWayland-Satellite] Failed to exec xwayland-satellite: {}", strerror(errno));
            _exit(127);
        }

        abstractFd.reset();
        unixFd.reset();

        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            if (errno == ECHILD)
                Log::logger->log(Log::WARN, "[XWayland-Satellite] xwayland-satellite terminated");
            else
                Log::logger->log(Log::WARN, "[XWayland-Satellite] Error waiting for xwayland-satellite: {}", strerror(errno));
        } else
            Log::logger->log(Log::WARN, "[XWayland-Satellite] xwayland-satellite exited with status {}", WEXITSTATUS(status));

        wl_event_loop_add_idle(
            m_eventLoop,
            [](void* data) {
                auto* self = static_cast<CXWaylandSatellite*>(data);
                self->setupWatch();
            },
            this);
    }).detach();
}

bool CXWaylandSatellite::enabled() const {
    return m_enabled;
}

const std::string& CXWaylandSatellite::displayName() const {
    return m_displayName;
}

#endif // USE_XWAYLAND_SATELLITE
