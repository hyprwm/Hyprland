#pragma once
#include <string>
#include <format>
#include <print>
#include <utility>

// Stolen from hyprutils
namespace Colors {
    constexpr const char* RED     = "\x1b[31m";
    constexpr const char* GREEN   = "\x1b[32m";
    constexpr const char* YELLOW  = "\x1b[33m";
    constexpr const char* BLUE    = "\x1b[34m";
    constexpr const char* MAGENTA = "\x1b[35m";
    constexpr const char* CYAN    = "\x1b[36m";
    constexpr const char* RESET   = "\x1b[0m";
};

namespace NLog {
    template <typename... Args>
    // NOLINTNEXTLINE
    void log(std::format_string<Args...> fmt, Args&&... args) {
        std::string logMsg = "";

        logMsg += std::format(fmt, std::forward<Args>(args)...);

        std::println("{}{}", logMsg, Colors::RESET);
        std::fflush(stdout);
    }

    template <typename... Args>
    // NOLINTNEXTLINE
    void yellow(std::format_string<Args...> fmt, Args&&... args) {
        auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
        log("{}{}", Colors::YELLOW, msg);
    }

    template <typename... Args>
    // NOLINTNEXTLINE
    void green(std::format_string<Args...> fmt, Args&&... args) {
        auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
        log("{}{}", Colors::GREEN, msg);
    }

    template <typename... Args>
    // NOLINTNEXTLINE
    void red(std::format_string<Args...> fmt, Args&&... args) {
        auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
        log("{}{}", Colors::RED, msg);
    }
}
