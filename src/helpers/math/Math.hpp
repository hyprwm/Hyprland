#pragma once

#include <wayland-server-protocol.h>

// includes box and vector as well
#include <hyprutils/math/Region.hpp>

using namespace Hyprutils::Math;

eTransform wlTransformToHyprutils(wl_output_transform t);
void       projectBox(float mat[9], CBox& box, eTransform transform, float rotation, const float projection[9]);
