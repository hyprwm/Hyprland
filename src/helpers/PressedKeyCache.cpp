#include "PressedKeyCache.hpp"

#include <algorithm>
#include <ranges>

namespace NInputUtils {
    bool isKeyboardEligibleForPressedKeyCache(const IKeyboard& keyboard) {
        return keyboard.m_enabled && keyboard.m_allowed && keyboard.shareStates();
    }

    std::vector<uint32_t> collectUniquePressedKeys(const std::vector<std::vector<uint32_t>>& pressedKeySets) {
        std::vector<uint32_t> rebuiltPressedKeys;

        for (const auto& pressedKeys : pressedKeySets) {
            for (const auto& key : pressedKeys) {
                if (!std::ranges::contains(rebuiltPressedKeys, key))
                    rebuiltPressedKeys.emplace_back(key);
            }
        }

        return rebuiltPressedKeys;
    }

    std::vector<uint32_t> collectPressedKeysFromKeyboards(const std::vector<SP<IKeyboard>>& keyboards) {
        std::vector<std::vector<uint32_t>> pressedKeySets;
        pressedKeySets.reserve(keyboards.size());

        for (const auto& kb : keyboards) {
            if (!kb)
                continue;

            if (!isKeyboardEligibleForPressedKeyCache(*kb))
                continue;

            pressedKeySets.emplace_back(kb->getPressedKeys());
        }

        return collectUniquePressedKeys(pressedKeySets);
    }
}
