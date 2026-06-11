#pragma once

#include <cstdint>

namespace Monitor {
    // Enum for the different types of auto directions, e.g. auto-left, auto-up.
    enum eAutoDirs : uint8_t {
        DIR_AUTO_NONE = 0, /* None will be treated as right. */
        DIR_AUTO_UP,
        DIR_AUTO_DOWN,
        DIR_AUTO_LEFT,
        DIR_AUTO_RIGHT,
        DIR_AUTO_CENTER_UP,
        DIR_AUTO_CENTER_DOWN,
        DIR_AUTO_CENTER_LEFT,
        DIR_AUTO_CENTER_RIGHT
    };
}
