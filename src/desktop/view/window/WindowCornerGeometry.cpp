#include "WindowCornerGeometry.hpp"

bool Desktop::View::isInWindowCorner(const Hyprutils::Math::Vector2D& point, const Hyprutils::Math::Vector2D& size, const Math::SCornerRadii& radii, float power, int borderSize) {
    if (borderSize >= radii.at(point, size))
        return false;

    return Math::isInRoundedCorner(point, size, radii, power);
}
