#include <overview/hyprland/scene/WorkspaceSearch.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace Overview::Hyprland;

TEST(Overview, workspaceSearchGeometryFitsTopMarginAtFractionalScale) {
    const auto GEOMETRY = WorkspaceSearch::calculateGeometry({1920, 1080}, 1.5F);

    EXPECT_GT(GEOMETRY.logicalBox.w, 0);
    EXPECT_GT(GEOMETRY.logicalBox.h, 0);
    EXPECT_GE(GEOMETRY.logicalBox.x, 0);
    EXPECT_GE(GEOMETRY.logicalBox.y, 0);
    EXPECT_LE(GEOMETRY.logicalBox.x + GEOMETRY.logicalBox.w, 1920);
    EXPECT_LE(GEOMETRY.logicalBox.y + GEOMETRY.logicalBox.h, 1080 * 0.05F);
    EXPECT_FLOAT_EQ(GEOMETRY.pixelBox.w, std::round(GEOMETRY.logicalBox.w * 1.5F));
    EXPECT_FLOAT_EQ(GEOMETRY.pixelBox.h, std::round(GEOMETRY.logicalBox.h * 1.5F));
}

TEST(Overview, workspaceSearchGeometryHandlesSmallAndInvalidOutputs) {
    const auto SMALL = WorkspaceSearch::calculateGeometry({320, 200}, 1.F);
    EXPECT_FALSE(SMALL.logicalBox.empty());
    EXPECT_LE(SMALL.logicalBox.x + SMALL.logicalBox.w, 320);
    EXPECT_LE(SMALL.logicalBox.y + SMALL.logicalBox.h, 200 * 0.05F);

    EXPECT_TRUE(WorkspaceSearch::calculateGeometry({}, 1.F).logicalBox.empty());
    EXPECT_TRUE(WorkspaceSearch::calculateGeometry({1920, 1080}, 0.F).logicalBox.empty());
}
