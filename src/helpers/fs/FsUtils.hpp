#pragma once
#include <optional>
#include <string>

namespace NFsUtils {
    // Returns the path to the hyprland directory in data home.
    std::optional<std::string> getDataHome();

    std::optional<std::string> readFileAsString(const std::string& path);

    // overwrites the file if exists
    bool writeToFile(const std::string& path, const std::string& content);

    bool executableExistsInPath(const std::string& exe);
};
