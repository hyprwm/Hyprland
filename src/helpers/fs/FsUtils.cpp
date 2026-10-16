#include "FsUtils.hpp"
#include "../../debug/log/Logger.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList.hpp>
using namespace Hyprutils::String;

std::optional<std::string> NFsUtils::getDataHome() {
    const char* const     XDGDATA = std::getenv("XDG_DATA_HOME");
    std::filesystem::path basePath;

    if (XDGDATA != nullptr && *XDGDATA != '\0') {
        basePath = XDGDATA;
        if (!basePath.is_absolute()) {
            LOG(Log::WARN, "FsUtils::getDataHome: $XDG_DATA_HOME is relative ('{}'), falling back to $HOME", basePath.string());
            basePath.clear();
        }
    }

    if (basePath.empty()) {
        const char* const HOME = std::getenv("HOME");
        if (HOME == nullptr || *HOME == '\0') {
            LOG(Log::ERR, "FsUtils::getDataHome: can't get data home: neither $XDG_DATA_HOME nor $HOME is set");
            return std::nullopt;
        }

        basePath = std::filesystem::path(HOME) / ".local" / "share";
    }

    const std::filesystem::path hyprlandDataDir = basePath / "hyprland";
    std::error_code             ec;

    const bool                  created = std::filesystem::create_directories(hyprlandDataDir, ec);
    if (ec) {
        LOG(Log::ERR, "FsUtils::getDataHome: failed to create data directory '{}': {}", hyprlandDataDir.string(), ec.message());
        return std::nullopt;
    }

    if (!std::filesystem::is_directory(hyprlandDataDir, ec) || ec) {
        LOG(Log::ERR, "FsUtils::getDataHome: path '{}' exists, but is not a directory", hyprlandDataDir.string());
        return std::nullopt;
    }

    if (created) {
        LOG(Log::DEBUG, "FsUtils::getDataHome: created new hyprland data directory: {}", hyprlandDataDir.string());
        std::filesystem::permissions(hyprlandDataDir, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, ec);

        if (ec) {
            LOG(Log::WARN, "FsUtils::getDataHome: couldn't set perms on '{}': {}. Proceeding anyways.", hyprlandDataDir.string(), ec.message());
        }
    }

    return hyprlandDataDir.string();
}

std::optional<std::string> NFsUtils::readFileAsString(const std::string& path) {
    std::error_code ec;

    if (!std::filesystem::exists(path, ec) || ec)
        return std::nullopt;

    std::ifstream file(path);
    if (!file.good())
        return std::nullopt;

    return trim(std::string((std::istreambuf_iterator<char>(file)), (std::istreambuf_iterator<char>())));
}

bool NFsUtils::writeToFile(const std::string& path, const std::string& content) {
    std::ofstream of(path, std::ios::trunc);
    if (!of.good()) {
        LOG(Log::ERR, "CVersionKeeperManager: couldn't open an ofstream for writing the version file.");
        return false;
    }

    of << content;
    of.close();

    return true;
}

bool NFsUtils::executableExistsInPath(const std::string& exe) {
    if (!getenv("PATH"))
        return false;

    static CVarList paths(getenv("PATH"), 0, ':', true);

    for (auto& p : paths) {
        std::string     path = p + std::string{"/"} + exe;
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec)
            continue;

        if (!std::filesystem::is_regular_file(path, ec) || ec)
            continue;

        auto stat = std::filesystem::status(path, ec);
        if (ec)
            continue;

        auto perms = stat.permissions();

        return std::filesystem::perms::none != (perms & std::filesystem::perms::others_exec);
    }

    return false;
}
