#pragma once

extern "C" {
#include <xkbcommon/xkbcommon.h>
}

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "../helpers/memory/Memory.hpp"

namespace Keybinds {

    enum eKeyboardModifiers : uint8_t {
        HL_MODIFIER_SHIFT = (1 << 0),
        HL_MODIFIER_CAPS  = (1 << 1),
        HL_MODIFIER_CTRL  = (1 << 2),
        HL_MODIFIER_ALT   = (1 << 3),
        HL_MODIFIER_MOD2  = (1 << 4),
        HL_MODIFIER_MOD3  = (1 << 5),
        HL_MODIFIER_META  = (1 << 6),
        HL_MODIFIER_MOD5  = (1 << 7),
    };

    using ModifierMask = std::underlying_type_t<eKeyboardModifiers>;

    struct SKeysymPattern {
        xkb_keysym_t sym = 0;
    };

    struct SKeycodePattern {
        std::optional<xkb_keysym_t> sym;
        xkb_keycode_t               code = 0;
    };

    struct SSidedModifierPattern {
        xkb_keysym_t       sym     = 0;
        eKeyboardModifiers modMask = sc<eKeyboardModifiers>(0);
    };

    struct SExternalEventPattern {
        std::string ev;
    };

    using KeyEvent = std::variant<SKeysymPattern, SKeycodePattern, SSidedModifierPattern, SExternalEventPattern>;

    struct SResolvedKey {
        xkb_keysym_t                      sym      = 0;
        xkb_keycode_t                     code     = 0;
        std::optional<eKeyboardModifiers> modifier = std::nullopt;
        std::optional<std::string>        event    = std::nullopt;
    };

    class CKey {
      public:
        CKey(eKeyboardModifiers modifier, const std::string& sided = "");
        CKey(xkb_keycode_t code);
        CKey(const std::string& string);
        ~CKey() = default;

        bool                              matches(KeyEvent event, xkb_state* relative) const;
        bool                              matches(const SResolvedKey& key) const;
        bool                              isMod() const;
        std::optional<xkb_keysym_t>       keysym() const;
        std::optional<xkb_keycode_t>      keycode() const;
        std::optional<eKeyboardModifiers> modifier() const;
        std::optional<std::string_view>   event() const;
        bool                              valid() const;

      private:
        bool                              empty() const;

        std::optional<xkb_keysym_t>       m_sym;
        std::optional<xkb_keycode_t>      m_code;
        std::optional<eKeyboardModifiers> m_mod;
        std::optional<std::string>        m_event;
    };
};
