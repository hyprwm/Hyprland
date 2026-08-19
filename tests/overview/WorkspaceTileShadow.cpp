#include <overview/hyprland/scene/WorkspaceTileShadow.hpp>

#include <gtest/gtest.h>

using namespace Overview::Hyprland;
using Hyprutils::Math::CBox;

TEST(Overview, workspaceTileShadowScalesAndExpandsGeometry) {
    const CBox TILE = {100, 200, 800, 450};

    const auto SHADOW = WorkspaceTileShadow::calculate(TILE, 2.F, 0.8F, 0.5F, 14.F, 20.F);
    ASSERT_TRUE(SHADOW);

    EXPECT_EQ(SHADOW->range, 14);
    EXPECT_EQ(SHADOW->rounding, 20);
    EXPECT_FLOAT_EQ(SHADOW->opacity, 0.5F);
    EXPECT_EQ(SHADOW->outerBox, TILE.copy().expand(14));
}

TEST(Overview, workspaceTileShadowFollowsVisibility) {
    const CBox TILE = {0, 0, 1920, 1080};

    EXPECT_FALSE(WorkspaceTileShadow::calculate(TILE, 1.F, 1.F, 0.F, 14.F, 20.F));
    EXPECT_FALSE(WorkspaceTileShadow::calculate(TILE, 1.F, 0.F, 1.F, 14.F, 20.F));
    EXPECT_FALSE(WorkspaceTileShadow::calculate(TILE, 0.F, 1.F, 1.F, 14.F, 20.F));

    const auto SHADOW = WorkspaceTileShadow::calculate(TILE, 1.F, 0.25F, 1.F, 14.F, 20.F);
    ASSERT_TRUE(SHADOW);
    EXPECT_FLOAT_EQ(SHADOW->opacity, 0.25F);
}
