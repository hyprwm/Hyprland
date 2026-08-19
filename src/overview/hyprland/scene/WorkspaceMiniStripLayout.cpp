#include "WorkspaceMiniStripLayout.hpp"

#include <algorithm>

using namespace Overview::Hyprland;
using Hyprutils::Math::CBox;
using Hyprutils::Math::Vector2D;

static Vector2D fittedTileSize(const CBox& bounds, const Vector2D& sourceSize) {
    if (sourceSize.x <= 0.F || sourceSize.y <= 0.F)
        return bounds.size();

    return sourceSize * (bounds.h / sourceSize.y);
}

std::vector<CBox> WorkspaceMiniStripLayout::calculate(const CBox& bounds, std::span<const Vector2D> sourceSizes, size_t selectedIndex, float gapScale) {
    if (bounds.empty() || sourceSizes.empty())
        return {};

    const double      GAP      = bounds.h * std::max(0.F, gapScale);
    const size_t      SELECTED = std::min(selectedIndex, sourceSizes.size() - 1);

    std::vector<CBox> boxes;
    boxes.reserve(sourceSizes.size());

    double contentWidth = 0.F;
    for (const auto& sourceSize : sourceSizes) {
        const auto SIZE = fittedTileSize(bounds, sourceSize);
        boxes.emplace_back(Vector2D{}, SIZE);
        contentWidth += SIZE.x;
    }
    contentWidth += GAP * (boxes.size() - 1);

    double x = bounds.x + (bounds.w - contentWidth) / 2.F;
    if (contentWidth > bounds.w) {
        double selectedCenter = 0.F;
        for (size_t i = 0; i < SELECTED; ++i)
            selectedCenter += boxes.at(i).w + GAP;
        selectedCenter += boxes.at(SELECTED).w / 2.F;

        x = bounds.x + bounds.w / 2.F - selectedCenter;
        x = std::clamp(x, bounds.x + bounds.w - contentWidth, bounds.x);
    }

    for (auto& box : boxes) {
        box.x = x;
        box.y = bounds.y + (bounds.h - box.h) / 2.F;
        x += box.w + GAP;
    }

    return boxes;
}
