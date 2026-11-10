#include "SurfaceDamage.hpp"

#include <cmath>

using namespace Hyprutils::Math;

void Render::clipSurfaceDamage(CRegion& damage, const Vector2D& size) {
    // An integral client size can leave a fractional logical strip covered by its projected texture.
    if (!damage.empty() && size != size.floor()) {
        // Whole-region unions avoid repeatedly merging individual rectangles in fragmented damage.
        damage.add(damage.copy().translate({1, 0}));
        damage.add(damage.copy().translate({-1, 0}));
        damage.add(damage.copy().translate({0, 1}));
        damage.add(damage.copy().translate({0, -1}));
    }

    damage.intersect(CBox{{}, {std::ceil(size.x), std::ceil(size.y)}});
}

CRegion Render::surfaceDamageToPixels(const CRegion& damage, const Vector2D& position, double scale) {
    auto       pixels = damage.copy().scale(scale);
    const auto OFFSET = position * scale;

    // Cover both integer translations of a fractional origin, without quantizing it in logical space.
    if (OFFSET.x != std::floor(OFFSET.x))
        pixels.add(pixels.copy().translate({1, 0}));
    if (OFFSET.y != std::floor(OFFSET.y))
        pixels.add(pixels.copy().translate({0, 1}));

    return pixels.translate(OFFSET.floor());
}
