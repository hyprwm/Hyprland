#include "ConfigManager.hpp"
#include "supplementary/jeremy/Jeremy.hpp"
#include "lua/ConfigManager.hpp"
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
        Log::logger->log(Log::CRIT, "[cfg] Couldn't load config: {}", CFG_PATH.error());
        return false;
    }

    std::filesystem::path filePath = CFG_PATH->path;

    if (CFG_PATH->type == Supplementary::Jeremy::CONFIG_TYPE_REGULAR) {
        Log::logger->log(Log::DEBUG, "[cfg] Regular config at {}", filePath.string());
        g_mgr = makeUnique<Lua::CConfigManager>();
    } else {
        Log::logger->log(Log::DEBUG, "[cfg] Config is either explicit or special");
        g_mgr = makeUnique<Lua::CConfigManager>();
    }

    RASSERT(g_mgr, "failed to create a suitable config manager");

    std::error_code ec;
    if (!std::filesystem::exists(filePath, ec) || ec) {
        if (ec) {
            Log::logger->log(Log::CRIT, "[cfg] Couldn't load config: {}", ec.message());
            return false;
        }

        // generate default
        if (const auto v = g_mgr->generateDefaultConfig(filePath); !v) {
            Log::logger->log(Log::CRIT, "[cfg] Couldn't generate default config: {}", v.error());
            return false;
        }
    }

    return true;
}

UP<IConfigManager>& Config::mgr() {
    return g_mgr;
}

const char* Config::typeToString(eConfigManagerType t) {
    switch (t) {
        case CONFIG_LUA: return "lua";
        default: return "error";
    }
}