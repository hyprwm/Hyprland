#pragma once

#include "../../../helpers/math/CornerRadii.hpp"

namespace Desktop::View {
    bool isInWindowCorner(const Hyprutils::Math::Vector2D& point, const Hyprutils::Math::Vector2D& size, const Math::SCornerRadii& radii, float power, int borderSize);
}
