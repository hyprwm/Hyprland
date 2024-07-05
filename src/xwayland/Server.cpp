#ifndef NO_XWAYLAND

#include "Server.hpp"
#include "../defines.hpp"
#include "../Compositor.hpp"
#include "../managers/CursorManager.hpp"
#include "XWayland.hpp"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

// TODO: cleanup
static bool set_cloexec(int fd, bool cloexec) {
    int flags = fcntl(fd, F_GETFD);
    if (flags == -1) {
        Debug::log(ERR, "fcntl failed");
        return false;
    }
    if (cloexec) {
        flags = flags | FD_CLOEXEC;
    } else {
        flags = flags & ~FD_CLOEXEC;
    }
    if (fcntl(fd, F_SETFD, flags) == -1) {
        Debug::log(ERR, "fcntl failed");
        return false;
    }
    return true;
}

static int openSocket(struct sockaddr_un* addr, size_t path_size) {
    int       fd, rc;
    socklen_t size = offsetof(struct sockaddr_un, sun_path) + path_size + 1;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        Debug::log(ERR, "failed to create socket {}{}", addr->sun_path[0] ? addr->sun_path[0] : '@', addr->sun_path + 1);
        return -1;
    }
    if (!set_cloexec(fd, true)) {
        close(fd);
        return -1;
    }

    if (addr->sun_path[0]) {
        unlink(addr->sun_path);
    }
    if (bind(fd, (struct sockaddr*)addr, size) < 0) {
        rc = errno;
        Debug::log(ERR, "failed to bind socket {}{}", addr->sun_path[0] ? addr->sun_path[0] : '@', addr->sun_path + 1);
        goto cleanup;
    }
    if (listen(fd, 1) < 0) {
        rc = errno;
        Debug::log(ERR, "failed to listen to socket {}{}", addr->sun_path[0] ? addr->sun_path[0] : '@', addr->sun_path + 1);
        goto cleanup;
    }

    return fd;

cleanup:
    close(fd);
    if (addr->sun_path[0]) {
        unlink(addr->sun_path);
    }
    errno = rc;
    return -1;
}

static bool checkPermissionsForSocketDir(void) {
    struct stat buf;

    if (lstat("/tmp/.X11-unix", &buf)) {
        Debug::log(ERR, "Failed statting X11 socket dir");
        return false;
    }

    if (!(buf.st_mode & S_IFDIR)) {
        Debug::log(ERR, "X11 socket dir is not a dir");
        return false;
    }

    if (!((buf.st_uid == 0) || (buf.st_uid == getuid()))) {
        Debug::log(ERR, "X11 socket dir is not ours");
        return false;
    }

    if (!(buf.st_mode & S_ISVTX)) {
        if ((buf.st_mode & (S_IWGRP | S_IWOTH))) {
            Debug::log(ERR, "X11 socket dir is sticky by others");
            return false;
        }
    }

    return true;
}

static bool openSockets(std::array<int, 2>& sockets, int display) {
    auto ret = mkdir("/tmp/.X11-unix", 755);

    if (ret != 0) {
        if (errno == EEXIST) {
            if (!checkPermissionsForSocketDir())
                return false;
        } else {
            Debug::log(ERR, "XWayland: couldn't create socket dir");
            return false;
        }
    }

    std::string path;
    sockaddr_un addr = {.sun_family = AF_UNIX};

#ifdef __linux__
    // cursed...
    addr.sun_path[0] = 0;
    path             = std::format("/tmp/.X11-unix/X{}", display);
    strncpy(addr.sun_path + 1, path.c_str(), path.length() + 1);
#else
    path = std::format("/tmp/.X11-unix/X{}_", display);
    strncpy(addr.sun_path, path.c_str(), path.length() + 1);
#endif
    sockets[0] = openSocket(&addr, path.length());
    if (sockets[0] < 0)
        return false;

    path = std::format("/tmp/.X11-unix/X{}", display);
    strncpy(addr.sun_path, path.c_str(), path.length() + 1);
    sockets[1] = openSocket(&addr, path.length());
    if (sockets[1] < 0) {
        close(sockets[0]);
        sockets[0] = -1;
        return false;
    }

    return true;
}

static void startServer(void* data) {
    if (!g_pXWayland->pServer->start())
        Debug::log(ERR, "The XWayland server could not start! XWayland will not work...");
}

static int xwaylandReady(int fd, uint32_t mask, void* data) {
    return g_pXWayland->pServer->ready(fd, mask);
}

static bool safeRemove(const std::string& path) {
    try {
        return std::filesystem::remove(path);
    } catch (std::exception& e) { Debug::log(ERR, "[XWayland] failed to remove {}", path); }

    return false;
}

bool CXWaylandServer::tryOpenSockets() {
    for (size_t i = 0; i <= 32; ++i) {
        auto LOCK = std::format("/tmp/.X{}-lock", i);

        if (int fd = open(LOCK.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0444); fd >= 0) {
            // we managed to open the lock
            if (!openSockets(xFDs, i)) {
                safeRemove(LOCK);
                close(fd);
                continue;
            }

            const auto PIDSTR = std::format("{}", getpid());

            if (write(fd, PIDSTR.c_str(), PIDSTR.length()) != (long)PIDSTR.length()) {
                safeRemove(LOCK);
                close(fd);
                continue;
            }

            close(fd);

            display     = i;
            displayName = std::format(":{}", display);
            break;
        }

        int fd = open(LOCK.c_str(), O_RDONLY | O_CLOEXEC);

        if (fd < 0)
            continue;

        char pidstr[12] = {0};
        read(fd, pidstr, sizeof(pidstr) - 1);
        close(fd);

        uint64_t pid = 0;
        try {
            pid = std::stoi(std::string{pidstr, 11});
        } catch (...) { continue; }

        if (kill(pid, 0) != 0 && errno == ESRCH) {
            if (!safeRemove(LOCK))
                continue;

            i--;
        }
    }

    if (display < 0) {
        Debug::log(ERR, "Failed to find a suitable socket for xwayland");
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

    if (xFDs[0])
        close(xFDs[0]);
    if (xFDs[1])
        close(xFDs[1]);

    auto LOCK = std::format("/tmp/.X{}-lock", display);
    safeRemove(LOCK);

    std::string path;
#ifdef __linux__
    path = std::format("/tmp/.X11-unix/X{}", display);
#else
    path = std::format("/tmp/.X11-unix/X{}_", display);
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

    if (waylandFDs[0])
        close(waylandFDs[0]);
    if (waylandFDs[1])
        close(waylandFDs[1]);
    if (xwmFDs[0])
        close(xwmFDs[0]);
    if (xwmFDs[1])
        close(xwmFDs[1]);

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

    // TODO: lazy mode

    idleSource = wl_event_loop_add_idle(g_pCompositor->m_sWLEventLoop, ::startServer, nullptr);

    return true;
}

void CXWaylandServer::runXWayland(int notifyFD) {
    if (!set_cloexec(xFDs[0], false) || !set_cloexec(xFDs[1], false) || !set_cloexec(waylandFDs[1], false) || !set_cloexec(xwmFDs[1], false)) {
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

    if (!set_cloexec(waylandFDs[0], true) || !set_cloexec(waylandFDs[1], true)) {
        Debug::log(ERR, "set_cloexec failed (1)");
        die();
        return false;
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, xwmFDs.data()) != 0) {
        Debug::log(ERR, "socketpair failed (2)");
        die();
        return false;
    }

    if (!set_cloexec(xwmFDs[0], true) || !set_cloexec(xwmFDs[1], true)) {
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

    if (!set_cloexec(notify[0], true)) {
        Debug::log(ERR, "set_cloexec failed (3)");
        close(notify[0]);
        close(notify[1]);
        die();
        return false;
    }

    pipeSource = wl_event_loop_add_fd(g_pCompositor->m_sWLEventLoop, notify[0], WL_EVENT_READABLE, ::xwaylandReady, nullptr);

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
        } else if (pid == 0) {
            runXWayland(notify[1]);
        }

        _exit(0);
    }

    close(notify[1]);
    close(waylandFDs[1]);
    close(xwmFDs[1]);
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
    g_pXWayland->pWM = std::make_unique<CXWM>();

    g_pCursorManager->setXWaylandCursor();

    return 0;
}

#endif
