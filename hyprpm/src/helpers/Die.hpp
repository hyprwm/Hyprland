#pragma once

#include <format>
#include <iostream>
#include <utility>

// NOLINTNEXTLINE
namespace Debug {
    template <typename... Args>
    void die(std::format_string<Args...> fmt, Args&&... args) {
        const std::string logMsg = std::format(fmt, std::forward<Args>(args)...);

        std::cout << "\n[ERR] " << logMsg << "\n";
        exit(1);
    }
};