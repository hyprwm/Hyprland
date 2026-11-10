#include "WindowGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

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
