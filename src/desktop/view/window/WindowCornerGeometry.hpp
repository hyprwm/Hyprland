#pragma once

#include "../../../helpers/math/CornerRadii.hpp"

namespace Desktop::View {
    Math::SCornerRadii effectiveWindowRounding(const Math::SCornerRadii& rawRadii, float power, const Hyprutils::Math::Vector2D& currentSize);
    bool isInWindowCorner(const Hyprutils::Math::Vector2D& point, const Hyprutils::Math::Vector2D& size, const Math::SCornerRadii& radii, float power, int borderSize);
}
