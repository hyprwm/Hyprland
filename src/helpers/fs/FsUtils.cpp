#include "FsUtils.hpp"
#include "../../debug/Log.hpp"

#include <cstdlib>
#include <filesystem>

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList.hpp>
using namespace Hyprutils::String;

std::optional<std::string> NFsUtils::getDataHome() {
    const auto  DATA_HOME = getenv("XDG_DATA_HOME");

    std::string dataRoot;

    if (!DATA_HOME) {
        const auto HOME = getenv("HOME");

        if (!HOME) {
            NDebug::log(ERR, "FsUtils::getDataHome: can't get data home: no $HOME or $XDG_DATA_HOME");
            return std::nullopt;
        }

        dataRoot = HOME + std::string{"/.local/share/"};
    } else
        dataRoot = DATA_HOME + std::string{"/"};

    std::error_code ec;
    if (!std::filesystem::exists(dataRoot, ec) || ec) {
        NDebug::log(ERR, "FsUtils::getDataHome: can't get data home: inaccessible / missing");
        return std::nullopt;
    }

    dataRoot += "hyprland/";

    if (!std::filesystem::exists(dataRoot, ec) || ec) {
        NDebug::log(LOG, "FsUtils::getDataHome: no hyprland data home, creating.");
        std::filesystem::create_directory(dataRoot, ec);
        if (ec) {
            NDebug::log(ERR, "FsUtils::getDataHome: can't create new data home for hyprland");
            return std::nullopt;
        }
        std::filesystem::permissions(dataRoot, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec, ec);
        if (ec)
            NDebug::log(WARN, "FsUtils::getDataHome: couldn't set perms on hyprland data store. Proceeding anyways.");
    }

    if (!std::filesystem::exists(dataRoot, ec) || ec) {
        NDebug::log(ERR, "FsUtils::getDataHome: no hyprland data home, failed to create.");
        return std::nullopt;
    }

    return dataRoot;
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
        NDebug::log(ERR, "CVersionKeeperManager: couldn't open an ofstream for writing the version file.");
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
