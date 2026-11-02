#pragma once

#include <format>
#include <print>
#include <utility>

// NOLINTNEXTLINE
namespace Debug {
    template <typename... Args>
    void die(std::format_string<Args...> fmt, Args&&... args) {
        const std::string logMsg = std::format(fmt, std::forward<Args>(args)...);

        std::println("\n[ERR] {}", logMsg);
        exit(1);
    }
};