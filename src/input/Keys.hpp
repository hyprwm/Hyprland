#pragma once

#include <cstdint>
#include <type_traits>

#include "../helpers/memory/Memory.hpp"

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

    constexpr eKeyboardModifiers operator|(eKeyboardModifiers lhs, eKeyboardModifiers rhs) noexcept {
        using T = std::underlying_type_t<eKeyboardModifiers>;
        return sc<eKeyboardModifiers>(sc<T>(lhs) | sc<T>(rhs));
    }

    constexpr eKeyboardModifiers operator&(eKeyboardModifiers lhs, eKeyboardModifiers rhs) noexcept {
        using T = std::underlying_type_t<eKeyboardModifiers>;
        return sc<eKeyboardModifiers>(sc<T>(lhs) & sc<T>(rhs));
    }

    constexpr eKeyboardModifiers& operator|=(eKeyboardModifiers& lhs, eKeyboardModifiers rhs) noexcept {
        return lhs = lhs | rhs;
    }

    using ModifierMask = eKeyboardModifiers;
};
