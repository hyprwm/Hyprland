#include <overview/hyprland/scene/WorkspaceTapeLayout.hpp>

#include <gtest/gtest.h>

using namespace Overview::Hyprland;
using Hyprutils::Math::Vector2D;

TEST(Overview, workspaceTapeLayoutFitsTilesAndKeepsUniformGaps) {
    constexpr Vector2D          MONITOR_SIZE = {1920, 1080};
    constexpr float             TILE_SCALE   = 0.9F;
    constexpr float             GAP_SCALE    = 0.02F;
    constexpr float             EXPECTED_GAP = MONITOR_SIZE.x * GAP_SCALE;

    const std::vector<Vector2D> sourceSizes = {
        {1080, 1920},
        {1920, 1080},
        {3440, 1440},
    };

    const auto BOXES = WorkspaceTapeLayout::calculate(MONITOR_SIZE, sourceSizes, 1, TILE_SCALE, GAP_SCALE);
    ASSERT_EQ(BOXES.size(), sourceSizes.size());

    EXPECT_FLOAT_EQ(BOXES.at(1).w, MONITOR_SIZE.x * TILE_SCALE);
    EXPECT_FLOAT_EQ(BOXES.at(1).h, MONITOR_SIZE.y * TILE_SCALE);
    EXPECT_FLOAT_EQ(BOXES.at(1).x, (MONITOR_SIZE.x - BOXES.at(1).w) / 2.F);
    EXPECT_FLOAT_EQ(BOXES.at(1).y, (MONITOR_SIZE.y - BOXES.at(1).h) / 2.F);

    EXPECT_FLOAT_EQ(BOXES.at(0).h, MONITOR_SIZE.y * TILE_SCALE);
    EXPECT_LT(BOXES.at(0).w, MONITOR_SIZE.x * TILE_SCALE);
    EXPECT_FLOAT_EQ(BOXES.at(2).w, MONITOR_SIZE.x * TILE_SCALE);
    EXPECT_LT(BOXES.at(2).h, MONITOR_SIZE.y * TILE_SCALE);

    EXPECT_FLOAT_EQ(BOXES.at(1).x - BOXES.at(0).x - BOXES.at(0).w, EXPECTED_GAP);
    EXPECT_FLOAT_EQ(BOXES.at(2).x - BOXES.at(1).x - BOXES.at(1).w, EXPECTED_GAP);
    EXPECT_FLOAT_EQ(BOXES.at(0).y, (MONITOR_SIZE.y - BOXES.at(0).h) / 2.F);
    EXPECT_FLOAT_EQ(BOXES.at(2).y, (MONITOR_SIZE.y - BOXES.at(2).h) / 2.F);
}

TEST(Overview, workspaceTapeLayoutHandlesEmptyAndInvalidSources) {
    EXPECT_TRUE(WorkspaceTapeLayout::calculate({1920, 1080}, {}, 0, 0.9F, 0.02F).empty());

    const std::vector<Vector2D> sourceSizes = {{0, 0}};
    const auto                  BOXES       = WorkspaceTapeLayout::calculate({1920, 1080}, sourceSizes, 10, 0.9F, 0.02F);

    ASSERT_EQ(BOXES.size(), 1U);
    EXPECT_NEAR(BOXES.at(0).w, 1728, 0.001);
    EXPECT_NEAR(BOXES.at(0).h, 972, 0.001);
    EXPECT_NEAR(BOXES.at(0).x, 96, 0.001);
    EXPECT_NEAR(BOXES.at(0).y, 54, 0.001);
}
