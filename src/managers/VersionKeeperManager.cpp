#include "VersionKeeperManager.hpp"
#include "../debug/Log.hpp"
#include "../macros.hpp"
#include "../version.h"
#include "../helpers/MiscFunctions.hpp"
#include "../helpers/varlist/VarList.hpp"
#include "eventLoop/EventLoopManager.hpp"
#include "../config/ConfigValue.hpp"

#include <filesystem>
#include <fstream>
#include <hyprutils/string/String.hpp>
#include <hyprutils/os/Process.hpp>

using namespace Hyprutils::String;
using namespace Hyprutils::OS;

constexpr const char* VERSION_FILE_NAME = "lastVersion";

CVersionKeeperManager::CVersionKeeperManager() {
    static auto PNONOTIFY = CConfigValue<Hyprlang::INT>("ecosystem:no_update_news");

    const auto  DATAROOT = getDataHome();

    if (!DATAROOT)
        return;

    const auto LASTVER = getDataLastVersion(*DATAROOT);

    if (!LASTVER)
        return;

    if (!isVersionOlderThanRunning(*LASTVER)) {
        Debug::log(LOG, "CVersionKeeperManager: Read version {} matches or is older than running.", *LASTVER);
        return;
    }

    writeVersionToVersionFile(*DATAROOT);

    if (*PNONOTIFY) {
        Debug::log(LOG, "CVersionKeeperManager: updated, but update news is disabled in the config :(");
        return;
    }

    if (!executableExistsInPath("hyprland-update-screen")) {
        Debug::log(ERR, "CVersionKeeperManager: hyprland-update-screen doesn't seem to exist, skipping notif about update...");
        return;
    }

    g_pEventLoopManager->doLater([]() {
        CProcess proc("hyprland-update-screen", {"--new-version", HYPRLAND_VERSION});
        proc.runAsync();
    });
}

std::optional<std::string> CVersionKeeperManager::getDataHome() {
    const auto  DATA_HOME = getenv("XDG_DATA_HOME");

    std::string dataRoot;

    if (!DATA_HOME) {
        const auto HOME = getenv("HOME");

        if (!HOME) {
            Debug::log(ERR, "CVersionKeeperManager: can't get data home: no $HOME or $XDG_DATA_HOME");
            return std::nullopt;
        }

        dataRoot = HOME + std::string{"/.local/share/"};
    } else
        dataRoot = DATA_HOME + std::string{"/"};

    std::error_code ec;
    if (!std::filesystem::exists(dataRoot, ec) || ec) {
        Debug::log(ERR, "CVersionKeeperManager: can't get data home: inaccessible / missing");
        return std::nullopt;
    }

    dataRoot += "hyprland/";

    if (!std::filesystem::exists(dataRoot, ec) || ec) {
        Debug::log(LOG, "CVersionKeeperManager: no hyprland data home, creating.");
        std::filesystem::create_directory(dataRoot, ec);
        if (ec) {
            Debug::log(ERR, "CVersionKeeperManager: can't create new data home for hyprland");
            return std::nullopt;
        }
        std::filesystem::permissions(dataRoot, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec, ec);
        if (ec)
            Debug::log(WARN, "CVersionKeeperManager: couldn't set perms on hyprland data store. Proceeding anyways.");
    }

    if (!std::filesystem::exists(dataRoot, ec) || ec) {
        Debug::log(ERR, "CVersionKeeperManager: no hyprland data home, failed to create.");
        return std::nullopt;
    }

    return dataRoot;
}

std::optional<std::string> CVersionKeeperManager::getDataLastVersion(const std::string& dataRoot) {
    std::error_code ec;
    std::string     lastVerFile = dataRoot + "/" + VERSION_FILE_NAME;

    if (!std::filesystem::exists(lastVerFile, ec) || ec) {
        Debug::log(LOG, "CVersionKeeperManager: no hyprland last version file, creating.");
        writeVersionToVersionFile(dataRoot);

        return HYPRLAND_VERSION;
    }

    std::ifstream file(lastVerFile);
    if (!file.good()) {
        Debug::log(ERR, "CVersionKeeperManager: couldn't open an ifstream for reading the version file.");
        return std::nullopt;
    }

    return trim(std::string((std::istreambuf_iterator<char>(file)), (std::istreambuf_iterator<char>())));
}

void CVersionKeeperManager::writeVersionToVersionFile(const std::string& dataRoot) {
    std::string   lastVerFile = dataRoot + "/" + VERSION_FILE_NAME;
    std::ofstream of(lastVerFile, std::ios::trunc);
    if (!of.good()) {
        Debug::log(ERR, "CVersionKeeperManager: couldn't open an ofstream for writing the version file.");
        return;
    }

    of << HYPRLAND_VERSION;
    of.close();
}

bool CVersionKeeperManager::isVersionOlderThanRunning(const std::string& ver) {
    const CVarList        verStrings(ver, 0, '.', true);

    const int             V1 = configStringToInt(verStrings[0]).value_or(0);
    const int             V2 = configStringToInt(verStrings[1]).value_or(0);
    const int             V3 = configStringToInt(verStrings[2]).value_or(0);

    static const CVarList runningStrings(HYPRLAND_VERSION, 0, '.', true);

    static const int      R1 = configStringToInt(runningStrings[0]).value_or(0);
    static const int      R2 = configStringToInt(runningStrings[1]).value_or(0);
    static const int      R3 = configStringToInt(runningStrings[2]).value_or(0);

    if (R1 > V1)
        return true;
    if (R2 > V2)
        return true;
    if (R3 > V3)
        return true;
    return false;
}
