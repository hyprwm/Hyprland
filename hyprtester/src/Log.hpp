#pragma once
#include <string>
#include <format>
#include <print>

namespace NLog {
    template <typename... Args>
    //NOLINTNEXTLINE
    void log(std::format_string<Args...> fmt, Args&&... args) {
        std::string logMsg = "";

        logMsg += std::vformat(fmt.get(), std::make_format_args(args...));

        std::println("{}", logMsg);
        std::fflush(stdout);
    }
}