#include "WorkspaceTapeLayout.hpp"

#include <algorithm>

using namespace Overview::Hyprland;
using Hyprutils::Math::CBox;
using Hyprutils::Math::Vector2D;

static Vector2D fittedTileSize(const Vector2D& regularSize, const Vector2D& sourceSize) {
    if (sourceSize.x <= 0.F || sourceSize.y <= 0.F)
        return regularSize;

    const float SCALE = std::min(regularSize.x / sourceSize.x, regularSize.y / sourceSize.y);
    return sourceSize * SCALE;
}

std::vector<CBox> WorkspaceTapeLayout::calculate(const CBox& bounds, std::span<const Vector2D> sourceSizes, size_t selectedIndex, float gapScale) {
    if (bounds.empty() || sourceSizes.empty())
        return {};

    const Vector2D    REGULAR_SIZE = bounds.size();
    const float       GAP          = bounds.w * gapScale;
    const size_t      SELECTED     = std::min(selectedIndex, sourceSizes.size() - 1);

    std::vector<CBox> boxes;
    boxes.reserve(sourceSizes.size());
    for (const auto& sourceSize : sourceSizes)
        boxes.emplace_back(Vector2D{}, fittedTileSize(REGULAR_SIZE, sourceSize));

    boxes.at(SELECTED).x = bounds.x + (bounds.w - boxes.at(SELECTED).w) / 2.F;
    boxes.at(SELECTED).y = bounds.y + (bounds.h - boxes.at(SELECTED).h) / 2.F;

    for (size_t i = SELECTED; i > 0; --i) {
        boxes.at(i - 1).x = boxes.at(i).x - GAP - boxes.at(i - 1).w;
        boxes.at(i - 1).y = bounds.y + (bounds.h - boxes.at(i - 1).h) / 2.F;
    }

    for (size_t i = SELECTED + 1; i < boxes.size(); ++i) {
        boxes.at(i).x = boxes.at(i - 1).x + boxes.at(i - 1).w + GAP;
        boxes.at(i).y = bounds.y + (bounds.h - boxes.at(i).h) / 2.F;
    }

    return boxes;
}
