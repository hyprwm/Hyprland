#pragma once

#include "../../helpers/math/Math.hpp"

namespace Layout {
    CBox windowContentBox(const CBox& allocation, const SBoxExtents& reserved, const Vector2D& monitorPosition, double scale);
}
