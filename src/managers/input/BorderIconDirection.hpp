#pragma once

#include <cstdint>

namespace Math {
    struct SCornerRadii;
}

namespace Hyprutils::Math {
    class Vector2D;
}

enum eBorderIconDirection : uint8_t {
    BORDERICON_NONE = 0,
    BORDERICON_UP,
    BORDERICON_DOWN,
    BORDERICON_LEFT,
    BORDERICON_RIGHT,
    BORDERICON_UP_LEFT,
    BORDERICON_DOWN_LEFT,
    BORDERICON_UP_RIGHT,
    BORDERICON_DOWN_RIGHT,
};

namespace Input {
    eBorderIconDirection borderIconDirection(const Hyprutils::Math::Vector2D& point, const Hyprutils::Math::Vector2D& size, const Math::SCornerRadii& rounding, int borderSize);
}
