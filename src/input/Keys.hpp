#pragma once

#include <cstdint>

#include "../macros/Enums.hpp"

namespace Input {
    enum class eKeyboardModifiers : uint8_t {
        HL_MODIFIER_NONE  = 0,
        HL_MODIFIER_SHIFT = (1 << 0),
        HL_MODIFIER_CAPS  = (1 << 1),
        HL_MODIFIER_CTRL  = (1 << 2),
        HL_MODIFIER_ALT   = (1 << 3),
        HL_MODIFIER_MOD2  = (1 << 4),
        HL_MODIFIER_MOD3  = (1 << 5),
        HL_MODIFIER_META  = (1 << 6),
        HL_MODIFIER_MOD5  = (1 << 7),
    };

    using enum eKeyboardModifiers;
    using ModifierMask = eKeyboardModifiers;

    EXPOSE_ENUM_AS_MASK(eKeyboardModifiers)
};
