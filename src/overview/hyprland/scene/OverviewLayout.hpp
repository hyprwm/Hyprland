#pragma once

#include <hyprutils/math/Box.hpp>
#include <hyprutils/math/Vector2D.hpp>

namespace Overview::Hyprland::OverviewLayout {
    struct SLayout {
        Hyprutils::Math::CBox logicalSearch;
        Hyprutils::Math::CBox logicalMiniStrip;
        Hyprutils::Math::CBox logicalMain;
        Hyprutils::Math::CBox pixelSearch;
        Hyprutils::Math::CBox pixelMiniStrip;
        Hyprutils::Math::CBox pixelMain;
    };

    SLayout calculate(const Hyprutils::Math::Vector2D& logicalMonitorSize, float scale);
}
