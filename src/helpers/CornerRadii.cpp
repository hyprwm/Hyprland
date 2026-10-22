#include "CornerRadii.hpp"

#include <algorithm>
#include <cmath>
#include <hyprutils/memory/Casts.hpp>

using namespace Hyprutils::Math;
using Hyprutils::Memory::sc;

SCornerRadii::SCornerRadii(float radius) : values{radius, radius, radius, radius} {
    ;
}

SCornerRadii::SCornerRadii(float topLeft, float topRight, float bottomRight, float bottomLeft) : values{topLeft, topRight, bottomRight, bottomLeft} {
    ;
}

bool SCornerRadii::uniform() const {
    return std::ranges::all_of(values, [this](float radius) { return radius == values.front(); });
}

float SCornerRadii::max() const {
    return std::ranges::max(values);
}

SCornerRadii SCornerRadii::operator*(double scale) const {
    auto result = *this;
    for (auto& radius : result.values)
        radius *= scale;
    return result;
}

SCornerRadii SCornerRadii::operator-(float inset) const {
    auto result = *this;
    for (auto& radius : result.values)
        radius = std::max(0.F, radius - inset);
    return result;
}

SCornerRadii SCornerRadii::expanded(float amount) const {
    auto result = *this;
    for (auto& radius : result.values)
        if (radius > 0.F)
            radius = std::max(0.F, radius + amount);
    return result;
}

SCornerRadii SCornerRadii::truncated() const {
    auto result = *this;
    for (auto& radius : result.values)
        radius = std::trunc(radius);
    return result;
}

SCornerRadii SCornerRadii::clamped(const Vector2D& size) const {
    auto       result = *this;
    const auto limit  = std::max(0.F, sc<float>(std::min(size.x, size.y) / 2.0));
    for (auto& radius : result.values)
        radius = std::clamp(radius, 0.F, limit);
    return result;
}

bool SCornerRadii::contains(const Vector2D& point, const Vector2D& size, float power) const {
    const bool right  = point.x >= size.x / 2.0;
    const bool bottom = point.y >= size.y / 2.0;
    const auto radius = values[bottom ? (right ? 2 : 3) : (right ? 1 : 0)];
    const auto x      = radius - (right ? size.x - point.x : point.x);
    const auto y      = radius - (bottom ? size.y - point.y : point.y);
    if (radius <= 0 || x <= 0 || y <= 0)
        return true;
    return std::pow(x / radius, power) + std::pow(y / radius, power) <= 1.0;
}

std::array<CRegion, 4> cornerRegions(const CBox& box, const CRegion& damage, bool uniform) {
    std::array<CRegion, 4> result;
    if (uniform) {
        result[0] = damage;
        return result;
    }

    const auto                extents   = damage.copy().getExtents();
    const auto                x         = std::clamp(std::floor(box.x + box.w / 2.0 + 0.5), extents.x, extents.x + extents.w);
    const auto                y         = std::clamp(std::floor(box.y + box.h / 2.0 + 0.5), extents.y, extents.y + extents.h);
    const std::array<CBox, 4> quadrants = {
        CBox{extents.x, extents.y, x - extents.x, y - extents.y},
        CBox{x, extents.y, extents.x + extents.w - x, y - extents.y},
        CBox{x, y, extents.x + extents.w - x, extents.y + extents.h - y},
        CBox{extents.x, y, x - extents.x, extents.y + extents.h - y},
    };
    for (size_t i = 0; i < result.size(); ++i)
        result[i] = damage.copy().intersect(quadrants[i]);
    return result;
}
