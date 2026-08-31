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
    auto submap = makeSubmap("example", {"device_a", "device_b"}, false);
    ASSERT_TRUE(submap.matchesDevice(nullptr));
}

TEST(KeybindsSubmap, NoMatchesEmptyDeviceWhenInclusive) {
    auto submap = makeSubmap("example", {"device_a", "device_b"}, true);
    ASSERT_FALSE(submap.matchesDevice(nullptr));
}
