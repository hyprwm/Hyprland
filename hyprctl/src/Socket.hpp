#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace HyprCtl::Socket {
    // The marker lives in the flag section so legacy servers ignore it.
    inline constexpr char EXPLICIT_FRAME_MARKER = '\x1F';

    enum class eReadResult : uint8_t {
        SUCCESS = 0,
        TIMEOUT,
        ERROR,
    };

    std::string frameRequest(std::string_view request);
    bool        setTimeouts(int fd, int seconds);
    bool        writeAll(int fd, std::string_view data);
    eReadResult readAll(int fd, std::string& reply);
}
