#include "Key.hpp"

#include <hyprutils/string/Numeric.hpp>

using namespace Keybinds;
using namespace Hyprutils::String;

static bool isValidEvent(const std::string& e) {
    return e == "catchall" || e.starts_with("mouse:") || e.starts_with("switch:") || e == "mouse_down" || e == "mouse_left" || e == "mouse_up" || e == "mouse_right";
}

CKey::CKey(eKeyboardModifiers modifier, const std::string& sided) : m_mod(modifier) {
    if (sided.empty())
        return;

    auto sym = xkb_keysym_from_name(sided.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
    if (sym != XKB_KEY_NoSymbol)
        m_sym = sym;
}

CKey::CKey(xkb_keycode_t code) : m_code(code) {
    ;
}

CKey::CKey(const std::string& string) {
    if (isValidEvent(string)) {
        m_event = string;
        return;
    }

    if (string.starts_with("code:")) {
        auto num = strToNumber<uint32_t>(string.substr(5));
        if (!num)
            return;

        m_code = *num;
        return;
    }

    auto sym = xkb_keysym_from_name(string.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
    if (sym != XKB_KEY_NoSymbol)
        m_sym = sym;
}

bool CKey::empty() const {
    return !m_code && !m_mod && !m_sym && !m_event;
}

bool CKey::isMod() const {
    return !!m_mod;
}

std::optional<xkb_keysym_t> CKey::keysym() const {
    return m_sym;
}

std::optional<xkb_keycode_t> CKey::keycode() const {
    return m_code;
}

std::optional<eKeyboardModifiers> CKey::modifier() const {
    return m_mod;
}

std::optional<std::string_view> CKey::event() const {
    if (!m_event)
        return std::nullopt;

    return *m_event;
}

bool CKey::valid() const {
    return !empty();
}

bool CKey::matches(KeyEvent ev, xkb_state*) const {
    if (empty())
        return false;

    return ev.visit([&](auto&& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, SKeysymPattern>)
            return m_sym && value.sym == *m_sym;
        else if constexpr (std::is_same_v<T, SKeycodePattern>)
            return (m_sym && value.sym == *m_sym) || (m_code && value.code == *m_code);
        else if constexpr (std::is_same_v<T, SSidedModifierPattern>)
            return (m_sym && value.sym == *m_sym /* modifier will match obviously */) || (!m_sym /* un-sided */ && m_mod && value.modMask == *m_mod);
        else if constexpr (std::is_same_v<T, SExternalEventPattern>)
            return m_event && value.ev == *m_event;
        else
            return false;
    });
}

bool CKey::matches(const SResolvedKey& key) const {
    if (empty())
        return false;

    if (m_event || key.event)
        return m_event && key.event && *m_event == *key.event;

    if (m_mod)
        return key.modifier == m_mod && (!m_sym || key.sym == *m_sym);

    return (m_sym && key.sym == *m_sym) || (m_code && key.code == *m_code);
}
