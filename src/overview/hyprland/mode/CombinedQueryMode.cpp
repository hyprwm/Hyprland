#include "CombinedQueryMode.hpp"

#include "WindowQueryMode.hpp"
#include "WorkspaceQueryMode.hpp"

using namespace Overview::Hyprland::Mode;

CCombinedQueryMode::~CCombinedQueryMode() = default;

Overview::Hyprland::eQueryMode CCombinedQueryMode::type() const {
    return Overview::Hyprland::eQueryMode::ALL;
}

eWorkspaceMatch CCombinedQueryMode::matchWorkspace(std::string_view name, std::string_view query, const FWorkspaceSelector& selector) const {
    static const CWorkspaceQueryMode WORKSPACE_MODE;
    return WORKSPACE_MODE.matchWorkspace(name, query, selector);
}

bool CCombinedQueryMode::matchesWindow(std::string_view appID, std::string_view title, std::string_view query) const {
    static const CWindowQueryMode WINDOW_MODE;
    return WINDOW_MODE.matchesWindow(appID, title, query);
}
