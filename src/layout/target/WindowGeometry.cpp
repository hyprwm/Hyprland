#include "WindowGeometry.hpp"
#include "TargetEdges.hpp"
#include "../../render/decorations/BorderGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <hyprutils/memory/Casts.hpp>

using namespace Hyprutils::Memory;

static double canonicalDimension(double value) {
    const auto INTEGER   = std::round(value);
    const auto TOLERANCE = std::numeric_limits<float>::epsilon() * std::max(1.0, std::abs(value));
    return std::abs(value - INTEGER) <= TOLERANCE ? INTEGER : value;
}

CBox Layout::windowContentBox(const CBox& allocation, const SBoxExtents& reserved, const Vector2D& monitorPosition, double scale) {
    const auto OUTER        = allocation.copy().translate(-monitorPosition).scale(scale).round();
    const auto TOP_LEFT     = (reserved.topLeft * scale).round();
    const auto BOTTOM_RIGHT = (reserved.bottomRight * scale).round();

    // Reserve whole physical pixels inside the allocation, then retain the fractional logical geometry.
    const CBox INNER   = {OUTER.pos() + TOP_LEFT, OUTER.size() - TOP_LEFT - BOTTOM_RIGHT};
    auto       logical = INNER.copy().scale(1.0 / scale);

    // Monitor scales are floats. Do not turn an integral client size into e.g. 249.99999 and configure 249.
    auto canonical = logical;
    canonical.w    = canonicalDimension(logical.w);
    canonical.h    = canonicalDimension(logical.h);
    if (canonical.copy().scale(scale).round() == INNER)
        logical = canonical;

    return logical.translate(monitorPosition);
}

SBoxExtents Layout::mergedBorderReserved(const SBoxExtents& reserved, const STargetEdges& edges, int borderSize, float scale) {
    const auto PIXELS = Render::borderSizeInPixels(borderSize, scale);
    // Opposing edges must reclaim complementary halves, including odd physical widths.
    const auto LOW    = (PIXELS / 2) / sc<double>(scale);
    const auto HIGH   = (PIXELS - PIXELS / 2) / sc<double>(scale);
    auto       result = reserved;

    if (edges.left == eTargetEdge::INNER)
        result.topLeft.x -= LOW;
    if (edges.top == eTargetEdge::INNER)
        result.topLeft.y -= LOW;
    if (edges.right == eTargetEdge::INNER)
        result.bottomRight.x -= HIGH;
    if (edges.bottom == eTargetEdge::INNER)
        result.bottomRight.y -= HIGH;

    return result;
}
