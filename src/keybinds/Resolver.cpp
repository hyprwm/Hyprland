#include "Resolver.hpp"

#include "../devices/IKeyboard.hpp"
#include "../managers/SeatManager.hpp"

#include <hyprutils/string/String.hpp>

#include <algorithm>
#include <cctype>
#include <format>
#include <utility>

using namespace Keybinds;
using namespace Hyprutils::String;

static constexpr std::pair<std::string_view, eKeyboardModifiers> MODIFIER_NAMES[] = {
    {"SHIFT", HL_MODIFIER_SHIFT}, {"CAPS", HL_MODIFIER_CAPS}, {"CTRL", HL_MODIFIER_CTRL}, {"CONTROL", HL_MODIFIER_CTRL}, {"ALT", HL_MODIFIER_ALT},
    {"MOD1", HL_MODIFIER_ALT},    {"MOD2", HL_MODIFIER_MOD2}, {"MOD3", HL_MODIFIER_MOD3}, {"SUPER", HL_MODIFIER_META},   {"WIN", HL_MODIFIER_META},
    {"LOGO", HL_MODIFIER_META},   {"MOD4", HL_MODIFIER_META}, {"META", HL_MODIFIER_META}, {"MOD5", HL_MODIFIER_MOD5},
};

std::optional<eKeyboardModifiers> Keybinds::modifierFromString(std::string_view modifier) {
    for (const auto& [name, mask] : MODIFIER_NAMES) {
        if (modifier == name)
            return mask;
    }

    return std::nullopt;
}

uint32_t Keybinds::modMaskFromString(std::string modifiers) {
    uint32_t modMask = 0;
    std::ranges::transform(modifiers, modifiers.begin(), ::toupper);

    for (const auto& [name, modifier] : MODIFIER_NAMES) {
        if (modifiers.contains(name))
            modMask |= modifier;
    }

    return modMask;
}

std::expected<uint32_t, std::string> CResolver::resolveKeycode(const std::string& key) {
    if (isNumber(key) && std::stoi(key) > 9)
        return (uint32_t)std::stoi(key);

    if (key.starts_with("code:") && isNumber(key.substr(5)))
        return (uint32_t)std::stoi(key.substr(5));

    if (key.starts_with("mouse:") && isNumber(key.substr(6))) {
        uint32_t code = std::stoi(key.substr(6));
        if (code < 272)
            return std::unexpected("invalid mouse button");
        return code;
    }

    const auto KEYSYM = xkb_keysym_from_name(key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);

    const auto KB = g_pSeatManager->m_keyboard;
    if (!KB)
        return std::unexpected("no keyboard");

    const auto KEYPAIRSTRING = std::format("{}{}", rc<uintptr_t>(KB.get()), key);

    if (const auto IT = m_keyToCodeCache.find(KEYPAIRSTRING); IT != m_keyToCodeCache.end())
        return IT->second;

    xkb_keymap*   km          = KB->m_xkbKeymap;
    xkb_state*    ks          = KB->m_xkbState;
    xkb_keycode_t keycode_min = xkb_keymap_min_keycode(km);
    xkb_keycode_t keycode_max = xkb_keymap_max_keycode(km);
    uint32_t      keycode     = 0;

    for (xkb_keycode_t kc = keycode_min; kc <= keycode_max; ++kc) {
        xkb_keysym_t sym = xkb_state_key_get_one_sym(ks, kc);
        if (sym == KEYSYM) {
            keycode                         = kc;
            m_keyToCodeCache[KEYPAIRSTRING] = keycode;
        }
    }

    if (!keycode)
        return std::unexpected("key not found");

    return keycode;
}

void CResolver::clearKeycodeCache() {
    m_keyToCodeCache.clear();
}

UP<CResolver>& Keybinds::resolver() {
    static auto resolver = makeUnique<CResolver>();
    return resolver;
}
