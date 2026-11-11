#pragma once

#include "../../helpers/math/Math.hpp"

namespace Layout {
    struct STargetEdges;

    CBox roundTargetBox(const CBox& box);
    CBox windowContentBox(const CBox& allocation, const SBoxExtents& reserved, const Vector2D& monitorPosition, double scale);
    CBox mergedWindowContentBox(const CBox& allocation, const SBoxExtents& reserved, const STargetEdges& edges, int borderSize, const Vector2D& monitorPosition, float scale);
    SBoxExtents mergedBorderReserved(const SBoxExtents& reserved, const STargetEdges& edges, int borderSize, float scale);
}
