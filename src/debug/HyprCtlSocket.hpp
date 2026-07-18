#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace NHyprCtlSocket {
    inline constexpr size_t MAX_REQUEST_SIZE = size_t{1024} * 1024;

    // Explicit clients place this before the flag separator, so old servers
    // ignore it as an unknown flag while new servers skip quiet-period framing.
    inline constexpr char EXPLICIT_FRAME_MARKER = '\x1F';

    enum class eReadResult : uint8_t {
        READ_INCOMPLETE = 0,
        READ_COMPLETE,
        READ_CLOSED,
        READ_TOO_LARGE,
        READ_INVALID,
        READ_ERROR,
    };

    enum class eWriteResult : uint8_t {
        WRITE_COMPLETE = 0,
        WRITE_BLOCKED,
        WRITE_ERROR,
    };

    struct SRequest {
        std::string payload;
        bool        explicitlyFramed = false;
    };

    eReadResult  readRequest(int fd, SRequest& request, size_t maxRequestSize = MAX_REQUEST_SIZE);
    eWriteResult writeReply(int fd, std::string_view reply, size_t& written);
}
