#pragma once

#include <format>
#include <iostream>

// NOLINTNEXTLINE
namespace Debug {
    template <typename... Args>
    void die(std::format_string<Args...> fmt, Args&&... args) {
        const std::string logMsg = std::vformat(fmt.get(), std::make_format_args(args...));

        std::cout << "\n[ERR] " << logMsg << "\n";
        exit(1);
    }
};