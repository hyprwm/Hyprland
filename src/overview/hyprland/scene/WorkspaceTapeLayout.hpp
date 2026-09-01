#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <hyprutils/math/Box.hpp>
#include <hyprutils/math/Vector2D.hpp>

namespace Overview::Hyprland::WorkspaceTapeLayout {
    std::vector<Hyprutils::Math::CBox> calculate(const Hyprutils::Math::CBox& bounds, std::span<const Hyprutils::Math::Vector2D> sourceSizes, size_t selectedIndex, float gapScale);
}
