#include "HyprCtlSocket.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/memory/Casts.hpp>

using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;

static bool waitForSocket(int fd, short events, std::chrono::steady_clock::time_point deadline) {
    while (true) {
        const auto NOW = std::chrono::steady_clock::now();
        if (NOW >= deadline) {
            errno = ETIMEDOUT;
            return false;
        }

        const auto REMAINING = std::chrono::ceil<std::chrono::milliseconds>(deadline - NOW).count();
        const auto TIMEOUT   = static_cast<int>(std::min<int64_t>(REMAINING, INT_MAX));
        pollfd     pollFD    = {
            .fd      = fd,
            .events  = events,
            .revents = 0,
        };

        const auto RESULT = poll(&pollFD, 1, TIMEOUT);
        if (RESULT > 0) {
            if (pollFD.revents & POLLNVAL) {
                errno = EBADF;
                return false;
            }

            return true;
        }

        if (RESULT == 0) {
            errno = ETIMEDOUT;
            return false;
        }

        if (errno != EINTR)
            return false;
    }
}

static bool connectSocket(int fd, const sockaddr_un& address, std::chrono::steady_clock::time_point deadline) {
    while (true) {
        if (std::chrono::steady_clock::now() >= deadline) {
            errno = ETIMEDOUT;
            return false;
        }

        if (connect(fd, rc<const sockaddr*>(&address), SUN_LEN(&address)) == 0 || errno == EISCONN)
            return true;

        if (errno == EINTR)
            continue;

        if (errno != EINPROGRESS && errno != EALREADY && errno != EAGAIN)
            return false;

        if (!waitForSocket(fd, POLLOUT, deadline))
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
}

static bool writeAll(int fd, std::string_view data, std::chrono::steady_clock::time_point deadline) {
    size_t written = 0;

    while (written < data.size()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            errno = ETIMEDOUT;
            return false;
        }

        const auto SIZE = send(fd, data.data() + written, data.size() - written, MSG_NOSIGNAL);
        if (SIZE > 0) {
            written += static_cast<size_t>(SIZE);
            continue;
        }

        if (SIZE < 0 && errno == EINTR)
            continue;

        if (SIZE < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (waitForSocket(fd, POLLOUT, deadline))
                continue;
            return false;
        }

        if (SIZE == 0)
            errno = EIO;
        return false;
    }

    return true;
}

static std::expected<std::string, int> readAll(int fd, std::chrono::steady_clock::time_point deadline) {
    std::array<char, 8192> buffer = {};
    std::string            reply;

    while (true) {
        if (std::chrono::steady_clock::now() >= deadline)
            return std::unexpected(ETIMEDOUT);

        const auto SIZE = recv(fd, buffer.data(), buffer.size(), 0);
        if (SIZE > 0) {
            reply.append(buffer.data(), static_cast<size_t>(SIZE));
            continue;
        }

        if (SIZE == 0)
            return reply;

        if (errno == EINTR)
            continue;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (waitForSocket(fd, POLLIN, deadline))
                continue;
        }

        return std::unexpected(errno);
    }
}

std::expected<std::string, NHyprTester::HyprCtlSocket::SError> NHyprTester::HyprCtlSocket::request(std::string_view socketPath, std::string_view command,
                                                                                                   std::chrono::milliseconds timeout) {
    const auto DEADLINE = std::chrono::steady_clock::now() + timeout;

    if (socketPath.size() >= sizeof(sockaddr_un::sun_path))
        return std::unexpected(SError{.stage = eErrorStage::CONNECT, .code = ENAMETOOLONG});

    CFileDescriptor socket{::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0)};
    if (!socket.isValid())
        return std::unexpected(SError{.stage = eErrorStage::SOCKET, .code = errno});

    sockaddr_un address = {};
    address.sun_family  = AF_UNIX;
    std::memcpy(address.sun_path, socketPath.data(), socketPath.size());
    address.sun_path[socketPath.size()] = '\0';

    if (!connectSocket(socket.get(), address, DEADLINE))
        return std::unexpected(SError{.stage = eErrorStage::CONNECT, .code = errno});

    const auto  separator      = command.find('/');
    const auto  space          = command.find(' ');
    const bool  needsSeparator = command.starts_with("[[BATCH]]") || separator == std::string_view::npos || (space != std::string_view::npos && separator > space);
    std::string framedRequest;
    framedRequest.reserve(command.size() + (needsSeparator ? 3 : 2));
    framedRequest += EXPLICIT_FRAME_MARKER;
    if (needsSeparator)
        framedRequest += '/';
    framedRequest += command;
    framedRequest += '\0';

    if (!writeAll(socket.get(), framedRequest, DEADLINE))
        return std::unexpected(SError{.stage = eErrorStage::WRITE, .code = errno});

    if (shutdown(socket.get(), SHUT_WR) < 0 && errno != ENOTCONN)
        return std::unexpected(SError{.stage = eErrorStage::WRITE, .code = errno});

    auto reply = readAll(socket.get(), DEADLINE);
    if (!reply)
        return std::unexpected(SError{.stage = eErrorStage::READ, .code = reply.error()});

    return std::move(*reply);
}
