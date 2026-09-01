#include "WorkspacePointerMapping.hpp"

#include <cmath>

using namespace Overview::Hyprland;
using Hyprutils::Math::CBox;
using Hyprutils::Math::Vector2D;

std::optional<Vector2D> WorkspacePointerMapping::mapClamped(const Vector2D& point, const CBox& tileBox, const CBox& sourceBox) {
    if (tileBox.empty() || sourceBox.empty() || !std::isfinite(point.x) || !std::isfinite(point.y))
        return std::nullopt;

    const Vector2D NORMALIZED = Vector2D{(point.x - tileBox.x) / tileBox.w, (point.y - tileBox.y) / tileBox.h}.clamp({}, {1, 1});
    const Vector2D SOURCE_MAX = {std::nextafter(sourceBox.x + sourceBox.w, sourceBox.x), std::nextafter(sourceBox.y + sourceBox.h, sourceBox.y)};
    const Vector2D MAPPED     = (sourceBox.pos() + NORMALIZED * sourceBox.size()).clamp(sourceBox.pos(), SOURCE_MAX);

    if (!std::isfinite(MAPPED.x) || !std::isfinite(MAPPED.y))
        return std::nullopt;

    return MAPPED;
}
