#include "HyprlandSocket.hpp"
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <poll.h>
#include <print>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <hyprutils/memory/Casts.hpp>
#include <hyprutils/os/FileDescriptor.hpp>

#include "../helpers/StringUtils.hpp"

using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;

// Old servers ignore this as an unknown flag; new servers use it to avoid
// quiet-period request framing.
static constexpr char EXPLICIT_FRAME_MARKER = '\x1F';
static constexpr auto CONNECT_TIMEOUT       = std::chrono::seconds{5};
static constexpr auto WRITE_TIMEOUT         = std::chrono::seconds{5};
static constexpr auto REPLY_TIMEOUT         = std::chrono::seconds{35}; // server handlers may run for up to 30 seconds

static bool           waitForSocket(const int fd, const short events, const std::chrono::steady_clock::time_point deadline) {
    while (true) {
        const auto NOW = std::chrono::steady_clock::now();
        if (NOW >= deadline) {
            errno = ETIMEDOUT;
            return false;
        }

        const auto REMAINING = std::chrono::ceil<std::chrono::milliseconds>(deadline - NOW);
        const auto TIMEOUT   = sc<int>(REMAINING.count());
        pollfd     pollFD    = {
            .fd      = fd,
            .events  = events,
            .revents = 0,
        };

        const auto POLL_RESULT = poll(&pollFD, 1, TIMEOUT);
        if (POLL_RESULT > 0) {
            if (pollFD.revents & POLLNVAL) {
                errno = EBADF;
                return false;
            }

            if (pollFD.revents & (events | POLLERR | POLLHUP))
                return true;

            errno = EIO;
            return false;
        }

        if (POLL_RESULT == 0) {
            errno = ETIMEDOUT;
            return false;
        }

        if (errno != EINTR)
            return false;
    }
}

static bool connectSocket(const int fd, const sockaddr_un& address) {
    const auto DEADLINE = std::chrono::steady_clock::now() + CONNECT_TIMEOUT;

    if (connect(fd, rc<const sockaddr*>(&address), SUN_LEN(&address)) == 0)
        return true;

    if (errno != EINPROGRESS && errno != EAGAIN && errno != EINTR)
        return false;

    if (!waitForSocket(fd, POLLOUT, DEADLINE))
        return false;

    int       socketError = 0;
    socklen_t errorSize   = sizeof(socketError);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &socketError, &errorSize) < 0)
        return false;

    if (socketError == 0)
        return true;

    errno = socketError;
    return false;
}

static bool writeAll(const int fd, std::string_view data) {
    const auto DEADLINE     = std::chrono::steady_clock::now() + WRITE_TIMEOUT;
    size_t     totalWritten = 0;

    while (totalWritten < data.size()) {
        if (std::chrono::steady_clock::now() >= DEADLINE) {
            errno = ETIMEDOUT;
            return false;
        }

        const auto written = send(fd, data.data() + totalWritten, data.size() - totalWritten, MSG_NOSIGNAL);
        if (written > 0) {
            totalWritten += sc<size_t>(written);
            continue;
        }

        if (written < 0 && errno == EINTR) {
            if (std::chrono::steady_clock::now() < DEADLINE)
                continue;

            errno = ETIMEDOUT;
            return false;
        }

        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (waitForSocket(fd, POLLOUT, DEADLINE))
                continue;

            return false;
        }

        if (written == 0)
            errno = EIO;
        return false;
    }

    return true;
}

static bool readAll(const int fd, std::string& reply) {
    const auto             DEADLINE = std::chrono::steady_clock::now() + REPLY_TIMEOUT;
    std::array<char, 8192> buffer   = {};

    while (true) {
        if (std::chrono::steady_clock::now() >= DEADLINE) {
            errno = ETIMEDOUT;
            return false;
        }

        const auto SIZE = recv(fd, buffer.data(), buffer.size(), 0);
        if (SIZE > 0) {
            reply.append(buffer.data(), sc<size_t>(SIZE));
            continue;
        }

        if (SIZE == 0)
            return true;

        if (errno == EINTR) {
            if (std::chrono::steady_clock::now() < DEADLINE)
                continue;

            errno = ETIMEDOUT;
            return false;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (waitForSocket(fd, POLLIN, DEADLINE))
                continue;
        }

        return false;
    }
}

static int getUID() {
    const auto UID   = getuid();
    const auto PWUID = getpwuid(UID);
    return PWUID ? PWUID->pw_uid : UID;
}

static std::string getRuntimeDir() {
    const auto XDG = getenv("XDG_RUNTIME_DIR");

    if (!XDG) {
        const std::string USERID = std::to_string(getUID());
        return "/run/user/" + USERID + "/hypr";
    }

    return std::string{XDG} + "/hypr";
}

std::string NHyprlandSocket::send(const std::string& cmd) {
    CFileDescriptor serverSocket{socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0)};

    if (!serverSocket.isValid()) {
        std::println("{}", failureString("Couldn't open a socket (1)"));
        return "";
    }

    const auto HIS = getenv("HYPRLAND_INSTANCE_SIGNATURE");

    if (!HIS) {
        std::println("{}", failureString("HYPRLAND_INSTANCE_SIGNATURE was not set! (Is Hyprland running?) (3)"));
        return "";
    }

    sockaddr_un serverAddress = {};
    serverAddress.sun_family  = AF_UNIX;

    const std::string socketPath = getRuntimeDir() + "/" + HIS + "/.socket.sock";
    if (socketPath.size() >= sizeof(serverAddress.sun_path)) {
        std::println("{}", failureString("Socket path is too long (4)"));
        return "";
    }

    std::memcpy(serverAddress.sun_path, socketPath.c_str(), socketPath.size() + 1);

    if (!connectSocket(serverSocket.get(), serverAddress)) {
        std::println("{}", failureString("Couldn't connect to " + socketPath + ". (4)"));
        return "";
    }

    const auto  separator      = cmd.find('/');
    const auto  space          = cmd.find(' ');
    const bool  needsSeparator = cmd.starts_with("[[BATCH]]") || separator == std::string::npos || (space != std::string::npos && separator > space);
    std::string framedCommand;
    framedCommand.reserve(cmd.size() + (needsSeparator ? 3 : 2));
    framedCommand.push_back(EXPLICIT_FRAME_MARKER);
    if (needsSeparator)
        framedCommand.push_back('/');
    framedCommand.append(cmd);
    framedCommand.push_back('\0');

    if (!writeAll(serverSocket.get(), framedCommand)) {
        std::println("{}", failureString("Couldn't write (5)"));
        return "";
    }

    if (shutdown(serverSocket.get(), SHUT_WR) < 0 && errno != ENOTCONN) {
        std::println("{}", failureString("Couldn't finish writing (5)"));
        return "";
    }

    std::string reply;
    if (!readAll(serverSocket.get(), reply)) {
        std::println("{}", failureString("Couldn't read (6)"));
        return "";
    }

    return reply;
}
