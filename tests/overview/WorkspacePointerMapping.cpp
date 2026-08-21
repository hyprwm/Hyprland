#include <overview/hyprland/scene/WorkspacePointerMapping.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace Overview::Hyprland;
using Hyprutils::Math::CBox;
using Hyprutils::Math::Vector2D;

TEST(Overview, workspacePointerMappingMapsTileCenter) {
    const CBox TILE   = {{100, 200}, {800, 400}};
    const CBox SOURCE = {{-1920, 120}, {1920, 1080}};

    const auto MAPPED = WorkspacePointerMapping::mapClamped({500, 400}, TILE, SOURCE);

    ASSERT_TRUE(MAPPED.has_value());
    EXPECT_DOUBLE_EQ(MAPPED->x, -960);
    EXPECT_DOUBLE_EQ(MAPPED->y, 660);
}

TEST(Overview, workspacePointerMappingClampsOutsideTile) {
    const CBox TILE   = {{100, 200}, {800, 400}};
    const CBox SOURCE = {{1920, -1080}, {2560, 1440}};

    const auto TOP_LEFT     = WorkspacePointerMapping::mapClamped({-500, -500}, TILE, SOURCE);
    const auto BOTTOM_RIGHT = WorkspacePointerMapping::mapClamped({5000, 5000}, TILE, SOURCE);

    ASSERT_TRUE(TOP_LEFT.has_value());
    EXPECT_EQ(*TOP_LEFT, SOURCE.pos());

    ASSERT_TRUE(BOTTOM_RIGHT.has_value());
    EXPECT_LT(BOTTOM_RIGHT->x, SOURCE.x + SOURCE.w);
    EXPECT_LT(BOTTOM_RIGHT->y, SOURCE.y + SOURCE.h);
    EXPECT_DOUBLE_EQ(std::nextafter(BOTTOM_RIGHT->x, INFINITY), SOURCE.x + SOURCE.w);
    EXPECT_DOUBLE_EQ(std::nextafter(BOTTOM_RIGHT->y, INFINITY), SOURCE.y + SOURCE.h);
}

TEST(Overview, workspacePointerMappingHandlesFractionalGeometry) {
    const CBox TILE   = {{12.5, 20.25}, {333.5, 187.75}};
    const CBox SOURCE = {{-1280, 64}, {1280, 1024}};

    const auto MAPPED = WorkspacePointerMapping::mapClamped({95.875, 67.1875}, TILE, SOURCE);

    ASSERT_TRUE(MAPPED.has_value());
    EXPECT_NEAR(MAPPED->x, -960, 0.0001);
    EXPECT_NEAR(MAPPED->y, 320, 0.0001);
}

TEST(Overview, workspacePointerMappingRejectsInvalidGeometry) {
    EXPECT_FALSE(WorkspacePointerMapping::mapClamped({10, 20}, {{}, {}}, {{0, 0}, {1920, 1080}}).has_value());
    EXPECT_FALSE(WorkspacePointerMapping::mapClamped({10, 20}, {{0, 0}, {1920, 1080}}, {{}, {}}).has_value());
    EXPECT_FALSE(WorkspacePointerMapping::mapClamped(Vector2D{INFINITY, 20.0}, {{0, 0}, {1920, 1080}}, {{0, 0}, {1920, 1080}}).has_value());
}
