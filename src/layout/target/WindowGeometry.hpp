#pragma once

#include "../../helpers/math/Math.hpp"

namespace Layout {
    struct STargetEdges;

    CBox        windowContentBox(const CBox& allocation, const SBoxExtents& reserved, const Vector2D& monitorPosition, double scale);
    SBoxExtents mergedBorderReserved(const SBoxExtents& reserved, const STargetEdges& edges, int borderSize, float scale);
}
