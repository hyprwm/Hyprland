#include <desktop/view/window/WindowBackend.hpp>

#include <gtest/gtest.h>

using namespace Desktop::View;

TEST(WindowBackend, ConfigureAckSelectsLatestMatchingSerial) {
    CWindowConfigureAckTracker tracker;
    tracker.add(10, {100, 100});
    tracker.add(20, {200, 200});
    tracker.add(30, {300, 300});

    EXPECT_EQ(tracker.acknowledge(25), Vector2D(200, 200));
    EXPECT_EQ(tracker.acknowledge(30), Vector2D(300, 300));
    EXPECT_TRUE(tracker.empty());
}

TEST(WindowBackend, ConfigureAckIgnoresOlderSerial) {
    CWindowConfigureAckTracker tracker;
    tracker.add(20, {200, 200});

    EXPECT_EQ(tracker.acknowledge(19), std::nullopt);
    EXPECT_FALSE(tracker.empty());
}

TEST(WindowBackend, ConfigureAckDropsSupersededEntries) {
    CWindowConfigureAckTracker tracker;
    tracker.add(10, {100, 100});
    tracker.add(20, {200, 200});

    EXPECT_EQ(tracker.acknowledge(20), Vector2D(200, 200));
    EXPECT_TRUE(tracker.empty());
}
