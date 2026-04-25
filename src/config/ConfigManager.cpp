#include "ConfigManager.hpp"
#include "supplementary/jeremy/Jeremy.hpp"
#include "legacy/ConfigManager.hpp"
#include "../debug/log/Logger.hpp"

#include <hyprutils/path/Path.hpp>
#include <filesystem>

using namespace Config;

static UP<IConfigManager> g_mgr;

//
bool Config::initConfigManager() {
    if (mgr())
        return true;

    // run this bitch
    const auto CFG_PATH = Supplementary::Jeremy::getMainConfigPath();

    if (!CFG_PATH) {
        Log::logger->log(Log::CRIT, "Couldn't load config: {}", CFG_PATH.error());
        return false;
    }

    std::filesystem::path filePath = *CFG_PATH;

    // TODO:
    // filePath.replace_extension(".lua");

    g_mgr = makeUnique<Legacy::CConfigManager>();

    std::error_code ec;
    if (!std::filesystem::exists(filePath, ec) || ec) {
        if (ec) {
            Log::logger->log(Log::CRIT, "Couldn't load config: {}", ec.message());
            return false;
        }
    }

    return true;
}

UP<IConfigManager>& Config::mgr() {
    return g_mgr;
}