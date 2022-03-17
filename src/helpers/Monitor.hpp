#pragma once

#include "../defines.hpp"

struct SMonitor {
    Vector2D    vecPosition     = Vector2D(0,0);
    Vector2D    vecSize         = Vector2D(0,0);

    bool        primary         = false;

    int         ID              = -1;

    std::string szName          = "";

    Vector2D    vecReservedTopLeft = Vector2D(0,0);
    Vector2D    vecReservedBottomRight = Vector2D(0,0);
};