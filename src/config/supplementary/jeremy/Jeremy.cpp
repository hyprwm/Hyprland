#include "Jeremy.hpp"

#include "../../../Compositor.hpp"

#include <hyprutils/path/Path.hpp>
#include <filesystem>

using namespace Config::Supplementary;

std::expected<std::string, std::string> Config::Supplementary::Jeremy::getMainConfigPath() {
    static bool lastSafeMode = g_pCompositor->m_safeMode;
    static auto getCfgPath   = []() -> std::expected<std::string, std::string> {
        lastSafeMode = g_pCompositor->m_safeMode;

        if (g_pCompositor->m_safeMode)
            return (std::filesystem::path{g_pCompositor->m_instancePath} / "recoverycfg.conf").string();

        if (!g_pCompositor->m_explicitConfigPath.empty())
            return g_pCompositor->m_explicitConfigPath;

        if (const auto CFG_ENV = getenv("HYPRLAND_CONFIG"); CFG_ENV)
            return CFG_ENV;

        const auto PATHS = Hyprutils::Path::findConfig(ISDEBUG ? "hyprlandd" : "hyprland");
        if (PATHS.first.has_value()) {
            return PATHS.first.value();
        } else if (PATHS.second.has_value()) {
            auto CONFIGPATH = Hyprutils::Path::fullConfigPath(PATHS.second.value(), ISDEBUG ? "hyprlandd" : "hyprland");
            return CONFIGPATH;
        } else
            return std::unexpected("Neither HOME nor XDG_CONFIG_HOME are set in the environment. Could not find config in XDG_CONFIG_DIRS or /etc/xdg.");
    };
    static auto CONFIG_PATH = getCfgPath();

    if (lastSafeMode != g_pCompositor->m_safeMode)
        CONFIG_PATH = getCfgPath();

    return CONFIG_PATH;
}
