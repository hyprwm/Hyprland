#include "Registry.hpp"

#include <algorithm>
#include <cctype>

using namespace Keybinds;

PBind CRegistry::add(CBind&& bind) {
    auto result = makeShared<CBind>(std::move(bind));
    m_binds.emplace_back(result);
    return result;
}

bool CRegistry::remove(const PBind& bind) {
    return std::erase(m_binds, bind) > 0;
}

size_t CRegistry::removeByDisplayKey(std::string_view displayKey) {
    const auto NORMALIZED = normalizeDisplayKey(displayKey);
    return std::erase_if(m_binds, [&NORMALIZED](const auto& bind) { return normalizeDisplayKey(bind->metadata().displayKey) == NORMALIZED; });
}

std::vector<PBind> CRegistry::findByDisplayKey(std::string_view displayKey) const {
    const auto         NORMALIZED = normalizeDisplayKey(displayKey);
    std::vector<PBind> result;
    for (const auto& bind : m_binds) {
        if (normalizeDisplayKey(bind->metadata().displayKey) == NORMALIZED)
            result.emplace_back(bind);
    }
    return result;
}

void CRegistry::clear() {
    m_binds.clear();
}

std::span<const PBind> CRegistry::binds() const {
    return m_binds;
}

bool CRegistry::contains(const PBind& bind) const {
    return std::ranges::contains(m_binds, bind);
}

bool CRegistry::empty() const {
    return m_binds.empty();
}

size_t CRegistry::size() const {
    return m_binds.size();
}

bool CRegistry::hasSubmap(std::string_view submap) const {
    return std::ranges::any_of(m_binds, [submap](const auto& bind) { return bind->metadata().submap == submap; });
}

PBind CRegistry::findShortcutConflict(xkb_keysym_t keysym, ModifierMask modifiers, xkb_state* xkbState) const {
    for (const auto& bind : m_binds) {
        if (!bind->enabled() || bind->hasFlag(BIND_FLAG_MOUSE) || !bind->metadata().submap.empty() || bind->modifierMask() != modifiers)
            continue;

        const CKey* trigger = nullptr;
        for (const auto& key : bind->keys()) {
            if (key.isMod())
                continue;
            if (trigger) {
                trigger = nullptr;
                break;
            }

            trigger = &key;
        }

        if (!trigger)
            continue;

        auto bindSym = trigger->keysym().value_or(XKB_KEY_NoSymbol);
        if (bindSym == XKB_KEY_NoSymbol && trigger->keycode() && xkbState)
            bindSym = xkb_state_key_get_one_sym(xkbState, *trigger->keycode());

        if (bindSym == keysym)
            return bind;
    }

    return nullptr;
}

std::string CRegistry::normalizeDisplayKey(std::string_view displayKey) {
    std::string normalized;
    normalized.reserve(displayKey.size());

    for (const unsigned char c : displayKey) {
        if (std::isspace(c))
            continue;

        normalized.push_back(sc<char>(std::tolower(c)));
    }

    return normalized;
}
