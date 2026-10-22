#pragma once

#include <array>
#include <hyprutils/math/Box.hpp>
#include <hyprutils/math/Region.hpp>

// Clockwise, starting at the top left. Zero keeps a corner square.
struct SCornerRadii {
    SCornerRadii(float radius = 0.F);
    SCornerRadii(float topLeft, float topRight, float bottomRight, float bottomLeft);

    std::array<float, 4> values;

    bool                 uniform() const;
    float                max() const;
    SCornerRadii         operator*(double scale) const;
    SCornerRadii         operator-(float inset) const;
    SCornerRadii         expanded(float amount) const;
    SCornerRadii         truncated() const;
    SCornerRadii         clamped(const Hyprutils::Math::Vector2D& size) const;
    bool                 contains(const Hyprutils::Math::Vector2D& point, const Hyprutils::Math::Vector2D& size, float power) const;
};

// Partition damage without overlaps, including pixels outside the box (shadows).
// The original region is returned unchanged for uniform radii.
std::array<Hyprutils::Math::CRegion, 4> cornerRegions(const Hyprutils::Math::CBox& box, const Hyprutils::Math::CRegion& damage, bool uniform);
