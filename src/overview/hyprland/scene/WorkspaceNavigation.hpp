#pragma once

#include <cstdint>

namespace Overview::Hyprland {
    enum class eWorkspaceNavigationResult : uint8_t {
        NONE,
        EXISTING,
        CREATED,
    };
}
