#include "JobControl.hpp"

#include <charconv>
#include <format>

std::expected<size_t, std::string_view> NJobControl::parse(std::string_view value) {
    size_t jobs          = 0;
    const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), jobs);
    if (ec != std::errc{} || ptr != value.data() + value.size() || jobs == 0 || jobs > MAX_JOBS)
        return std::unexpected("job count must be between 1 and 256");

    return jobs;
}

std::string NJobControl::buildEnvironment(size_t jobs) {
    if (jobs == 0)
        return {};

    return std::format("export CMAKE_BUILD_PARALLEL_LEVEL={} MAKEFLAGS=-j{}; ", jobs, jobs);
}

std::string NJobControl::pluginBuildCommand(std::string_view directory, std::string_view environment, std::string_view buildStep, size_t jobs) {
    if (jobs == 0)
        return std::format("cd {} && {} {}", directory, environment, buildStep);

    return std::format("cd {} && ({} {})", directory, environment, buildStep);
}

std::string NJobControl::headersInstallCommand(std::string_view workingDirectory, std::string_view headersPath, size_t jobs) {
    return std::format("make{} -C '{}' installheaders && chmod -R 644 '{}' && find '{}' -type d -exec chmod a+x {{}} \\;", jobs > 0 ? std::format(" -j{}", jobs) : "",
                       workingDirectory, headersPath, headersPath);
}
