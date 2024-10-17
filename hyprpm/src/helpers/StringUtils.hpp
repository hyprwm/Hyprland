#pragma once

#include <format>
#include <string>
#include "Colors.hpp"

template <typename... Args>
std::string statusString(const std::string_view emoji, const std::string_view color, const std::string_view fmt, Args&&... args) {
    std::string ret = std::format("{}{}{} ", color, emoji, Colors::RESET);
    ret += std::vformat(fmt, std::make_format_args(args...));
    return ret;
}

template <typename... Args>
std::string successString(const std::string_view fmt, Args&&... args) {
    return statusString("✔", Colors::GREEN, fmt, args...);
}

template <typename... Args>
std::string failureString(const std::string_view fmt, Args&&... args) {
    return statusString("✖", Colors::RED, fmt, args...);
}

template <typename... Args>
std::string verboseString(const std::string_view fmt, Args&&... args) {
    return statusString("[v]", Colors::BLUE, fmt, args...);
}

template <typename... Args>
std::string infoString(const std::string_view fmt, Args&&... args) {
    return statusString("→", Colors::RESET, fmt, args...);
}
