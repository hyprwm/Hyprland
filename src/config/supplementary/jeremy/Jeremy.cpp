#include "Jeremy.hpp"

#include "../../../Compositor.hpp"
#include "../../ConfigManager.hpp"

#include <hyprutils/path/Path.hpp>
#include <filesystem>

using namespace Config;
using namespace Config::Supplementary;
using namespace Config::Supplementary::Jeremy;

std::expected<SConfigStateReply, std::string> Jeremy::getMainConfigPath() {
    static bool lastSafeMode = g_pCompositor->m_safeMode;
    static auto getCfgPath   = []() -> std::expected<SConfigStateReply, std::string> {
        lastSafeMode = g_pCompositor->m_safeMode;

        if (g_pCompositor->m_safeMode) {
            const std::filesystem::path CONFIGPATH = g_pCompositor->m_instancePath + "/recoverycfg.conf";
            auto                        v          = Config::mgr()->generateDefaultConfig(CONFIGPATH);
            if (!v)
                return std::unexpected("safe mode: failed to generate config");
            return SConfigStateReply{CONFIGPATH.string(), CONFIG_TYPE_SPECIAL};
        }

        if (!g_pCompositor->m_explicitConfigPath.empty())
            return SConfigStateReply{g_pCompositor->m_explicitConfigPath, CONFIG_TYPE_EXPLICIT};

        if (const auto CFG_ENV = getenv("HYPRLAND_CONFIG"); CFG_ENV)
            return SConfigStateReply{CFG_ENV, CONFIG_TYPE_EXPLICIT};

        const auto PATHS = Hyprutils::Path::findConfig(ISDEBUG ? "hyprlandd" : "hyprland");
        if (PATHS.first.has_value()) {
            return SConfigStateReply{PATHS.first.value(), CONFIG_TYPE_REGULAR};
        } else if (PATHS.second.has_value()) {
            auto CONFIGPATH = Hyprutils::Path::fullConfigPath(PATHS.second.value(), ISDEBUG ? "hyprlandd" : "hyprland");
            return SConfigStateReply{CONFIGPATH, CONFIG_TYPE_REGULAR};
        } else
            return std::unexpected("Neither HOME nor XDG_CONFIG_HOME are set in the environment. Could not find config in XDG_CONFIG_DIRS or /etc/xdg.");
    };
    static auto CONFIG_PATH = getCfgPath();

    if (lastSafeMode != g_pCompositor->m_safeMode)
        CONFIG_PATH = getCfgPath();

    return CONFIG_PATH;
}
