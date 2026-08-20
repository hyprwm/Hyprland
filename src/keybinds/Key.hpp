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
#include "../input/Keys.hpp"

namespace Keybinds {

    struct SKeysymPattern {
        xkb_keysym_t sym = 0;
    };

    struct SKeycodePattern {
        std::optional<xkb_keysym_t> sym;
        xkb_keycode_t               code = 0;
    };

    struct SSidedModifierPattern {
        xkb_keysym_t              sym     = 0;
        Input::eKeyboardModifiers modMask = Input::HL_MODIFIER_NONE;
    };

    struct SExternalEventPattern {
        std::string ev;
    };

    using KeyEvent = std::variant<SKeysymPattern, SKeycodePattern, SSidedModifierPattern, SExternalEventPattern>;

    struct SResolvedKey {
        xkb_keysym_t                             sym      = 0;
        xkb_keycode_t                            code     = 0;
        std::optional<Input::eKeyboardModifiers> modifier = std::nullopt;
        std::optional<std::string>               event    = std::nullopt;
    };

    class CKey {
      public:
        CKey(Input::eKeyboardModifiers modifier, const std::string& sided = "");
        CKey(xkb_keycode_t code);
        CKey(const std::string& string);
        ~CKey() = default;

        bool                                     matches(KeyEvent event, xkb_state* relative) const;
        bool                                     matches(const SResolvedKey& key) const;
        bool                                     isMod() const;
        std::optional<xkb_keysym_t>              keysym() const;
        std::optional<xkb_keycode_t>             keycode() const;
        std::optional<Input::eKeyboardModifiers> modifier() const;
        std::optional<std::string_view>          event() const;
        bool                                     valid() const;

      private:
        bool                                     empty() const;

        std::optional<xkb_keysym_t>              m_sym;
        std::optional<xkb_keycode_t>             m_code;
        std::optional<Input::eKeyboardModifiers> m_mod;
        std::optional<std::string>               m_event;
    };
};
