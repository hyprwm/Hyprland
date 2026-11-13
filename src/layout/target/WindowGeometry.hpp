#pragma once

#include "../../helpers/math/Math.hpp"

namespace Layout {
    SBoxExtents gapExtentsInPixels(const SBoxExtents& gaps, float scale);
    CBox        boxWithPixelGaps(const CBox& allocation, const SBoxExtents& pixelGaps, const Vector2D& monitorPosition, double scale);
    CBox        windowContentBox(const CBox& allocation, const SBoxExtents& reserved, const Vector2D& monitorPosition, double scale);
}
