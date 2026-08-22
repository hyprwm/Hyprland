#pragma once

#include <optional>

#include <hyprutils/math/Box.hpp>
#include <hyprutils/math/Vector2D.hpp>

namespace Overview::Hyprland::WorkspacePointerMapping {
    std::optional<Hyprutils::Math::Vector2D> mapClamped(const Hyprutils::Math::Vector2D& point, const Hyprutils::Math::CBox& tileBox, const Hyprutils::Math::CBox& sourceBox);
}
