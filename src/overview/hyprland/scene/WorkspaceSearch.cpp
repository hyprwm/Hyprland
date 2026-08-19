#include "WorkspaceSearch.hpp"

#include "WorkspaceTapeController.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>

using namespace Overview::Hyprland;
using namespace Hyprutils::Memory;

WorkspaceSearch::SGeometry WorkspaceSearch::calculateGeometry(const Vector2D& logicalMonitorSize, float scale) {
    if (logicalMonitorSize.x <= 0 || logicalMonitorSize.y <= 0 || scale <= 0)
        return {};

    const auto HEIGHT_ABOVE  = (logicalMonitorSize.y * (1.F - CWorkspaceTapeController::TILE_SCALE)) / 2.F;
    const auto SEARCH_HEIGHT = HEIGHT_ABOVE * 0.6F;
    const auto SEARCH_WIDTH  = logicalMonitorSize.x * 0.2F;

    const auto SEARCH_X = (logicalMonitorSize.x / 2.F) - (SEARCH_WIDTH / 2.F);
    const auto SEARCH_Y = (HEIGHT_ABOVE / 2.F) - (SEARCH_HEIGHT / 2.F);

    if (SEARCH_HEIGHT <= 0 || SEARCH_WIDTH <= 0)
        return {};

    const CBox LOGICAL_BOX = {{SEARCH_X, SEARCH_Y}, {SEARCH_WIDTH, SEARCH_HEIGHT}};
    const CBox PIXEL_BOX   = LOGICAL_BOX.copy().scale(scale);

    return {.logicalBox = LOGICAL_BOX, .pixelBox = PIXEL_BOX};
}
