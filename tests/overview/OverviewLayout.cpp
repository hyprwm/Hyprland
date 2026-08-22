#include <overview/hyprland/scene/OverviewLayout.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace Overview::Hyprland;

TEST(Overview, overviewLayoutSeparatesSearchMiniStripAndMainArea) {
    const auto LAYOUT = OverviewLayout::calculate({1920, 1080}, 1.5F);

    EXPECT_FALSE(LAYOUT.logicalSearch.empty());
    EXPECT_FALSE(LAYOUT.logicalMiniStrip.empty());
    EXPECT_FALSE(LAYOUT.logicalMain.empty());
    EXPECT_LE(LAYOUT.logicalSearch.y + LAYOUT.logicalSearch.h, LAYOUT.logicalMiniStrip.y);
    EXPECT_LE(LAYOUT.logicalMiniStrip.y + LAYOUT.logicalMiniStrip.h, LAYOUT.logicalMain.y);
    EXPECT_LT(LAYOUT.logicalMain.h, 1080 * 0.9F);
    EXPECT_FLOAT_EQ(LAYOUT.pixelMiniStrip.w, std::round(LAYOUT.logicalMiniStrip.w * 1.5F));
}

TEST(Overview, overviewLayoutHandlesInvalidOutputs) {
    EXPECT_TRUE(OverviewLayout::calculate({}, 1.F).logicalMain.empty());
    EXPECT_TRUE(OverviewLayout::calculate({1920, 1080}, 0.F).logicalMain.empty());
}
