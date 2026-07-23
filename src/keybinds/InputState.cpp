#include "InputState.hpp"

#include <algorithm>

using namespace Keybinds;

bool CInputState::press(SPressedInput&& input) {
    const auto device = input.device.lock();
    if (!device || indexOf(input.key, device))
        return false;

    m_pressed.emplace_back(std::move(input));
    rebuildHeldKeys();
    return true;
}

std::optional<SPressedInput> CInputState::release(const SResolvedKey& key, const SP<IHID>& device) {
    const auto INDEX = indexOf(key, device);
    if (!INDEX)
        return std::nullopt;

    auto result = std::move(m_pressed[*INDEX]);
    m_pressed.erase(m_pressed.begin() + *INDEX);
    rebuildHeldKeys();
    return result;
}

void CInputState::clear() {
    m_pressed.clear();
    m_heldKeys.clear();
}

void CInputState::clearDevice(const SP<IHID>& device) {
    if (!device)
        return;

    for (size_t i = m_pressed.size(); i > 0; --i) {
        const auto INDEX = i - 1;
        if (m_pressed[INDEX].device.lock() != device)
            continue;

        m_pressed.erase(m_pressed.begin() + INDEX);
    }

    rebuildHeldKeys();
}

bool CInputState::setForwarded(const SResolvedKey& key, const SP<IHID>& device, bool forwarded) {
    const auto INDEX = indexOf(key, device);
    if (!INDEX)
        return false;

    m_pressed[*INDEX].forwarded = forwarded;
    return true;
}

SPressedInput* CInputState::find(const SResolvedKey& key, const SP<IHID>& device) {
    const auto INDEX = indexOf(key, device);
    return INDEX ? &m_pressed[*INDEX] : nullptr;
}

const SPressedInput* CInputState::find(const SResolvedKey& key, const SP<IHID>& device) const {
    const auto INDEX = indexOf(key, device);
    return INDEX ? &m_pressed[*INDEX] : nullptr;
}

std::vector<SPendingRelease> CInputState::takeReleaseCallbacks(const SResolvedKey& key, const SP<IHID>& device) {
    if (!device)
        return {};

    return takeReleaseCallbacks([&](const auto& input, const auto&) { return input.device.lock() == device && sameIdentity(input.key, key); });
}

std::vector<SPendingRelease> CInputState::takeReleaseCallbacks(const PBind& bind) {
    return takeReleaseCallbacks([&](const auto&, const auto& candidate) { return candidate == bind; });
}

std::vector<SPendingRelease> CInputState::takeReleaseCallbacksForDevice(const SP<IHID>& device) {
    if (!device)
        return {};

    return takeReleaseCallbacks([&](const auto& input, const auto&) { return input.device.lock() == device; });
}

std::vector<SPendingRelease> CInputState::takeAllReleaseCallbacks() {
    return takeReleaseCallbacks([](const auto&, const auto&) { return true; });
}

bool CInputState::isKeycodeDown(xkb_keycode_t keycode) const {
    return std::ranges::any_of(m_pressed, [keycode](const auto& input) { return input.key.code == keycode; });
}

bool CInputState::isKeysymDown(xkb_keysym_t keysym) const {
    return std::ranges::any_of(m_pressed, [keysym](const auto& input) { return input.key.sym == keysym; });
}

bool CInputState::isEventDown(std::string_view event) const {
    return std::ranges::any_of(m_pressed, [event](const auto& input) { return input.key.event && *input.key.event == event; });
}

std::span<SPressedInput> CInputState::pressed() {
    return m_pressed;
}

std::span<const SPressedInput> CInputState::pressed() const {
    return m_pressed;
}

std::span<const SResolvedKey> CInputState::heldKeys() const {
    return m_heldKeys;
}

std::optional<size_t> CInputState::indexOf(const SResolvedKey& key, const SP<IHID>& device) const {
    if (!device)
        return std::nullopt;

    for (size_t i = 0; i < m_pressed.size(); ++i) {
        if (m_pressed[i].device.lock() == device && sameIdentity(m_pressed[i].key, key))
            return i;
    }

    return std::nullopt;
}

bool CInputState::sameIdentity(const SResolvedKey& lhs, const SResolvedKey& rhs) {
    if (lhs.event || rhs.event)
        return lhs.event && rhs.event && lhs.event == rhs.event;

    return lhs.code != 0 && lhs.code == rhs.code;
}

bool CInputState::equivalentHeldKey(const SResolvedKey& lhs, const SResolvedKey& rhs) {
    if (lhs.event || rhs.event)
        return lhs.event && rhs.event && lhs.event == rhs.event;

    return lhs.code == rhs.code && lhs.sym == rhs.sym && lhs.modifier == rhs.modifier;
}

std::vector<SPendingRelease> CInputState::takeReleaseCallbacks(const std::function<bool(const SPressedInput&, const PBind&)>& predicate) {
    std::vector<SPendingRelease> result;

    for (auto& input : m_pressed) {
        std::erase_if(input.releaseCallbacks, [&](const auto& weak) {
            const auto bind = weak.lock();
            if (!bind)
                return true;
            if (!predicate(input, bind))
                return false;

            result.emplace_back(input.key, input.device, bind, input.actionCode, input.actionMouseCode, input.actionTimeMs);
            return true;
        });
    }

    return result;
}

void CInputState::rebuildHeldKeys() {
    m_heldKeys.clear();
    for (const auto& input : m_pressed) {
        if (std::ranges::none_of(m_heldKeys, [&input](const auto& key) { return equivalentHeldKey(key, input.key); }))
            m_heldKeys.emplace_back(input.key);
    }
}
