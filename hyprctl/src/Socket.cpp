#include "Socket.hpp"

#include <cerrno>
#include <sys/socket.h>
#include <sys/time.h>

#include <hyprutils/memory/Casts.hpp>

using namespace Hyprutils::Memory;

std::string HyprCtl::Socket::frameRequest(std::string_view request) {
    const auto  SEPARATOR       = request.find('/');
    const auto  SPACE           = request.find(' ');
    const bool  NEEDS_SEPARATOR = request.starts_with("[[BATCH]]") || SEPARATOR == std::string_view::npos || (SPACE != std::string_view::npos && SEPARATOR > SPACE);
    std::string framed;
    framed.reserve(request.size() + (NEEDS_SEPARATOR ? 3 : 2));
    framed += EXPLICIT_FRAME_MARKER;

    // Old servers only ignore the marker when it is in the flag section.
    if (NEEDS_SEPARATOR)
        framed += '/';

    framed += request;
    framed += '\0';
    return framed;
}

bool HyprCtl::Socket::setTimeouts(int fd, int seconds) {
    const auto TIMEOUT = timeval{.tv_sec = seconds, .tv_usec = 0};
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &TIMEOUT, sizeof(TIMEOUT)) == 0 && setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &TIMEOUT, sizeof(TIMEOUT)) == 0;
}

bool HyprCtl::Socket::writeAll(int fd, std::string_view data) {
    size_t written = 0;

    while (written < data.size()) {
        const auto SIZE = send(fd, data.data() + written, data.size() - written, MSG_NOSIGNAL);
        if (SIZE > 0) {
            written += sc<size_t>(SIZE);
            continue;
        }

        if (SIZE < 0 && errno == EINTR)
            continue;

        if (SIZE == 0)
            errno = EIO;
        return false;
    }

    return true;
}

HyprCtl::Socket::eReadResult HyprCtl::Socket::readAll(int fd, std::string& reply) {
    constexpr size_t BUFFER_SIZE         = 8192;
    char             buffer[BUFFER_SIZE] = {};

    while (true) {
        const auto SIZE = recv(fd, buffer, sizeof(buffer), 0);
        if (SIZE > 0) {
            reply.append(buffer, sc<size_t>(SIZE));
            continue;
        }

        if (SIZE == 0)
            return eReadResult::SUCCESS;

        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return eReadResult::TIMEOUT;

        return eReadResult::ERROR;
    }
}
