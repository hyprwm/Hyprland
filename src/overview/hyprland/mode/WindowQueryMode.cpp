#include "WindowQueryMode.hpp"

#include "../StringUtils.hpp"

using namespace Overview::Hyprland::Mode;

CWindowQueryMode::~CWindowQueryMode() = default;

Overview::Hyprland::eQueryMode CWindowQueryMode::type() const {
    return Overview::Hyprland::eQueryMode::WINDOW;
}

eWorkspaceMatch CWindowQueryMode::matchWorkspace(std::string_view, std::string_view, const FWorkspaceSelector&) const {
    return eWorkspaceMatch::NONE;
}

bool CWindowQueryMode::matchesWindow(std::string_view appID, std::string_view title, std::string_view query) const {
    return StringUtils::matchesName(appID, query) || StringUtils::matchesName(title, query);
}
