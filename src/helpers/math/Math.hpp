#pragma once

#include <wayland-server-protocol.h>

// includes box and vector as well
#include <hyprutils/math/Region.hpp>
#include <hyprutils/math/Mat3x3.hpp>

// NOLINTNEXTLINE
using namespace Hyprutils::Math;

eTransform          wlTransformToHyprutils(wl_output_transform t);
wl_output_transform invertTransform(wl_output_transform tr);
