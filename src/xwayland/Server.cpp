#include <cstdint>
#ifndef NO_XWAYLAND

#include <format>
#include <string>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <cstring>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/un.h>
#include <unistd.h>
#include <exception>
#include <filesystem>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "Server.hpp"
#include "XWayland.hpp"
#include "debug/Log.hpp"
#include "../defines.hpp"
#include "../Compositor.hpp"
#include "../managers/CursorManager.hpp"

// Constants
constexpr int SOCKET_DIR_PERMISSIONS = 0755;
constexpr int SOCKET_BACKLOG         = 1;
constexpr int MAX_SOCKET_RETRIES     = 32;
constexpr int LOCK_FILE_MODE         = 0444;

static bool   setCloseOnExec(int fd, bool cloexec) {
    int flags = fcntl(fd, F_GETFD);
    if (flags == -1) {
        Debug::log(ERR, "fcntl failed");
        return false;
    }

    if (cloexec)
        flags = flags | FD_CLOEXEC;
    else
        flags = flags & ~FD_CLOEXEC;

    if (fcntl(fd, F_SETFD, flags) == -1) {
        Debug::log(ERR, "fcntl failed");
        return false;
    }

    return true;
}

void cleanUpSocket(int fd, const char* path) {
    close(fd);
    if (path[0])
        unlink(path);
}

inline void closeSocketSafely(int& fd) {
    if (fd >= 0)
        close(fd);
}

static int createSocket(struct sockaddr_un* addr, size_t path_size) {
    socklen_t size = offsetof(struct sockaddr_un, sun_path) + path_size + 1;
    int       fd   = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        Debug::log(ERR, "Failed to create socket {}{}", addr->sun_path[0] ? addr->sun_path[0] : '@', addr->sun_path + 1);
        return -1;
    }

    if (!setCloseOnExec(fd, true)) {
        close(fd);
        return -1;
    }

    if (addr->sun_path[0])
        unlink(addr->sun_path);

    if (bind(fd, (struct sockaddr*)addr, size) < 0) {
        Debug::log(ERR, "Failed to bind socket {}{}", addr->sun_path[0] ? addr->sun_path[0] : '@', addr->sun_path + 1);
        cleanUpSocket(fd, addr->sun_path);
        return -1;
    }

    if (listen(fd, SOCKET_BACKLOG) < 0) {
        Debug::log(ERR, "Failed to listen to socket {}{}", addr->sun_path[0] ? addr->sun_path[0] : '@', addr->sun_path + 1);
        cleanUpSocket(fd, addr->sun_path);
        return -1;
    }

    return fd;
}

static bool checkPermissionsForSocketDir(void) {
    struct stat buf;

    if (lstat("/tmp/.X11-unix", &buf)) {
        Debug::log(ERR, "Failed to stat X11 socket dir");
        return false;
    }

    if (!(buf.st_mode & S_IFDIR)) {
        Debug::log(ERR, "X11 socket dir is not a directory");
        return false;
    }

    if (!((buf.st_uid == 0) || (buf.st_uid == getuid()))) {
        Debug::log(ERR, "X11 socket dir is not owned by root or current user");
        return false;
    }

    if (!(buf.st_mode & S_ISVTX)) {
        if ((buf.st_mode & (S_IWGRP | S_IWOTH))) {
            Debug::log(ERR, "X11 socket dir is writable by others");
            return false;
        }
    }

    return true;
}

static bool ensureSocketDirExists() {
    if (mkdir("/tmp/.X11-unix", SOCKET_DIR_PERMISSIONS) != 0) {
        if (errno == EEXIST)
            return checkPermissionsForSocketDir();
        else {
            Debug::log(ERR, "XWayland: Couldn't create socket dir");
            return false;
        }
    }

    return true;
}

static std::string getSocketPath(int display, bool isLinux) {
    if (isLinux)
        return std::format("/tmp/.X11-unix/X{}", display);

    return std::format("/tmp/.X11-unix/X{}_", display);
}

static bool openSockets(std::array<int, 2>& sockets, int display) {
    if (!ensureSocketDirExists())
        return false;

    sockaddr_un addr = {.sun_family = AF_UNIX};
    std::string path;

#ifdef __linux__
    // cursed...
    addr.sun_path[0] = 0;
    path             = getSocketPath(display, true);
    strncpy(addr.sun_path + 1, path.c_str(), path.length() + 1);
#else
    path = getSocketPath(display, false);
    strncpy(addr.sun_path, path.c_str(), path.length() + 1);
#endif

    sockets[0] = createSocket(&addr, path.length());
    if (sockets[0] < 0)
        return false;

    path = getSocketPath(display, true);
    strncpy(addr.sun_path, path.c_str(), path.length() + 1);

    sockets[1] = createSocket(&addr, path.length());
    if (sockets[1] < 0) {
        close(sockets[0]);
        sockets[0] = -1;
        return false;
    }

    return true;
}

static int xwaylandReady(int fd, uint32_t mask, void* data) {
    return g_pXWayland->pServer->ready(fd, mask);
}

static bool safeRemove(const std::string& path) {
    try {
        return std::filesystem::remove(path);
    } catch (const std::exception& e) { Debug::log(ERR, "[XWayland] Failed to remove {}", path); }
    return false;
}

bool CXWaylandServer::tryOpenSockets() {
    for (size_t i = 0; i <= MAX_SOCKET_RETRIES; ++i) {
        std::string lockPath = std::format("/tmp/.X{}-lock", i);

        int         fd = open(lockPath.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, LOCK_FILE_MODE);
        if (fd >= 0) {
            // we managed to open the lock
            if (!openSockets(xFDs, i)) {
                safeRemove(lockPath);
                close(fd);
                continue;
            }

            const std::string pidStr = std::to_string(getpid());
            if (write(fd, pidStr.c_str(), pidStr.length()) != (long)pidStr.length()) {
                safeRemove(lockPath);
                close(fd);
                continue;
            }

            close(fd);
            display     = i;
            displayName = std::format(":{}", display);
            break;
        }

        fd = open(lockPath.c_str(), O_RDONLY | O_CLOEXEC);

        if (fd < 0)
            continue;

        char pidstr[12] = {0};
        read(fd, pidstr, sizeof(pidstr) - 1);
        close(fd);

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

    if (display < 0) {
        Debug::log(ERR, "Failed to find a suitable socket for XWayland");
        return false;
    }

    Debug::log(LOG, "XWayland found a suitable display socket at DISPLAY: {}", displayName);
    return true;
}

CXWaylandServer::CXWaylandServer() {
    ;
}

CXWaylandServer::~CXWaylandServer() {
    die();
    if (display < 0)
        return;

    closeSocketSafely(xFDs[0]);
    closeSocketSafely(xFDs[1]);

    std::string lockPath = std::format("/tmp/.X{}-lock", display);
    safeRemove(lockPath);

    std::string path;
#ifdef __linux__
    path = getSocketPath(display, true);
#else
    path = getSocketPath(display, false);
#endif
    safeRemove(path);
}

void CXWaylandServer::die() {
    if (display < 0)
        return;

    if (xFDReadEvents[0]) {
        wl_event_source_remove(xFDReadEvents[0]);
        wl_event_source_remove(xFDReadEvents[1]);
        xFDReadEvents = {nullptr, nullptr};
    }

    if (pipeSource)
        wl_event_source_remove(pipeSource);

    if (pipeFd >= 0)
        close(pipeFd);

    closeSocketSafely(waylandFDs[0]);
    closeSocketSafely(waylandFDs[1]);
    closeSocketSafely(xwmFDs[0]);
    closeSocketSafely(xwmFDs[1]);

    // possible crash. Better to leak a bit.
    //if (xwaylandClient)
    //    wl_client_destroy(xwaylandClient);

    xwaylandClient = nullptr;
    waylandFDs     = {-1, -1};
    xwmFDs         = {-1, -1};
}

bool CXWaylandServer::create() {
    if (!tryOpenSockets())
        return false;

    setenv("DISPLAY", displayName.c_str(), true);

    g_pEventLoopManager->doLater([this]() {
        if (!start())
            Debug::log(ERR, "The XWayland server could not start! XWayland will not work...");
    });

    return true;
}

void CXWaylandServer::runXWayland(int notifyFD) {
    if (!setCloseOnExec(xFDs[0], false) || !setCloseOnExec(xFDs[1], false) || !setCloseOnExec(waylandFDs[1], false) || !setCloseOnExec(xwmFDs[1], false)) {
        Debug::log(ERR, "Failed to unset cloexec on fds");
        _exit(EXIT_FAILURE);
    }

    auto cmd = std::format("Xwayland {} -rootless -core -listenfd {} -listenfd {} -displayfd {} -wm {}", displayName, xFDs[0], xFDs[1], notifyFD, xwmFDs[1]);

    auto waylandSocket = std::format("{}", waylandFDs[1]);
    setenv("WAYLAND_SOCKET", waylandSocket.c_str(), true);

    Debug::log(LOG, "Starting XWayland with \"{}\", bon voyage!", cmd);

    execl("/bin/sh", "/bin/sh", "-c", cmd.c_str(), nullptr);

    Debug::log(ERR, "XWayland failed to open");
    _exit(1);
}

bool CXWaylandServer::start() {
    idleSource = nullptr;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, waylandFDs.data()) != 0) {
        Debug::log(ERR, "socketpair failed (1)");
        die();
        return false;
    }

    if (!setCloseOnExec(waylandFDs[0], true) || !setCloseOnExec(waylandFDs[1], true)) {
        Debug::log(ERR, "set_cloexec failed (1)");
        die();
        return false;
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, xwmFDs.data()) != 0) {
        Debug::log(ERR, "socketpair failed (2)");
        die();
        return false;
    }

    if (!setCloseOnExec(xwmFDs[0], true) || !setCloseOnExec(xwmFDs[1], true)) {
        Debug::log(ERR, "set_cloexec failed (2)");
        die();
        return false;
    }

    xwaylandClient = wl_client_create(g_pCompositor->m_sWLDisplay, waylandFDs[0]);
    if (!xwaylandClient) {
        Debug::log(ERR, "wl_client_create failed");
        die();
        return false;
    }

    waylandFDs[0] = -1;

    int notify[2] = {-1, -1};
    if (pipe(notify) < 0) {
        Debug::log(ERR, "pipe failed");
        die();
        return false;
    }

    if (!setCloseOnExec(notify[0], true)) {
        Debug::log(ERR, "set_cloexec failed (3)");
        close(notify[0]);
        close(notify[1]);
        die();
        return false;
    }

    pipeSource = wl_event_loop_add_fd(g_pCompositor->m_sWLEventLoop, notify[0], WL_EVENT_READABLE, ::xwaylandReady, nullptr);
    pipeFd     = notify[0];

    serverPID = fork();
    if (serverPID < 0) {
        Debug::log(ERR, "fork failed");
        close(notify[0]);
        close(notify[1]);
        die();
        return false;
    } else if (serverPID == 0) {
        pid_t pid = fork();
        if (pid < 0) {
            Debug::log(ERR, "second fork failed");
            _exit(1);
        } else if (pid == 0)
            runXWayland(notify[1]);

        _exit(0);
    }

    close(notify[1]);
    close(waylandFDs[1]);
    closeSocketSafely(xwmFDs[1]);
    waylandFDs[1] = -1;
    xwmFDs[1]     = -1;

    return true;
}

int CXWaylandServer::ready(int fd, uint32_t mask) {
    if (mask & WL_EVENT_READABLE) {
        // xwayland writes twice
        char    buf[64];
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0 && errno != EINTR) {
            Debug::log(ERR, "Xwayland: read from displayFd failed");
            mask = 0;
        } else if (n <= 0 || buf[n - 1] != '\n')
            return 1;
    }

    while (waitpid(serverPID, nullptr, 0) < 0) {
        if (errno == EINTR)
            continue;
        Debug::log(ERR, "Xwayland: waitpid for fork failed");
        g_pXWayland->pServer.reset();
        return 1;
    }

    // if we don't have readable here, it failed
    if (!(mask & WL_EVENT_READABLE)) {
        Debug::log(ERR, "Xwayland: startup failed, not setting up xwm");
        g_pXWayland->pServer.reset();
        return 1;
    }

    Debug::log(LOG, "XWayland is ready");

    close(fd);
    wl_event_source_remove(pipeSource);
    pipeSource = nullptr;

    // start the wm
    if (!g_pXWayland->pWM)
        g_pXWayland->pWM = std::make_unique<CXWM>();

    g_pCursorManager->setXWaylandCursor();

    return 0;
}

#endif
