#include "CornerRadii.hpp"

#include "../../config/shared/complex/CornersData.hpp"
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/memory/Casts.hpp>

#include <algorithm>
#include <cmath>

using namespace Math;
using Hyprutils::Math::Vector2D;
using Hyprutils::Memory::sc;

SCornerRadii::SCornerRadii(float radius) : SCornerRadii(radius, radius, radius, radius) {
    ;
}

SCornerRadii::SCornerRadii(float tl, float tr, float br, float bl) : topLeft(tl), topRight(tr), bottomRight(br), bottomLeft(bl) {
    ;
}

SCornerRadii::SCornerRadii(const Config::CCornersData& value) : SCornerRadii(value.m_topLeft, value.m_topRight, value.m_bottomRight, value.m_bottomLeft) {
    ;
}

SCornerRadii SCornerRadii::operator*(float scale) const {
    return {topLeft * scale, topRight * scale, bottomRight * scale, bottomLeft * scale};
}

SCornerRadii SCornerRadii::operator/(float scale) const {
    return *this * (1.F / scale);
}

float SCornerRadii::max() const {
    return std::max({topLeft, topRight, bottomRight, bottomLeft});
}

bool SCornerRadii::isRounded() const {
    return max() > 0.F;
}

SCornerRadii SCornerRadii::clamped(const Vector2D& size) const {
    const float LIMIT = std::max(0.F, sc<float>(std::min(size.x, size.y) * 0.5));
    return {std::clamp(topLeft, 0.F, LIMIT), std::clamp(topRight, 0.F, LIMIT), std::clamp(bottomRight, 0.F, LIMIT), std::clamp(bottomLeft, 0.F, LIMIT)};
}

SCornerRadii SCornerRadii::inset(float amount) const {
    return expanded(-amount);
}

SCornerRadii SCornerRadii::expanded(float amount) const {
    const auto EXPAND = [amount](float radius) { return radius > 0.F ? std::max(radius + amount, 0.F) : 0.F; };
    return {EXPAND(topLeft), EXPAND(topRight), EXPAND(bottomRight), EXPAND(bottomLeft)};
}

float SCornerRadii::at(const Vector2D& point, const Vector2D& size) const {
    if (point.y < size.y * 0.5)
        return point.x < size.x * 0.5 ? topLeft : topRight;

    return point.x < size.x * 0.5 ? bottomLeft : bottomRight;
}

bool Math::isInRoundedCorner(const Vector2D& point, const Vector2D& size, const SCornerRadii& radii, float power) {
    const float RADIUS = radii.at(point, size);
    if (RADIUS <= 0.F)
        return false;

    const Vector2D DELTA = {RADIUS - std::min(point.x, size.x - point.x), RADIUS - std::min(point.y, size.y - point.y)};
    if (DELTA.x <= 0.0 || DELTA.y <= 0.0)
        return false;

    return std::pow(DELTA.x, power) + std::pow(DELTA.y, power) > std::pow(sc<double>(RADIUS), power);
}
