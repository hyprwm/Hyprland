#pragma once
#include <string>
#include <format>
#include <print>

#include "shared.hpp"

namespace NLog {
    template <typename... Args>
    //NOLINTNEXTLINE
    void log(std::format_string<Args...> fmt, Args&&... args) {
        std::string logMsg = "";

        logMsg += std::vformat(fmt.get(), std::make_format_args(args...));

        std::println("{}{}", logMsg, Colors::RESET);
        std::fflush(stdout);
    }

    template <typename... Args>
    //NOLINTNEXTLINE
    void info(std::format_string<Args...> fmt, Args&&... args) {
        std::string logMsg = "";

        logMsg += std::vformat(fmt.get(), std::make_format_args(args...));

        std::println("{}{}{}", Colors::YELLOW, logMsg, Colors::RESET);
        std::fflush(stdout);
    }

    template <typename... Args>
    //NOLINTNEXTLINE
    void error(std::format_string<Args...> fmt, Args&&... args) {
        std::string logMsg = "";

        logMsg += std::vformat(fmt.get(), std::make_format_args(args...));

        std::println("{}{}{}", Colors::RED, logMsg, Colors::RESET);
        std::fflush(stdout);
    }
}
