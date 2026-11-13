#include "WindowGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

static double roundPixelCoordinate(double value, double magnitude) {
    const auto HALF      = std::floor(value) + 0.5;
    const auto TOLERANCE = 8 * std::numeric_limits<double>::epsilon() * std::max(1.0, magnitude);
    return std::round(std::abs(value - HALF) <= TOLERANCE ? HALF : value);
}

static CBox pixelBox(const CBox& allocation, const Vector2D& monitorPosition, double scale) {
    const auto START = (allocation.pos() - monitorPosition) * scale;
    const auto END   = (allocation.pos() - monitorPosition + allocation.size()) * scale;

    // Equivalent shared edges can differ by roundoff at half-pixel ties. Include the source magnitudes
    // in the allowance, since subtracting the monitor origin can leave a near-zero local coordinate.
    const auto     X_MAGNITUDE  = (std::abs(allocation.x) + std::abs(monitorPosition.x) + std::abs(allocation.w)) * scale;
    const auto     Y_MAGNITUDE  = (std::abs(allocation.y) + std::abs(monitorPosition.y) + std::abs(allocation.h)) * scale;
    const Vector2D TOP_LEFT     = {roundPixelCoordinate(START.x, X_MAGNITUDE), roundPixelCoordinate(START.y, Y_MAGNITUDE)};
    const Vector2D BOTTOM_RIGHT = {roundPixelCoordinate(END.x, X_MAGNITUDE), roundPixelCoordinate(END.y, Y_MAGNITUDE)};
    return {TOP_LEFT, BOTTOM_RIGHT - TOP_LEFT};
}

SBoxExtents Layout::gapExtentsInPixels(const SBoxExtents& gaps, float scale) {
    return {
        {Math::scaledPixelLength(gaps.topLeft.x, scale), Math::scaledPixelLength(gaps.topLeft.y, scale)},
        {Math::scaledPixelLength(gaps.bottomRight.x, scale), Math::scaledPixelLength(gaps.bottomRight.y, scale)},
    };
}

double Layout::innerGapPairInPixels(double gap, int borderSize, float scale) {
    if (gap >= 0 || borderSize <= 0)
        return 2 * Math::scaledPixelLength(gap, scale);

    const auto BORDER = Math::scaledPixelLength(borderSize, scale);
    if (BORDER == 0)
        return 2 * Math::scaledPixelLength(gap, scale);

    // Negative gaps describe overlap relative to the rendered border: half a border on each
    // side overlaps one border, while a full border on each side makes the content edges meet.
    return std::round(2 * (gap / borderSize) * BORDER);
}

SBoxExtents Layout::innerGapExtentsInPixels(const SBoxExtents& gaps, int borderSize, float scale) {
    // Assign an odd overlap pixel to left/top edges, independently of focus or scrolling direction.
    return {
        {std::floor(innerGapPairInPixels(gaps.topLeft.x, borderSize, scale) / 2), std::floor(innerGapPairInPixels(gaps.topLeft.y, borderSize, scale) / 2)},
        {std::ceil(innerGapPairInPixels(gaps.bottomRight.x, borderSize, scale) / 2), std::ceil(innerGapPairInPixels(gaps.bottomRight.y, borderSize, scale) / 2)},
    };
}

CBox Layout::boxWithPixelGaps(const CBox& allocation, const SBoxExtents& pixelGaps, const Vector2D& monitorPosition, double scale) {
    const auto OUTER = pixelBox(allocation, monitorPosition, scale);
    // Apply whole-pixel offsets only after snapping the common allocation edges.
    CBox gapped = {OUTER.pos() + pixelGaps.topLeft, OUTER.size() - pixelGaps.topLeft - pixelGaps.bottomRight};
    return gapped.scale(1.0 / scale).translate(monitorPosition);
}

static double canonicalDimension(double value) {
    const auto INTEGER   = std::round(value);
    const auto TOLERANCE = std::numeric_limits<float>::epsilon() * std::max(1.0, std::abs(value));
    return std::abs(value - INTEGER) <= TOLERANCE ? INTEGER : value;
}

CBox Layout::windowContentBox(const CBox& allocation, const SBoxExtents& reserved, const Vector2D& monitorPosition, double scale) {
    const auto OUTER        = pixelBox(allocation, monitorPosition, scale);
    const auto TOP_LEFT     = (reserved.topLeft * scale).round();
    const auto BOTTOM_RIGHT = (reserved.bottomRight * scale).round();

    // Reserve whole physical pixels inside the allocation, then retain the fractional logical geometry.
    const CBox INNER   = {OUTER.pos() + TOP_LEFT, OUTER.size() - TOP_LEFT - BOTTOM_RIGHT};
    auto       logical = INNER.copy().scale(1.0 / scale);

    // Monitor scales are floats. Do not turn an integral client size into e.g. 249.99999 and configure 249.
    auto canonical = logical;
    canonical.w    = canonicalDimension(logical.w);
    canonical.h    = canonicalDimension(logical.h);
    if (pixelBox(canonical, {}, scale) == INNER)
        logical = canonical;

    return logical.translate(monitorPosition);
}
