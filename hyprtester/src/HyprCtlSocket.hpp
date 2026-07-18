#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace NHyprTester::HyprCtlSocket {
    // Old servers ignore this as an unknown flag; new servers use it to opt
    // into NUL-delimited request framing without a quiet-period heuristic.
    inline constexpr char EXPLICIT_FRAME_MARKER = '\x1F';

    enum class eErrorStage : uint8_t {
        SOCKET = 0,
        CONNECT,
        WRITE,
        READ,
    };

    struct SError {
        eErrorStage stage = eErrorStage::SOCKET;
        int         code  = 0;
    };

    // timeout bounds the complete connect, write, and reply exchange.
    std::expected<std::string, SError> request(std::string_view socketPath, std::string_view command, std::chrono::milliseconds timeout);
}
