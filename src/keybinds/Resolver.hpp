#pragma once

#include "Key.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Keybinds {

    std::optional<Input::eKeyboardModifiers> modifierFromString(std::string_view modifier);
    Input::ModifierMask                      modMaskFromString(std::string modifiers);
    std::vector<std::string>                 modMaskToKeyNames(Input::ModifierMask modMask);

    class CResolver {
      public:
        std::expected<uint32_t, std::string> resolveKeycode(const std::string& key);
        void                                 clearKeycodeCache();

      private:
        std::unordered_map<std::string, xkb_keycode_t> m_keyToCodeCache;
    };

    UP<CResolver>& resolver();
}
