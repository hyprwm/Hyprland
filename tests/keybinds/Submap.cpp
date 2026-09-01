#include <keybinds/Submap.hpp>

#include <gtest/gtest.h>

#include "Utils.hpp"

using namespace Keybinds;

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
