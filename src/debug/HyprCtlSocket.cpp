#include "HyprCtlSocket.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>

#include <hyprutils/memory/Casts.hpp>

using namespace Hyprutils::Memory;

static NHyprCtlSocket::eReadResult consumeRequestBytes(NHyprCtlSocket::SRequest& request, const char* data, size_t size, size_t maxRequestSize) {
    size_t offset = 0;

    if (request.payload.empty() && !request.explicitlyFramed && size > 0 && data[0] == NHyprCtlSocket::EXPLICIT_FRAME_MARKER) {
        request.explicitlyFramed = true;
        offset                   = 1;
    }

    const auto END  = sc<const char*>(std::memchr(data + offset, '\0', size - offset));
    const auto SIZE = END ? sc<size_t>(END - data) - offset : size - offset;

    if (request.payload.size() > maxRequestSize || SIZE > maxRequestSize - request.payload.size())
        return NHyprCtlSocket::eReadResult::READ_TOO_LARGE;

    request.payload.append(data + offset, SIZE);

    if (!END)
        return NHyprCtlSocket::eReadResult::READ_INCOMPLETE;

    return END == data + size - 1 ? NHyprCtlSocket::eReadResult::READ_COMPLETE : NHyprCtlSocket::eReadResult::READ_INVALID;
}

static NHyprCtlSocket::eReadResult checkForTrailingBytes(int fd) {
    char byte = 0;

    while (true) {
        const auto SIZE = recv(fd, &byte, 1, MSG_PEEK | MSG_DONTWAIT);
        if (SIZE > 0)
            return NHyprCtlSocket::eReadResult::READ_INVALID;
        if (SIZE == 0)
            return NHyprCtlSocket::eReadResult::READ_COMPLETE;
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return NHyprCtlSocket::eReadResult::READ_COMPLETE;

        return NHyprCtlSocket::eReadResult::READ_ERROR;
    }
}

NHyprCtlSocket::eReadResult NHyprCtlSocket::readRequest(int fd, SRequest& request, size_t maxRequestSize) {
    std::array<char, 4096> buffer = {};

    while (true) {
        const auto SIZE = recv(fd, buffer.data(), buffer.size(), 0);
        if (SIZE > 0) {
            const auto RESULT = consumeRequestBytes(request, buffer.data(), sc<size_t>(SIZE), maxRequestSize);
            if (RESULT == eReadResult::READ_COMPLETE)
                return checkForTrailingBytes(fd);
            if (RESULT != eReadResult::READ_INCOMPLETE)
                return RESULT;
            continue;
        }

        if (SIZE == 0)
            return request.payload.empty() && !request.explicitlyFramed ? eReadResult::READ_CLOSED : eReadResult::READ_COMPLETE;

        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return eReadResult::READ_INCOMPLETE;

        return eReadResult::READ_ERROR;
    }
}

NHyprCtlSocket::eWriteResult NHyprCtlSocket::writeReply(int fd, std::string_view reply, size_t& written) {
    if (written > reply.size()) {
        errno = EINVAL;
        return eWriteResult::WRITE_ERROR;
    }

    while (written < reply.size()) {
        const auto SIZE = send(fd, reply.data() + written, reply.size() - written, MSG_NOSIGNAL);
        if (SIZE > 0) {
            written += sc<size_t>(SIZE);
            continue;
        }

        if (SIZE < 0 && errno == EINTR)
            continue;
        if (SIZE < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return eWriteResult::WRITE_BLOCKED;

        if (SIZE == 0)
            errno = EIO;
        return eWriteResult::WRITE_ERROR;
    }

    return eWriteResult::WRITE_COMPLETE;
}
