#pragma once

#include <optional>

#include <hyprutils/math/Box.hpp>

namespace Overview::Hyprland::WorkspaceTileShadow {
    struct SGeometry {
        Hyprutils::Math::CBox outerBox;
        int                   range    = 0;
        int                   rounding = 0;
        float                 opacity  = 0.F;
    };

    std::optional<SGeometry> calculate(const Hyprutils::Math::CBox& tileBox, float monitorScale, float tileOpacity, float overviewProgress, float range, float rounding);
}
