#include "Bind.hpp"
#include "Resolver.hpp"

using namespace Keybinds;

#include <algorithm>
#include <array>
#include <cctype>
#include <format>

struct SSidedMod {
    const char*        name;
    const char*        xkb;
    eKeyboardModifiers mask;
};

static constexpr auto SIDED_MODIFIER_NAMES = std::to_array<SSidedMod>({
    {"SHIFT_L", "SHIFT_L", HL_MODIFIER_SHIFT},
    {"SHIFT_R", "SHIFT_R", HL_MODIFIER_SHIFT},
    {"CONTROL_L", "CONTROL_L", HL_MODIFIER_CTRL},
    {"CTRL_L", "CONTROL_L", HL_MODIFIER_CTRL},
    {"CONTROL_R", "CONTROL_R", HL_MODIFIER_CTRL},
    {"CTRL_R", "CONTROL_R", HL_MODIFIER_CTRL},
    {"ALT_L", "ALT_L", HL_MODIFIER_ALT},
    {"ALT_R", "ALT_R", HL_MODIFIER_ALT},
    {"SUPER_L", "SUPER_L", HL_MODIFIER_META},
    {"SUPER_R", "SUPER_R", HL_MODIFIER_META},
});

static bool           matchCaseInsensitive(std::string_view a, std::string_view b) {
    if (a.size() != b.size() || a.empty() || b.empty())
        return false;

    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(a[i]) != std::tolower(b[i]))
            return false;
    }

    return true;
}

static eBindMatch matchKeySets(const std::vector<const CKey*>& bound, const std::vector<const SResolvedKey*>& held) {
    std::vector<int> heldMatches(held.size(), -1);

    const auto       tryMatch = [&](auto&& self, size_t boundIdx, std::vector<uint8_t>& seen) -> bool {
        for (size_t heldIdx = 0; heldIdx < held.size(); ++heldIdx) {
            if (seen[heldIdx] || !bound[boundIdx]->matches(*held[heldIdx]))
                continue;

            seen[heldIdx] = true;

            if (heldMatches[heldIdx] == -1 || self(self, sc<size_t>(heldMatches[heldIdx]), seen)) {
                heldMatches[heldIdx] = sc<int>(boundIdx);
                return true;
            }
        }

        return false;
    };

    size_t matches = 0;
    for (size_t boundIdx = 0; boundIdx < bound.size(); ++boundIdx) {
        std::vector<uint8_t> seen(held.size(), false);
        if (tryMatch(tryMatch, boundIdx, seen))
            ++matches;
    }

    if (matches == bound.size() && matches == held.size())
        return BIND_MATCH_FULL;

    if (matches > 0)
        return BIND_MATCH_PARTIAL;

    return BIND_MATCH_NONE;
}

// NOLINTNEXTLINE SHUT THE FUCK UP CLANG TIDY
std::expected<CBind, std::string> CBind::make(std::vector<std::string>&& keys, BindFlags flags, BindCallback&& callback, SExtraBindArgs&& args) {
    CBind ret;
    ret.m_devices  = std::move(args.devices);
    ret.m_metadata = std::move(args.metadata);
    ret.m_flags    = flags;
    ret.m_callback = std::move(callback);

    if (!ret.m_callback)
        return std::unexpected("Bad callback");

    bool   finishedMods = false;
    size_t externalKeys = 0;
    size_t ordinaryKeys = 0;

    for (const auto& k : keys) {
        std::string normalized = k;
        std::ranges::transform(normalized, normalized.begin(), [](unsigned char c) { return std::toupper(c); });

        if (const auto MODIFIER = modifierFromString(normalized)) {
            if (finishedMods)
                return std::unexpected("Modifiers cannot appear after keys");

            ret.m_modmask |= *MODIFIER;
            continue;
        }

        if (auto x = std::ranges::find_if(SIDED_MODIFIER_NAMES, [&k](const auto& M) { return matchCaseInsensitive(k, M.name); }); x != SIDED_MODIFIER_NAMES.end()) {
            if (finishedMods)
                return std::unexpected("Modifiers cannot appear after keys");

            ret.m_modmask |= x->mask;
            ret.m_keys.emplace_back(x->mask, x->xkb);
            ret.m_keyNames.emplace_back(k);
            continue;
        }

        finishedMods = true;

        CKey key{k};
        if (!key.valid())
            return std::unexpected(std::format("Unknown key: {}", k));

        if (key.event())
            ++externalKeys;
        else
            ++ordinaryKeys;

        ret.m_keys.emplace_back(std::move(key));
        ret.m_keyNames.emplace_back(k);
    }

    if (ret.m_keys.empty())
        return std::unexpected("A bind requires a trigger");

    if (externalKeys > 1 || (externalKeys && ordinaryKeys))
        return std::unexpected("External events cannot be combined with other keys");

    return ret;
}

bool CBind::matchesDevice(SP<IHID> dev) const {
    if (!dev)
        return !(m_flags & BIND_FLAG_DEVICE_INCLUSIVE);

    const bool LISTED = m_devices.contains(dev->m_hlName) || std::ranges::any_of(dev->m_deviceTags, [this](const auto& tag) { return m_devices.contains(tag); });
    return (m_flags & BIND_FLAG_DEVICE_INCLUSIVE) ? LISTED : !LISTED;
}

eBindMatch CBind::matches(const SBindEventContext& ctx) const {
    if (!matchesContext(ctx))
        return BIND_MATCH_NONE;

    std::vector<const CKey*>         boundSidedMods;
    std::vector<const CKey*>         boundKeys;
    std::vector<const SResolvedKey*> heldSidedMods;
    std::vector<const SResolvedKey*> heldKeys;

    for (const auto& key : m_keys) {
        if (key.isMod())
            boundSidedMods.emplace_back(&key);
        else
            boundKeys.emplace_back(&key);
    }

    for (const auto& key : ctx.heldKeys) {
        if (key.event)
            continue;

        if (!key.modifier) {
            heldKeys.emplace_back(&key);
            continue;
        }

        const bool SIDE_IS_CONSTRAINED = std::ranges::any_of(boundSidedMods, [&key](const auto* bound) { return bound->modifier() == key.modifier; });
        if (SIDE_IS_CONSTRAINED)
            heldSidedMods.emplace_back(&key);
    }

    if (!boundSidedMods.empty() && matchKeySets(boundSidedMods, heldSidedMods) != BIND_MATCH_FULL)
        return BIND_MATCH_NONE;

    const auto KEY_MATCH       = matchKeySets(boundKeys, heldKeys);
    const bool TRIGGER_MATCHES = ctx.trigger && !m_keys.empty() && m_keys.back().matches(*ctx.trigger);
    const bool TRIGGER_ONLY    = boundKeys.size() == 1 || (m_flags & BIND_FLAG_RELEASE);

    if (KEY_MATCH == BIND_MATCH_NONE && ctx.trigger && std::ranges::any_of(boundSidedMods, [&ctx](const auto* key) { return key->matches(*ctx.trigger); }))
        return BIND_MATCH_PARTIAL;

    if (KEY_MATCH != BIND_MATCH_FULL && !(TRIGGER_ONLY && TRIGGER_MATCHES))
        return KEY_MATCH;

    if (!TRIGGER_MATCHES)
        return BIND_MATCH_NONE;

    if (m_flags & BIND_FLAG_RELEASE)
        return ctx.pressed ? BIND_MATCH_PARTIAL : BIND_MATCH_FULL;

    return ctx.pressed ? BIND_MATCH_FULL : BIND_MATCH_NONE;
}

bool CBind::matchesContext(const SBindEventContext& ctx) const {
    if (!m_enabled || !matchesDevice(ctx.device))
        return false;

    if (!(m_flags & BIND_FLAG_SUBMAP_UNIVERSAL) && m_metadata.submap != ctx.submap)
        return false;

    auto effectiveMods = ctx.modifiersNow;
    for (const auto& key : ctx.heldKeys) {
        if (key.modifier)
            effectiveMods |= *key.modifier;
    }

    return (m_flags & BIND_FLAG_IGNORE_MODS) || effectiveMods == m_modmask;
}

SBindResult CBind::invoke() const {
    return m_callback();
}

bool CBind::enabled() const {
    return m_enabled;
}

void CBind::setEnabled(bool x) {
    m_enabled = x;
}

bool CBind::hasFlag(eBindFlags flag) const {
    return m_flags & flag;
}

BindFlags CBind::flags() const {
    return m_flags;
}

ModifierMask CBind::modifierMask() const {
    return m_modmask;
}

size_t CBind::chordSize() const {
    return std::ranges::count_if(m_keys, [](const auto& key) { return !key.isMod(); });
}

bool CBind::containsKey(const SResolvedKey& key) const {
    return std::ranges::any_of(m_keys, [&key](const auto& pattern) { return pattern.matches(key); });
}

bool CBind::isFullyHeld(const SBindEventContext& ctx) const {
    if (!matchesContext(ctx) || !ctx.trigger || !m_keys.back().matches(*ctx.trigger))
        return false;

    std::vector<const CKey*>         boundSidedMods;
    std::vector<const CKey*>         boundKeys;
    std::vector<const SResolvedKey*> heldSidedMods;
    std::vector<const SResolvedKey*> heldKeys;

    for (const auto& key : m_keys) {
        if (key.isMod())
            boundSidedMods.emplace_back(&key);
        else
            boundKeys.emplace_back(&key);
    }

    for (const auto& key : ctx.heldKeys) {
        if (key.event)
            continue;
        if (!key.modifier) {
            heldKeys.emplace_back(&key);
            continue;
        }

        const bool SIDE_IS_CONSTRAINED = std::ranges::any_of(boundSidedMods, [&key](const auto* bound) { return bound->modifier() == key.modifier; });
        if (SIDE_IS_CONSTRAINED)
            heldSidedMods.emplace_back(&key);
    }

    return matchKeySets(boundSidedMods, heldSidedMods) == BIND_MATCH_FULL && matchKeySets(boundKeys, heldKeys) == BIND_MATCH_FULL;
}

bool CBind::isSubChordOf(const CBind& other, const SBindEventContext& ctx) const {
    if (chordSize() >= other.chordSize())
        return false;

    std::vector<bool> usedHeld(ctx.heldKeys.size(), false);
    std::vector<bool> usedOther(other.m_keys.size(), false);

    for (const auto& key : m_keys) {
        if (key.isMod())
            continue;

        bool matched = false;
        for (size_t heldIdx = 0; heldIdx < ctx.heldKeys.size() && !matched; ++heldIdx) {
            if (usedHeld[heldIdx] || ctx.heldKeys[heldIdx].modifier || !key.matches(ctx.heldKeys[heldIdx]))
                continue;

            for (size_t otherIdx = 0; otherIdx < other.m_keys.size(); ++otherIdx) {
                if (usedOther[otherIdx] || other.m_keys[otherIdx].isMod() || !other.m_keys[otherIdx].matches(ctx.heldKeys[heldIdx]))
                    continue;

                usedHeld[heldIdx]   = true;
                usedOther[otherIdx] = true;
                matched             = true;
                break;
            }
        }

        if (!matched)
            return false;
    }

    return true;
}

bool CBind::isOrderedPrefixOf(const CBind& other, const SBindEventContext& ctx) const {
    const auto CHORD_SIZE = chordSize();
    if (CHORD_SIZE == 0 || CHORD_SIZE >= other.chordSize() || !ctx.trigger || !m_keys.back().matches(*ctx.trigger))
        return false;

    size_t keyIdx      = 0;
    size_t otherKeyIdx = 0;
    size_t relevant    = 0;
    for (const auto& held : ctx.heldKeys) {
        if (held.modifier || std::ranges::none_of(other.m_keys, [&held](const auto& pattern) { return !pattern.isMod() && pattern.matches(held); }))
            continue;

        if (relevant == CHORD_SIZE)
            return false;
        while (m_keys[keyIdx].isMod())
            ++keyIdx;
        while (other.m_keys[otherKeyIdx].isMod())
            ++otherKeyIdx;
        if (!m_keys[keyIdx].matches(held) || !other.m_keys[otherKeyIdx].matches(held))
            return false;

        ++keyIdx;
        ++otherKeyIdx;
        ++relevant;
    }

    return relevant == CHORD_SIZE;
}

std::span<const CKey> CBind::keys() const {
    return m_keys;
}

std::span<const std::string> CBind::keyNames() const {
    return m_keyNames;
}

const std::unordered_set<std::string>& CBind::devices() const {
    return m_devices;
}

const SBindMetadata& CBind::metadata() const {
    return m_metadata;
}
