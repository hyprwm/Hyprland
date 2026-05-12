#include "Jeremy.hpp"

#include "../../../Compositor.hpp"

#include <hyprutils/path/Path.hpp>
#include <filesystem>

using namespace Config;
using namespace Config::Supplementary;
using namespace Config::Supplementary::Jeremy;

std::expected<SConfigStateReply, std::string> Jeremy::getMainConfigPath() {
    static bool lastSafeMode = g_pCompositor->m_safeMode;

    static auto getCfgPath = []() -> std::expected<SConfigStateReply, std::string> {
        lastSafeMode = g_pCompositor->m_safeMode;

        if (g_pCompositor->m_safeMode)
            return SConfigStateReply{.path = (std::filesystem::path{g_pCompositor->m_instancePath} / "recoverycfg.conf").string(), .type = CONFIG_TYPE_SPECIAL};

        if (!g_pCompositor->m_explicitConfigPath.empty())
            return SConfigStateReply{.path = g_pCompositor->m_explicitConfigPath, .type = CONFIG_TYPE_EXPLICIT};

        if (const auto CFG_ENV = getenv("HYPRLAND_CONFIG"); CFG_ENV)
            return SConfigStateReply{.path = CFG_ENV, .type = CONFIG_TYPE_EXPLICIT};

        const auto LUA_PATHS  = Hyprutils::Path::findConfig(ISDEBUG ? "hyprlandd" : "hyprland", "lua");
        const auto CONF_PATHS = Hyprutils::Path::findConfig(ISDEBUG ? "hyprlandd" : "hyprland", "conf");

        if (LUA_PATHS.first.has_value())
            return SConfigStateReply{.path = LUA_PATHS.first.value(), .type = CONFIG_TYPE_REGULAR};
        else if (CONF_PATHS.first.has_value())
            return SConfigStateReply{.path = CONF_PATHS.first.value(), .type = CONFIG_TYPE_REGULAR};
        else if (LUA_PATHS.second.has_value()) {
            auto CONFIGPATH = Hyprutils::Path::fullConfigPath(LUA_PATHS.second.value(), ISDEBUG ? "hyprlandd" : "hyprland");
            return SConfigStateReply{.path = CONFIGPATH, .type = CONFIG_TYPE_REGULAR};
        } else
            return std::unexpected("Neither HOME nor XDG_CONFIG_HOME are set in the environment. Could not find config in XDG_CONFIG_DIRS or /etc/xdg.");
    };
    static auto CONFIG_PATH = getCfgPath();

    if (lastSafeMode != g_pCompositor->m_safeMode)
        CONFIG_PATH = getCfgPath();

    return CONFIG_PATH;
}
