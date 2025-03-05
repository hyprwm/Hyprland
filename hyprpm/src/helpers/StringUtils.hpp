#pragma once

#include <format>
#include <string>
#include "Colors.hpp"

template <typename... Args>
std::string statusString(const std::string_view EMOJI, const std::string_view COLOR, const std::string_view FMT, Args&&... args) {
    std::string ret = std::format("{}{}{} ", COLOR, EMOJI, Colors::RESET);
    ret += std::vformat(FMT, std::make_format_args(args...));
    return ret;
}

template <typename... Args>
std::string successString(const std::string_view FMT, Args&&... args) {
    return statusString("✔", Colors::GREEN, FMT, args...);
}

template <typename... Args>
std::string failureString(const std::string_view FMT, Args&&... args) {
    return statusString("✖", Colors::RED, FMT, args...);
}

template <typename... Args>
std::string verboseString(const std::string_view FMT, Args&&... args) {
    return statusString("[v]", Colors::BLUE, FMT, args...);
}

template <typename... Args>
std::string infoString(const std::string_view FMT, Args&&... args) {
    return statusString("→", Colors::RESET, FMT, args...);
}
