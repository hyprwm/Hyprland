#pragma once

#include <type_traits>
#include <hyprutils/memory/Casts.hpp>

#define EXPOSE_ENUM_AS_MASK(x, name)                                                                                                                                               \
    class name {                                                                                                                                                                   \
      public:                                                                                                                                                                      \
        constexpr name(x value) : m_value(value) {}                                                                                                                                \
                                                                                                                                                                                   \
        template <typename T>                                                                                                                                                      \
            requires std::is_integral_v<T>                                                                                                                                         \
        constexpr explicit name(T value) : m_value(Hyprutils::Memory::sc<x>(value)) {}                                                                                             \
                                                                                                                                                                                   \
        constexpr explicit operator bool() const noexcept {                                                                                                                        \
            using U = std::underlying_type_t<x>;                                                                                                                                   \
            return Hyprutils::Memory::sc<U>(m_value) != 0;                                                                                                                         \
        }                                                                                                                                                                          \
                                                                                                                                                                                   \
        template <typename T>                                                                                                                                                      \
            requires(std::is_integral_v<T> && !std::is_same_v<T, bool>)                                                                                                            \
        constexpr explicit operator T() const noexcept {                                                                                                                           \
            return Hyprutils::Memory::sc<T>(m_value);                                                                                                                              \
        }                                                                                                                                                                          \
                                                                                                                                                                                   \
        constexpr x value() const noexcept {                                                                                                                                       \
            return m_value;                                                                                                                                                        \
        }                                                                                                                                                                          \
                                                                                                                                                                                   \
        constexpr bool operator==(const name&) const noexcept = default;                                                                                                           \
                                                                                                                                                                                   \
      private:                                                                                                                                                                     \
        x m_value;                                                                                                                                                                 \
    };                                                                                                                                                                             \
                                                                                                                                                                                   \
    constexpr name operator&(name lhs, name rhs) noexcept {                                                                                                                        \
        using U = std::underlying_type_t<x>;                                                                                                                                       \
                                                                                                                                                                                   \
        return Hyprutils::Memory::sc<x>(Hyprutils::Memory::sc<U>(lhs.value()) & Hyprutils::Memory::sc<U>(rhs.value()));                                                            \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    constexpr name operator&(x lhs, x rhs) noexcept {                                                                                                                              \
        return name{lhs} & name{rhs};                                                                                                                                              \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    constexpr name operator~(name bit) noexcept {                                                                                                                                  \
        using U = std::underlying_type_t<x>;                                                                                                                                       \
                                                                                                                                                                                   \
        return Hyprutils::Memory::sc<x>(~Hyprutils::Memory::sc<U>(bit.value()));                                                                                                   \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    constexpr name operator~(x bit) noexcept {                                                                                                                                     \
        return ~name{bit};                                                                                                                                                         \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    constexpr name& operator&=(name& lhs, name rhs) noexcept {                                                                                                                     \
        using U = std::underlying_type_t<x>;                                                                                                                                       \
                                                                                                                                                                                   \
        lhs = Hyprutils::Memory::sc<x>(Hyprutils::Memory::sc<U>(lhs.value()) & Hyprutils::Memory::sc<U>(rhs.value()));                                                             \
        return lhs;                                                                                                                                                                \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    constexpr name& operator|=(name& lhs, name rhs) noexcept {                                                                                                                     \
        using U = std::underlying_type_t<x>;                                                                                                                                       \
                                                                                                                                                                                   \
        lhs = Hyprutils::Memory::sc<x>(Hyprutils::Memory::sc<U>(lhs.value()) | Hyprutils::Memory::sc<U>(rhs.value()));                                                             \
        return lhs;                                                                                                                                                                \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    constexpr name operator|(name lhs, name rhs) noexcept {                                                                                                                        \
        using U = std::underlying_type_t<x>;                                                                                                                                       \
                                                                                                                                                                                   \
        return Hyprutils::Memory::sc<x>(Hyprutils::Memory::sc<U>(lhs.value()) | Hyprutils::Memory::sc<U>(rhs.value()));                                                            \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    constexpr name operator|(x lhs, x rhs) noexcept {                                                                                                                              \
        return name{lhs} | name{rhs};                                                                                                                                              \
    }
