#include "WorkspaceTileShadow.hpp"

#include <algorithm>
#include <cmath>

using namespace Overview::Hyprland;
using Hyprutils::Math::CBox;

std::optional<WorkspaceTileShadow::SGeometry> WorkspaceTileShadow::calculate(const CBox& tileBox, float monitorScale, float tileOpacity, float overviewProgress, float range,
                                                                             float rounding) {
    if (tileBox.empty() || monitorScale <= 0.F)
        return std::nullopt;

    const float PROGRESS = std::clamp(overviewProgress, 0.F, 1.F);
    const int   RANGE    = std::lround(std::max(0.F, range) * monitorScale * PROGRESS);
    const float OPACITY  = std::min(std::clamp(tileOpacity, 0.F, 1.F), PROGRESS);
    if (RANGE <= 0 || OPACITY <= 0.F)
        return std::nullopt;

    return SGeometry{
        .outerBox = tileBox.copy().expand(RANGE).round(),
        .range    = RANGE,
        .rounding = std::lround(std::max(0.F, rounding) * monitorScale * PROGRESS),
        .opacity  = OPACITY,
    };
}
