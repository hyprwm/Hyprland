#pragma once

#include "../../../macros/Enums.hpp"

#include <cstdint>
#include <limits>
#include <type_traits>

namespace Desktop::View {
    // shoutout Ford Focus Mk2 2007
    enum class eFocusBlockReason : uint8_t {
        FOCUS_BLOCK_NONE             = 0,
        FOCUS_BLOCK_GROUP_INACTIVE   = (1 << 0),
        FOCUS_BLOCK_MONOCLE_INACTIVE = (1 << 1),
        FOCUS_BLOCK_BELOW_FULLSCREEN = (1 << 2),

        FOCUS_BLOCK_ALL = std::numeric_limits<std::underlying_type_t<eFocusBlockReason>>::max(),
    };

    using enum eFocusBlockReason;
    EXPOSE_ENUM_AS_MASK(eFocusBlockReason, FocusBlockReasons)
}
