#pragma once

#include <hyprutils/math/Region.hpp>

namespace Render {
    void                     clipSurfaceDamage(Hyprutils::Math::CRegion& damage, const Hyprutils::Math::Vector2D& size);
    Hyprutils::Math::CRegion surfaceDamageToPixels(const Hyprutils::Math::CRegion& damage, const Hyprutils::Math::Vector2D& position, double scale);
}
