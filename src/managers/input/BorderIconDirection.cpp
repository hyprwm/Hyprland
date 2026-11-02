#include "BorderIconDirection.hpp"
#include "../../helpers/math/CornerRadii.hpp"

#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/memory/Casts.hpp>
#include <algorithm>

using namespace Hyprutils::Memory;

eBorderIconDirection Input::borderIconDirection(const Hyprutils::Math::Vector2D& point, const Hyprutils::Math::Vector2D& size, const Math::SCornerRadii& rounding, int borderSize) {
    // Keep the 10 px corner leeway, including for square corners.
    const auto TOPLEFT     = sc<int>(rounding.topLeft) + borderSize + 10;
    const auto TOPRIGHT    = sc<int>(rounding.topRight) + borderSize + 10;
    const auto BOTTOMRIGHT = sc<int>(rounding.bottomRight) + borderSize + 10;
    const auto BOTTOMLEFT  = sc<int>(rounding.bottomLeft) + borderSize + 10;

    if (point.x < TOPLEFT && point.y < TOPLEFT)
        return BORDERICON_UP_LEFT;
    if (point.x > size.x - TOPRIGHT && point.y < TOPRIGHT)
        return BORDERICON_UP_RIGHT;
    if (point.y < std::min(TOPLEFT, TOPRIGHT))
        return BORDERICON_UP;
    if (point.x < BOTTOMLEFT && point.y > size.y - BOTTOMLEFT)
        return BORDERICON_DOWN_LEFT;
    if (point.x > size.x - BOTTOMRIGHT && point.y > size.y - BOTTOMRIGHT)
        return BORDERICON_DOWN_RIGHT;
    if (point.y > size.y - std::min(BOTTOMLEFT, BOTTOMRIGHT))
        return BORDERICON_DOWN;
    if (point.x < std::min(TOPLEFT, BOTTOMLEFT))
        return BORDERICON_LEFT;
    if (point.x > size.x - std::min(TOPRIGHT, BOTTOMRIGHT))
        return BORDERICON_RIGHT;

    return BORDERICON_NONE;
}
