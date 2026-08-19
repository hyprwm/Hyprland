#include <overview/hyprland/scene/WorkspaceTapeLayout.hpp>

#include <gtest/gtest.h>

using namespace Overview::Hyprland;
using Hyprutils::Math::CBox;
using Hyprutils::Math::Vector2D;

TEST(Overview, workspaceTapeLayoutFitsTilesAndKeepsUniformGaps) {
    const CBox                  BOUNDS       = {{96, 200}, {1728, 800}};
    constexpr float             GAP_SCALE    = 0.02F;
    const float                 EXPECTED_GAP = BOUNDS.w * GAP_SCALE;

    const std::vector<Vector2D> sourceSizes = {
        {1080, 1920},
        {1920, 1080},
        {3440, 1440},
    };

    const auto BOXES = WorkspaceTapeLayout::calculate(BOUNDS, sourceSizes, 1, GAP_SCALE);
    ASSERT_EQ(BOXES.size(), sourceSizes.size());

    EXPECT_FLOAT_EQ(BOXES.at(1).w, BOUNDS.h * 1920.F / 1080.F);
    EXPECT_FLOAT_EQ(BOXES.at(1).h, BOUNDS.h);
    EXPECT_FLOAT_EQ(BOXES.at(1).x, BOUNDS.x + (BOUNDS.w - BOXES.at(1).w) / 2.F);
    EXPECT_FLOAT_EQ(BOXES.at(1).y, BOUNDS.y + (BOUNDS.h - BOXES.at(1).h) / 2.F);

    EXPECT_FLOAT_EQ(BOXES.at(0).h, BOUNDS.h);
    EXPECT_LT(BOXES.at(0).w, BOUNDS.w);
    EXPECT_FLOAT_EQ(BOXES.at(2).w, BOUNDS.w);
    EXPECT_LT(BOXES.at(2).h, BOUNDS.h);

    EXPECT_FLOAT_EQ(BOXES.at(1).x - BOXES.at(0).x - BOXES.at(0).w, EXPECTED_GAP);
    EXPECT_FLOAT_EQ(BOXES.at(2).x - BOXES.at(1).x - BOXES.at(1).w, EXPECTED_GAP);
    EXPECT_FLOAT_EQ(BOXES.at(0).y, BOUNDS.y + (BOUNDS.h - BOXES.at(0).h) / 2.F);
    EXPECT_FLOAT_EQ(BOXES.at(2).y, BOUNDS.y + (BOUNDS.h - BOXES.at(2).h) / 2.F);
}

TEST(Overview, workspaceTapeLayoutHandlesEmptyAndInvalidSources) {
    EXPECT_TRUE(WorkspaceTapeLayout::calculate({{0, 0}, {1920, 1080}}, {}, 0, 0.02F).empty());

    const std::vector<Vector2D> sourceSizes = {{0, 0}};
    const auto                  BOXES       = WorkspaceTapeLayout::calculate({{96, 54}, {1728, 972}}, sourceSizes, 10, 0.02F);

    ASSERT_EQ(BOXES.size(), 1U);
    EXPECT_NEAR(BOXES.at(0).w, 1728, 0.001);
    EXPECT_NEAR(BOXES.at(0).h, 972, 0.001);
    EXPECT_NEAR(BOXES.at(0).x, 96, 0.001);
    EXPECT_NEAR(BOXES.at(0).y, 54, 0.001);
}
