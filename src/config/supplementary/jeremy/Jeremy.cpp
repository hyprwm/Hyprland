#include "Jeremy.hpp"

#include "../../../Compositor.hpp"

#include <hyprutils/path/Path.hpp>
#include <filesystem>

using namespace Config;
using namespace Config::Supplementary;
using namespace Config::Supplementary::Jeremy;

std::expected<SConfigStateReply, std::string> Jeremy::getMainConfigPath() {
    static bool lastSafeMode = g_pCompositor->m_safeMode;

    static auto regularOrLuaIfAvail = [](std::filesystem::path p) -> std::filesystem::path {
        std::error_code ec;
        auto            p2 = p;
        p2.replace_extension(".lua");
        if (std::filesystem::exists(p2, ec) && !ec)
            return p2;
        return p;
    };

    static auto getCfgPath = []() -> std::expected<SConfigStateReply, std::string> {
        lastSafeMode = g_pCompositor->m_safeMode;

        if (g_pCompositor->m_safeMode)
            return SConfigStateReply{(std::filesystem::path{g_pCompositor->m_instancePath} / "recoverycfg.conf").string(), CONFIG_TYPE_SPECIAL};

        if (!g_pCompositor->m_explicitConfigPath.empty())
            return SConfigStateReply{g_pCompositor->m_explicitConfigPath, CONFIG_TYPE_EXPLICIT};

        if (const auto CFG_ENV = getenv("HYPRLAND_CONFIG"); CFG_ENV)
            return SConfigStateReply{CFG_ENV, CONFIG_TYPE_EXPLICIT};

        const auto PATHS = Hyprutils::Path::findConfig(ISDEBUG ? "hyprlandd" : "hyprland");
        if (PATHS.first.has_value()) {
            return SConfigStateReply{regularOrLuaIfAvail(PATHS.first.value()), CONFIG_TYPE_REGULAR};
        } else if (PATHS.second.has_value()) {
            auto CONFIGPATH = Hyprutils::Path::fullConfigPath(PATHS.second.value(), ISDEBUG ? "hyprlandd" : "hyprland");
            return SConfigStateReply{regularOrLuaIfAvail(CONFIGPATH), CONFIG_TYPE_REGULAR};
        } else
            return std::unexpected("Neither HOME nor XDG_CONFIG_HOME are set in the environment. Could not find config in XDG_CONFIG_DIRS or /etc/xdg.");
    };
    static auto CONFIG_PATH = getCfgPath();

    if (lastSafeMode != g_pCompositor->m_safeMode)
        CONFIG_PATH = getCfgPath();

    return CONFIG_PATH;
}
