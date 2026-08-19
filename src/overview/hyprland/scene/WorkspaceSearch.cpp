#include "WorkspaceSearch.hpp"

#include "OverviewLayout.hpp"

using namespace Overview::Hyprland;

WorkspaceSearch::SGeometry WorkspaceSearch::calculateGeometry(const Vector2D& logicalMonitorSize, float scale) {
    if (logicalMonitorSize.x <= 0 || logicalMonitorSize.y <= 0 || scale <= 0)
        return {};

    const auto LAYOUT = OverviewLayout::calculate(logicalMonitorSize, scale);
    if (LAYOUT.logicalSearch.empty() || LAYOUT.pixelSearch.empty())
        return {};

    return {.logicalBox = LAYOUT.logicalSearch, .pixelBox = LAYOUT.pixelSearch};
}
