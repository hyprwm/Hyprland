#include "WorkspaceQueryMode.hpp"

#include "../StringUtils.hpp"

#include <algorithm>
#include <cctype>

using namespace Overview::Hyprland::Mode;

static bool looksLikeSelector(std::string_view query) {
    const auto FIRST = query.find_first_not_of(" \t\n\r");
    if (FIRST == std::string_view::npos)
        return false;

    const auto LAST = query.find_last_not_of(" \t\n\r");
    query           = query.substr(FIRST, LAST - FIRST + 1);

    if (query.starts_with("name:") || query.starts_with("special"))
        return true;

    if (!query.empty() && std::ranges::all_of(query, [](unsigned char c) { return std::isdigit(c); }))
        return true;

    if (query.size() < 2)
        return false;

    constexpr std::string_view SELECTORS = "rsnmwf";
    return SELECTORS.contains(query.front()) && query[1] == '[';
}

CWorkspaceQueryMode::~CWorkspaceQueryMode() = default;

Overview::Hyprland::eQueryMode CWorkspaceQueryMode::type() const {
    return Overview::Hyprland::eQueryMode::WORKSPACE;
}

eWorkspaceMatch CWorkspaceQueryMode::matchWorkspace(std::string_view name, std::string_view query, const FWorkspaceSelector& selector) const {
    if (StringUtils::fullMatchCaseIns(name, query))
        return eWorkspaceMatch::EXACT;

    if (StringUtils::matchesName(name, query))
        return eWorkspaceMatch::MATCH;

    if (selector && looksLikeSelector(query) && selector(query))
        return eWorkspaceMatch::MATCH;

    return eWorkspaceMatch::NONE;
}

bool CWorkspaceQueryMode::matchesWindow(std::string_view, std::string_view, std::string_view) const {
    return false;
}
