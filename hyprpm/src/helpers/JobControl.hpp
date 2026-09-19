#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace NJobControl {
    constexpr size_t                        MAX_JOBS = 256;

    std::expected<size_t, std::string_view> parse(std::string_view value);
    std::string                             buildEnvironment(size_t jobs);
    std::string                             pluginBuildCommand(std::string_view directory, std::string_view environment, std::string_view buildStep, size_t jobs);
    std::string                             headersInstallCommand(std::string_view workingDirectory, std::string_view headersPath, size_t jobs);
}
