#include "VersionKeeperManager.hpp"
#include "../debug/Log.hpp"
#include "../macros.hpp"
#include "../version.h"
#include "../helpers/MiscFunctions.hpp"
#include "../helpers/varlist/VarList.hpp"
#include "eventLoop/EventLoopManager.hpp"
#include "../config/ConfigValue.hpp"
#include "../helpers/fs/FsUtils.hpp"

#include <filesystem>
#include <fstream>
#include <hyprutils/string/String.hpp>
#include <hyprutils/os/Process.hpp>

using namespace Hyprutils::String;
using namespace Hyprutils::OS;

constexpr const char* VERSION_FILE_NAME = "lastVersion";

CVersionKeeperManager::CVersionKeeperManager() {
    static auto PNONOTIFY = CConfigValue<Hyprlang::INT>("ecosystem:no_update_news");

    const auto  DATAROOT = NFsUtils::getDataHome();

    if (!DATAROOT)
        return;

    const auto LASTVER = NFsUtils::readFileAsString(*DATAROOT + "/" + VERSION_FILE_NAME);

    if (!LASTVER)
        return;

    if (!isVersionOlderThanRunning(*LASTVER)) {
        NDebug::log(LOG, "CVersionKeeperManager: Read version {} matches or is older than running.", *LASTVER);
        return;
    }

    NFsUtils::writeToFile(*DATAROOT + "/" + VERSION_FILE_NAME, HYPRLAND_VERSION);

    if (*PNONOTIFY) {
        NDebug::log(LOG, "CVersionKeeperManager: updated, but update news is disabled in the config :(");
        return;
    }

    if (!NFsUtils::executableExistsInPath("hyprland-update-screen")) {
        NDebug::log(ERR, "CVersionKeeperManager: hyprland-update-screen doesn't seem to exist, skipping notif about update...");
        return;
    }

    m_bFired = true;

    g_pEventLoopManager->doLater([]() {
        CProcess proc("hyprland-update-screen", {"--new-version", HYPRLAND_VERSION});
        proc.runAsync();
    });
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

bool CVersionKeeperManager::fired() {
    return m_bFired;
}
