#include <render/SurfaceGeometry.hpp>
#include <render/SurfaceDamage.hpp>

#include <gtest/gtest.h>
#include <limits>

using namespace Hyprutils::Math;

TEST(SurfaceGeometry, FractionalConfigureRemaindersAreProjectedPerAxis) {
    EXPECT_EQ(Render::subsurfaceScale({100.5, 80.25}, {100, 80}, false), Vector2D(1.005, 80.25 / 80));
    EXPECT_EQ(Render::subsurfaceScale({100, 80}, {100, 80}, false), Vector2D(1, 1));
    EXPECT_EQ(Render::subsurfaceScale({100.5, 80.25}, {90, 80}, false), Vector2D(1.0, 80.25 / 80));
    EXPECT_EQ(Render::subsurfaceScale({100.5, 80.25}, {101, 90}, false), Vector2D(1, 1));
}

TEST(SurfaceGeometry, AnimationSettlesWithoutChangingTheSubsurfaceProjection) {
    const Vector2D SIZE = {1438.0 / 1.33333F, 1918.0 / 1.33333F};
    EXPECT_EQ(Render::subsurfaceScale(SIZE, SIZE.floor(), true), Render::subsurfaceScale(SIZE, SIZE.floor(), false));
    EXPECT_EQ(Render::subsurfaceScale({50, 40}, {100, 80}, true), Vector2D(0.5, 0.5));
}

TEST(SurfaceGeometry, InvalidDimensionsDoNotProduceInvalidTransforms) {
    EXPECT_EQ(Render::subsurfaceScale({100.5, 80.25}, {0, -1}, true), Vector2D(1, 1));
    EXPECT_EQ(Render::subsurfaceScale({0, -1}, {100, 80}, true), Vector2D(1, 1));
    EXPECT_EQ(Render::subsurfaceScale({std::numeric_limits<double>::infinity(), 80.0}, {100.0, std::numeric_limits<double>::quiet_NaN()}, true), Vector2D(1, 1));
}

TEST(SurfaceGeometry, OffsetSubsurfaceEdgeDamageCoversProjectedTrailingPixels) {
    for (const float scale : {1.33333F, 4.F / 3.F, 1.25F, 1.5F, 2.F}) {
        const Vector2D SIZE     = {1438.0 / scale, 1918.0 / scale};
        const auto     FACTOR   = Render::subsurfaceScale(SIZE, SIZE.floor(), false);
        const Vector2D OFFSET   = {17, 23};
        const auto     CHILD    = SIZE.floor() - OFFSET;
        const auto     POSITION = Vector2D{1.0 / scale, 1.0 / scale} + OFFSET * FACTOR;
        const auto     DRAW     = CBox{POSITION, CHILD * FACTOR}.scale(scale).round();
        CRegion        damage{CHILD.x - 1, 0, 1, CHILD.y};
        damage.add(CBox{0, CHILD.y - 1, CHILD.x, 1});
        damage.scale(FACTOR);
        Render::clipSurfaceDamage(damage, CHILD * FACTOR);
        const auto PIXELS = Render::surfaceDamageToPixels(damage, POSITION, scale);
        CRegion    expected{DRAW.x + DRAW.w - 1, DRAW.y, 1, DRAW.h};
        expected.add(CBox{DRAW.x, DRAW.y + DRAW.h - 1, DRAW.w, 1});
        EXPECT_TRUE(expected.subtract(PIXELS).empty());
        EXPECT_FALSE(PIXELS.containsPoint(DRAW.pos() + DRAW.size() / 2));
    }
}
