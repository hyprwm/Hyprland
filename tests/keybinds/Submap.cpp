#include "devices/Keyboard.hpp"
#include <keybinds/Submap.hpp>

#include <gtest/gtest.h>

using namespace Keybinds;

static CSubmap makeSubmap(std::string name, std::unordered_set<std::string> devices, const bool inclusive) {
    return CSubmap(name, {.devices = devices, .inclusive = inclusive});
};

// testing with actual devices requires being able to instantiate them
// virtually :(

TEST(KeybindsSubmap, MatchesEmptyDeviceWhenNotInclusive) {
    const auto SUBMAP = makeSubmap("example", {"device_a", "device_b"}, false);
    ASSERT_TRUE(SUBMAP.matchesDevice(nullptr));
}

TEST(KeybindsSubmap, NoMatchesEmptyDeviceWhenInclusive) {
    const auto SUBMAP = makeSubmap("example", {"device_a", "device_b"}, true);
    ASSERT_FALSE(SUBMAP.matchesDevice(nullptr));
}
