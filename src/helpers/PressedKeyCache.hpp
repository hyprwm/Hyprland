#pragma once

#include "../devices/IKeyboard.hpp"

#include <vector>

namespace NInputUtils {
    bool                  isKeyboardEligibleForPressedKeyCache(const IKeyboard& keyboard);
    std::vector<uint32_t> collectUniquePressedKeys(const std::vector<std::vector<uint32_t>>& pressedKeySets);
    std::vector<uint32_t> collectPressedKeysFromKeyboards(const std::vector<SP<IKeyboard>>& keyboards);
}
