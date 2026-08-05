#pragma once

#include <type_traits>
#include <hyprutils/memory/Casts.hpp>

#define EXPOSE_ENUM_AS_MASK(x)                                                                                                                                                     \
    constexpr x operator|(x lhs, x rhs) noexcept {                                                                                                                                 \
        using T = std::underlying_type_t<x>;                                                                                                                                       \
        return Hyprutils::Memory::sc<x>(Hyprutils::Memory::sc<T>(lhs) | Hyprutils::Memory::sc<T>(rhs));                                                                            \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    constexpr x operator&(x lhs, x rhs) noexcept {                                                                                                                                 \
        using T = std::underlying_type_t<x>;                                                                                                                                       \
        return Hyprutils::Memory::sc<x>(Hyprutils::Memory::sc<T>(lhs) & Hyprutils::Memory::sc<T>(rhs));                                                                            \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    constexpr x& operator|=(x& lhs, x rhs) noexcept {                                                                                                                              \
        return lhs = lhs | rhs;                                                                                                                                                    \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    constexpr x operator~(x value) noexcept {                                                                                                                                      \
        using T = std::underlying_type_t<x>;                                                                                                                                       \
        return Hyprutils::Memory::sc<x>(Hyprutils::Memory::sc<T>(~Hyprutils::Memory::sc<T>(value)));                                                                               \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    constexpr x& operator&=(x& lhs, x rhs) noexcept {                                                                                                                              \
        return lhs = lhs & rhs;                                                                                                                                                    \
    }
