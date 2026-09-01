#include <overview/hyprland/scene/WorkspaceMiniStripLayout.hpp>

#include <gtest/gtest.h>

using namespace Overview::Hyprland;
using Hyprutils::Math::CBox;
using Hyprutils::Math::Vector2D;

TEST(Overview, workspaceMiniStripCentersTilesAndPreservesAspectRatios) {
    const CBox                  BOUNDS      = {{40, 50}, {1000, 120}};
    const std::vector<Vector2D> sourceSizes = {{1920, 1080}, {1080, 1920}, {3440, 1440}};

    const auto                  BOXES = WorkspaceMiniStripLayout::calculate(BOUNDS, sourceSizes, 1, 0.15F);
    ASSERT_EQ(BOXES.size(), sourceSizes.size());
    EXPECT_FLOAT_EQ(BOXES.at(0).h, BOUNDS.h);
    EXPECT_FLOAT_EQ(BOXES.at(1).h, BOUNDS.h);
    EXPECT_FLOAT_EQ(BOXES.at(2).h, BOUNDS.h);
    EXPECT_NEAR(BOXES.at(0).w / BOXES.at(0).h, 1920.F / 1080.F, 0.001F);
    EXPECT_NEAR(BOXES.at(1).w / BOXES.at(1).h, 1080.F / 1920.F, 0.001F);
    EXPECT_NEAR(BOXES.at(2).w / BOXES.at(2).h, 3440.F / 1440.F, 0.001F);
    EXPECT_NEAR((BOXES.front().x + BOXES.back().x + BOXES.back().w) / 2.F, BOUNDS.x + BOUNDS.w / 2.F, 0.001F);
}

TEST(Overview, workspaceMiniStripKeepsSelectedTileVisibleOnOverflow) {
    const CBox                  BOUNDS = {{40, 50}, {500, 100}};
    const std::vector<Vector2D> sourceSizes(8, {1920, 1080});

    const auto                  FIRST = WorkspaceMiniStripLayout::calculate(BOUNDS, sourceSizes, 0, 0.15F);
    const auto                  LAST  = WorkspaceMiniStripLayout::calculate(BOUNDS, sourceSizes, sourceSizes.size() - 1, 0.15F);

    EXPECT_FLOAT_EQ(FIRST.front().x, BOUNDS.x);
    EXPECT_FLOAT_EQ(LAST.back().x + LAST.back().w, BOUNDS.x + BOUNDS.w);
    EXPECT_FALSE(FIRST.front().intersection(BOUNDS).empty());
    EXPECT_FALSE(LAST.back().intersection(BOUNDS).empty());
}
