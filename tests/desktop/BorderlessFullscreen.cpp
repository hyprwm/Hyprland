
#include <desktop/view/Window.hpp>

#include <gtest/gtest.h>

using namespace Desktop::View;

// Test that the tolerance constant is defined correctly
TEST(BorderlessFullscreen, ToleranceConstant) {
    EXPECT_EQ(BORDERLESS_FULLSCREEN_TOLERANCE, 5.0);
}

// Test exact coverage - window at 0,0 with exact monitor size
TEST(BorderlessFullscreen, ExactCoverage) {
    Vector2D monitorSize{1920, 1080};
    Vector2D windowPos{0, 0};
    Vector2D windowSize{1920, 1080};

    EXPECT_TRUE(coversMonitorForBorderlessFullscreen(windowPos, windowSize, monitorSize));
}

// Test window slightly offset but still covers monitor (within tolerance)
TEST(BorderlessFullscreen, SlightlyOffset_WithinTolerance) {
    Vector2D monitorSize{1920, 1080};

    // Window at 4,4 with exact size - still covers within 5px tolerance
    EXPECT_TRUE(coversMonitorForBorderlessFullscreen({4, 4}, {1920, 1080}, monitorSize));

    // Window at -1,-1 extending past monitor - covers
    EXPECT_TRUE(coversMonitorForBorderlessFullscreen({-1, -1}, {1922, 1082}, monitorSize));

    // Window at 0,0 but 4px smaller - end is at 1916, within tolerance of 1920-5=1915
    EXPECT_TRUE(coversMonitorForBorderlessFullscreen({0, 0}, {1916, 1076}, monitorSize));
}

// Test window offset outside tolerance - does NOT cover
TEST(BorderlessFullscreen, Offset_OutsideTolerance) {
    Vector2D monitorSize{1920, 1080};

    // Window at 10,10 - leaves visible gap at top-left (outside 5px tolerance)
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({10, 10}, {1920, 1080}, monitorSize));

    // Window at 6,6 - outside 5px tolerance
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({6, 6}, {1920, 1080}, monitorSize));

    // Window at 400,400 - clearly not fullscreen
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({400, 400}, {1920, 1080}, monitorSize));
}

// Test window doesn't extend to monitor edge
TEST(BorderlessFullscreen, DoesNotReachEdge) {
    Vector2D monitorSize{1920, 1080};

    // Window at 0,0 but too small - doesn't reach edge (more than 5px short)
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({0, 0}, {1910, 1070}, monitorSize));

    // Window at 0,0 but 6px short on each dimension (outside 5px tolerance)
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({0, 0}, {1914, 1074}, monitorSize));
}

// Test window larger than monitor (extends past edges)
TEST(BorderlessFullscreen, LargerThanMonitor) {
    Vector2D monitorSize{1920, 1080};

    // Window starts before monitor and extends past - covers entirely
    EXPECT_TRUE(coversMonitorForBorderlessFullscreen({-10, -10}, {1940, 1100}, monitorSize));

    // Window at 0,0 but larger - extends past edges
    EXPECT_TRUE(coversMonitorForBorderlessFullscreen({0, 0}, {2000, 1200}, monitorSize));
}

// Test asymmetric cases
TEST(BorderlessFullscreen, AsymmetricCases) {
    Vector2D monitorSize{1920, 1080};

    // X covers, Y doesn't (too short)
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({0, 0}, {1920, 1070}, monitorSize));

    // Y covers, X doesn't (too short)
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({0, 0}, {1910, 1080}, monitorSize));

    // X offset too much, Y fine
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({10, 0}, {1920, 1080}, monitorSize));

    // Y offset too much, X fine
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({0, 10}, {1920, 1080}, monitorSize));
}

// Test with different monitor sizes
TEST(BorderlessFullscreen, DifferentMonitorSizes) {
    // 4K monitor
    Vector2D monitor4K{3840, 2160};
    EXPECT_TRUE(coversMonitorForBorderlessFullscreen({0, 0}, {3840, 2160}, monitor4K));
    EXPECT_TRUE(coversMonitorForBorderlessFullscreen({1, 1}, {3840, 2160}, monitor4K));
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({10, 10}, {3840, 2160}, monitor4K));

    // 1440p monitor
    Vector2D monitor1440p{2560, 1440};
    EXPECT_TRUE(coversMonitorForBorderlessFullscreen({0, 0}, {2560, 1440}, monitor1440p));
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({5, 5}, {2560, 1440}, monitor1440p));

    // Ultrawide
    Vector2D monitorUW{3440, 1440};
    EXPECT_TRUE(coversMonitorForBorderlessFullscreen({0, 0}, {3440, 1440}, monitorUW));
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({0, 0}, {3430, 1430}, monitorUW));
}

// Test boundary conditions at exactly 5px tolerance
TEST(BorderlessFullscreen, ExactToleranceBoundary) {
    Vector2D monitorSize{1920, 1080};

    // Position at exactly 5px - should NOT cover (>= tolerance)
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({5, 0}, {1920, 1080}, monitorSize));
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({0, 5}, {1920, 1080}, monitorSize));

    // Position at 4.9px - should cover (within tolerance)
    EXPECT_TRUE(coversMonitorForBorderlessFullscreen({4.9, 4.9}, {1920, 1080}, monitorSize));

    // End position exactly 5px short of edge - should cover
    // End = 0 + 1915 = 1915, need >= 1920 - 5 = 1915, so 1915 >= 1915 is true
    EXPECT_TRUE(coversMonitorForBorderlessFullscreen({0, 0}, {1915, 1075}, monitorSize));

    // End position 6px short - should NOT cover
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({0, 0}, {1914, 1074}, monitorSize));
}

// Test negative positions (window extends past monitor origin)
TEST(BorderlessFullscreen, NegativePositions) {
    Vector2D monitorSize{1920, 1080};

    // Window starts at -5,-5 and is large enough to cover
    EXPECT_TRUE(coversMonitorForBorderlessFullscreen({-5, -5}, {1930, 1090}, monitorSize));

    // Window starts at -100,-100 and extends well past
    EXPECT_TRUE(coversMonitorForBorderlessFullscreen({-100, -100}, {2100, 1280}, monitorSize));

    // Window starts negative but doesn't reach far edge
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({-100, -100}, {1900, 1060}, monitorSize));
}

// Test typical windowed game scenarios that should NOT be detected as fullscreen
TEST(BorderlessFullscreen, WindowedGameScenarios) {
    Vector2D monitorSize{1920, 1080};

    // Centered 1280x720 window
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({320, 180}, {1280, 720}, monitorSize));

    // Centered 1600x900 window
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({160, 90}, {1600, 900}, monitorSize));

    // Small window in corner
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({0, 0}, {800, 600}, monitorSize));

    // Window at monitor size but offset (user dragged it)
    EXPECT_FALSE(coversMonitorForBorderlessFullscreen({50, 50}, {1920, 1080}, monitorSize));
}
