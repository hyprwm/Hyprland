#pragma once

#include "../desktop/DesktopTypes.hpp"
#include <hyprutils/math/Vector2D.hpp>

namespace Render {
    Hyprutils::Math::Vector2D subsurfaceScale(const Hyprutils::Math::Vector2D& windowSize, const Hyprutils::Math::Vector2D& reportedSize, bool animating);
    Hyprutils::Math::Vector2D windowSubsurfaceScale(PHLWINDOW window);
}
