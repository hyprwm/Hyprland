#pragma once
#include <string>
#include <format>
#include <print>

#include "shared.hpp"

namespace NLog {
    template <typename... Args>
    // NOLINTNEXTLINE
    void log(std::format_string<Args...> fmt, Args&&... args) {
        std::string logMsg = "";

        logMsg += std::vformat(fmt.get(), std::make_format_args(args...));

        std::println("{}{}", logMsg, Colors::RESET);
        std::fflush(stdout);
    }

    /// Logs message with yellow
    template <typename... Args>
    // NOLINTNEXTLINE
    void alert(std::format_string<Args...> fmt, Args&&... args) {
        auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
        log("{}{}", Colors::YELLOW, msg);
    }

    /// Logs message with green
    template <typename... Args>
    // NOLINTNEXTLINE
    void info(std::format_string<Args...> fmt, Args&&... args) {
        auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
        log("{}{}", Colors::GREEN, msg);
    }

    /// Logs message with red
    template <typename... Args>
    // NOLINTNEXTLINE
    void error(std::format_string<Args...> fmt, Args&&... args) {
        auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
        log("{}{}", Colors::RED, msg);
    }
}
