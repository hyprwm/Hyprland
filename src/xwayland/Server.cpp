#include <cstdint>
#ifndef NO_XWAYLAND

#include <format>
#include <string>
#include <cerrno>
#include <fcntl.h>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <sys/un.h>
#include <unistd.h>
#include <exception>
#include <filesystem>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "Server.hpp"
#include "XWayland.hpp"
#include "config/ConfigValue.hpp"
#include "debug/Log.hpp"
#include "../defines.hpp"
#include "../Compositor.hpp"
#include "../managers/CursorManager.hpp"
using namespace Hyprutils::OS;

// Constants
constexpr int          SOCKET_DIR_PERMISSIONS = 0755;
constexpr int          SOCKET_BACKLOG         = 1;
constexpr int          MAX_SOCKET_RETRIES     = 32;
constexpr int          LOCK_FILE_MODE         = 0444;

static CFileDescriptor createSocket(struct sockaddr_un* addr, size_t pathSize) {
    const bool        isRegularSocket(addr->sun_path[0]);
    const char        dbgSocketPathPrefix = isRegularSocket ? addr->sun_path[0] : '@';
    const char* const dbgSocketPathRem    = addr->sun_path + 1;

    socklen_t         size = offsetof(struct sockaddr_un, sun_path) + pathSize + 1;
    CFileDescriptor   fd{socket(AF_UNIX, SOCK_STREAM, 0)};
    if (!fd.isValid()) {
        Debug::log(ERR, "Failed to create socket {}{}", dbgSocketPathPrefix, dbgSocketPathRem);
        return {};
    }

    if (!fd.setFlags(fd.getFlags() | FD_CLOEXEC)) {
        Debug::log(ERR, "Failed to set flags for socket {}{}", dbgSocketPathPrefix, dbgSocketPathRem);
        return {};
    }

    if (isRegularSocket)
        unlink(addr->sun_path);

    if (bind(fd.get(), (struct sockaddr*)addr, size) < 0) {
        Debug::log(ERR, "Failed to bind socket {}{}", dbgSocketPathPrefix, dbgSocketPathRem);
        if (isRegularSocket)
            unlink(addr->sun_path);
        return {};
    }

    // Required for the correct functioning of `xhost` #9574
    // The operation is safe because XWayland controls socket access by itself
    // and rejects connections not matched by the `xhost` ACL
    if (isRegularSocket && chmod(addr->sun_path, 0666) < 0) {
        // We are only extending the default permissions,
        // and I don't see the reason to make a full stop in case of a failed operation.
        Debug::log(ERR, "Failed to set permission mode for socket {}{}", dbgSocketPathPrefix, dbgSocketPathRem);
    }

    if (listen(fd.get(), SOCKET_BACKLOG) < 0) {
        Debug::log(ERR, "Failed to listen to socket {}{}", dbgSocketPathPrefix, dbgSocketPathRem);
        if (isRegularSocket)
            unlink(addr->sun_path);
        return {};
    }

    return fd;
}

static bool checkPermissionsForSocketDir() {
    struct stat buf;

    if (lstat("/tmp/.X11-unix", &buf)) {
        Debug::log(ERR, "Failed to stat X11 socket dir");
        return false;
    }

    if (!(buf.st_mode & S_IFDIR)) {
        Debug::log(ERR, "X11 socket dir is not a directory");
        return false;
    }

    if ((buf.st_uid != 0) && (buf.st_uid != getuid())) {
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

static bool openSockets(std::array<CFileDescriptor, 2>& sockets, int display) {
    static auto CREATEABSTRACTSOCKET = CConfigValue<Hyprlang::INT>("xwayland:create_abstract_socket");

    if (!ensureSocketDirExists())
        return false;

    sockaddr_un addr = {.sun_family = AF_UNIX};
    std::string path;

#ifdef __linux__
    if (*CREATEABSTRACTSOCKET) {
        // cursed...
        // but is kept as an option for better compatibility
        addr.sun_path[0] = 0;
        path             = getSocketPath(display, true);
        strncpy(addr.sun_path + 1, path.c_str(), path.length() + 1);
    } else {
        path = getSocketPath(display, false);
        strncpy(addr.sun_path, path.c_str(), path.length() + 1);
    }
#else
    if (*CREATEABSTRACTSOCKET) {
        Debug::log(WARN, "The abstract XWayland Unix domain socket might be used only on Linux systems. A regular one'll be created insted.");
    }
    path = getSocketPath(display, false);
    strncpy(addr.sun_path, path.c_str(), path.length() + 1);
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

static void startServer(void* data) {
    if (!g_pXWayland->m_server->start())
        Debug::log(ERR, "The XWayland server could not start! XWayland will not work...");
}

static int xwaylandReady(int fd, uint32_t mask, void* data) {
    return g_pXWayland->m_server->ready(fd, mask);
}

static bool safeRemove(const std::string& path) {
    try {
        return std::filesystem::remove(path);
    } catch (const std::exception& e) { Debug::log(ERR, "[XWayland] Failed to remove {}", path); }
    return false;
}

bool CXWaylandServer::tryOpenSockets() {
    for (size_t i = 0; i <= MAX_SOCKET_RETRIES; ++i) {
        std::string     lockPath = std::format("/tmp/.X{}-lock", i);

        CFileDescriptor fd{open(lockPath.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, LOCK_FILE_MODE)};
        if (fd.isValid()) {
            // we managed to open the lock
            if (!openSockets(m_xFDs, i)) {
                safeRemove(lockPath);
                continue;
            }

            const std::string pidStr = std::format("{:010d}\n", getpid());
            ASSERT(pidStr.length() == 11);
            if (write(fd.get(), pidStr.c_str(), 11) != 11L) {
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
        read(fd.get(), pidstr, sizeof(pidstr) - 1);

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
        Debug::log(ERR, "Failed to find a suitable socket for XWayland");
        return false;
    }

    Debug::log(LOG, "XWayland found a suitable display socket at DISPLAY: {}", m_displayName);
    return true;
}

CXWaylandServer::CXWaylandServer() {
    ;
}

CXWaylandServer::~CXWaylandServer() {
    die();
    if (m_display < 0)
        return;

    std::string lockPath = std::format("/tmp/.X{}-lock", m_display);
    safeRemove(lockPath);

    std::string path;
    for (bool isLinux : {true, false}) {
        path = getSocketPath(m_display, isLinux);
        safeRemove(path);
    }
}

void CXWaylandServer::die() {
    if (m_display < 0)
        return;

    if (m_xFDReadEvents[0]) {
        wl_event_source_remove(m_xFDReadEvents[0]);
        wl_event_source_remove(m_xFDReadEvents[1]);
        m_xFDReadEvents = {nullptr, nullptr};
    }

    if (m_pipeSource)
        wl_event_source_remove(m_pipeSource);

    // possible crash. Better to leak a bit.
    //if (xwaylandClient)
    //    wl_client_destroy(xwaylandClient);

    m_xwaylandClient = nullptr;
}

bool CXWaylandServer::create() {
    if (!tryOpenSockets())
        return false;

    setenv("DISPLAY", m_displayName.c_str(), true);

    // TODO: lazy mode

    m_idleSource = wl_event_loop_add_idle(g_pCompositor->m_wlEventLoop, ::startServer, nullptr);

    return true;
}

void CXWaylandServer::runXWayland(CFileDescriptor& notifyFD) {
    if (!m_xFDs[0].setFlags(m_xFDs[0].getFlags() & ~FD_CLOEXEC) || !m_xFDs[1].setFlags(m_xFDs[1].getFlags() & ~FD_CLOEXEC) ||
        !m_waylandFDs[1].setFlags(m_waylandFDs[1].getFlags() & ~FD_CLOEXEC) || !m_xwmFDs[1].setFlags(m_xwmFDs[1].getFlags() & ~FD_CLOEXEC)) {
        Debug::log(ERR, "Failed to unset cloexec on fds");
        _exit(EXIT_FAILURE);
    }

    auto cmd = std::format("Xwayland {} -rootless -core -listenfd {} -listenfd {} -displayfd {} -wm {}", m_displayName, m_xFDs[0].get(), m_xFDs[1].get(), notifyFD.get(),
                           m_xwmFDs[1].get());

    auto waylandSocket = std::format("{}", m_waylandFDs[1].get());
    setenv("WAYLAND_SOCKET", waylandSocket.c_str(), true);

    Debug::log(LOG, "Starting XWayland with \"{}\", bon voyage!", cmd);

    execl("/bin/sh", "/bin/sh", "-c", cmd.c_str(), nullptr);

    Debug::log(ERR, "XWayland failed to open");
    _exit(1);
}

bool CXWaylandServer::start() {
    m_idleSource  = nullptr;
    int wlPair[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, wlPair) != 0) {
        Debug::log(ERR, "socketpair failed (1)");
        die();
        return false;
    }
    m_waylandFDs[0] = CFileDescriptor{wlPair[0]};
    m_waylandFDs[1] = CFileDescriptor{wlPair[1]};

    if (!m_waylandFDs[0].setFlags(m_waylandFDs[0].getFlags() | FD_CLOEXEC) || !m_waylandFDs[1].setFlags(m_waylandFDs[1].getFlags() | FD_CLOEXEC)) {
        Debug::log(ERR, "set_cloexec failed (1)");
        die();
        return false;
    }

    int xwmPair[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, xwmPair) != 0) {
        Debug::log(ERR, "socketpair failed (2)");
        die();
        return false;
    }

    m_xwmFDs[0] = CFileDescriptor{xwmPair[0]};
    m_xwmFDs[1] = CFileDescriptor{xwmPair[1]};

    if (!m_xwmFDs[0].setFlags(m_xwmFDs[0].getFlags() | FD_CLOEXEC) || !m_xwmFDs[1].setFlags(m_xwmFDs[1].getFlags() | FD_CLOEXEC)) {
        Debug::log(ERR, "set_cloexec failed (2)");
        die();
        return false;
    }

    m_xwaylandClient = wl_client_create(g_pCompositor->m_wlDisplay, m_waylandFDs[0].get());
    if (!m_xwaylandClient) {
        Debug::log(ERR, "wl_client_create failed");
        die();
        return false;
    }

    m_waylandFDs[0].take(); // wl_client owns this fd now

    int notify[2] = {-1, -1};
    if (pipe(notify) < 0) {
        Debug::log(ERR, "pipe failed");
        die();
        return false;
    }

    CFileDescriptor notifyFds[2] = {CFileDescriptor{notify[0]}, CFileDescriptor{notify[1]}};

    if (!notifyFds[0].setFlags(notifyFds[0].getFlags() | FD_CLOEXEC)) {
        Debug::log(ERR, "set_cloexec failed (3)");
        die();
        return false;
    }

    m_pipeSource = wl_event_loop_add_fd(g_pCompositor->m_wlEventLoop, notifyFds[0].get(), WL_EVENT_READABLE, ::xwaylandReady, nullptr);
    m_pipeFd     = std::move(notifyFds[0]);

    auto serverPID = fork();
    if (serverPID < 0) {
        Debug::log(ERR, "fork failed");
        die();
        return false;
    } else if (serverPID == 0) {
        runXWayland(notifyFds[1]);
        _exit(0);
    }

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

    // if we don't have readable here, it failed
    if (!(mask & WL_EVENT_READABLE)) {
        Debug::log(ERR, "Xwayland: startup failed, not setting up xwm");
        g_pXWayland->m_server.reset();
        return 1;
    }

    Debug::log(LOG, "XWayland is ready");

    wl_event_source_remove(m_pipeSource);
    m_pipeFd.reset();
    m_pipeSource = nullptr;

    // start the wm
    if (!g_pXWayland->m_wm)
        g_pXWayland->m_wm = makeUnique<CXWM>();

    g_pCursorManager->setXWaylandCursor();

    return 0;
}

#endif
