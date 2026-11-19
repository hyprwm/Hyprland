#include "WindowCornerGeometry.hpp"

Math::SCornerRadii Desktop::View::effectiveWindowRounding(const Math::SCornerRadii& rawRadii, float power, const Hyprutils::Math::Vector2D& currentSize) {
    // Make perceived roundness consistent, then constrain it to the current geometry.
    return (rawRadii * (power / 2.F)).clamped(currentSize);
}

bool Desktop::View::isInWindowCorner(const Hyprutils::Math::Vector2D& point, const Hyprutils::Math::Vector2D& size, const Math::SCornerRadii& radii, float power, int borderSize) {
    if (borderSize >= radii.at(point, size))
        return false;

    return Math::isInRoundedCorner(point, size, radii, power);
}
