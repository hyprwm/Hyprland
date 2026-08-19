#pragma once

#include "../../../helpers/math/Math.hpp"

namespace Overview::Hyprland::WorkspaceSearch {
    struct SGeometry {
        CBox logicalBox;
        CBox pixelBox;
    };

    SGeometry calculateGeometry(const Vector2D& logicalMonitorSize, float scale);
}
