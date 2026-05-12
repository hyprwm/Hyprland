#pragma once

#include <wayland-server-protocol.h>

// includes box and vector as well
#include <hyprutils/math/Region.hpp>
#include <hyprutils/math/Mat3x3.hpp>

// NOLINTNEXTLINE
using namespace Hyprutils::Math;

namespace Math {
    constexpr const Vector2D VECTOR2D_MAX = {std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};

    eTransform               wlTransformToHyprutils(wl_output_transform t);
    wl_output_transform      invertTransform(wl_output_transform tr);
    eTransform               composeTransform(eTransform a, eTransform b);
}