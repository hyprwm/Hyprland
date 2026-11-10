#include <render/SurfaceDamage.hpp>

#include <gtest/gtest.h>

using namespace Hyprutils::Math;

TEST(SurfaceDamage, FractionalContentDamagesTheLastRenderedColumn) {
    for (const double width : {100.0, 3836.0}) {
        const double   SCALE       = 1.5;
        const Vector2D SIZE        = {width / SCALE, 100.0};
        const Vector2D CLIENT_SIZE = SIZE.floor();

        for (const Vector2D monitorPosition : {Vector2D{}, Vector2D{1920, 137}}) {
            const auto POSITION = monitorPosition + Vector2D{2 / SCALE, 47 / SCALE};
            const auto DRAW     = CBox{POSITION - monitorPosition, SIZE}.scale(SCALE).round();

            for (const bool edgeOnly : {false, true}) {
                auto damage = edgeOnly ? CRegion{CLIENT_SIZE.x - 1, 0, 1, CLIENT_SIZE.y} : CRegion{CBox{{}, CLIENT_SIZE}};
                Render::clipSurfaceDamage(damage, SIZE);
                const auto PIXELS = Render::surfaceDamageToPixels(damage, POSITION - monitorPosition, SCALE);

                CRegion    expected{edgeOnly ? CBox{DRAW.x + DRAW.w - 1, DRAW.y, 1, DRAW.h} : DRAW};
                EXPECT_TRUE(expected.subtract(PIXELS).empty());
            }
        }
    }
}

TEST(SurfaceDamage, FractionalPixelOriginDoesNotTruncateInLogicalSpace) {
    const CRegion DAMAGE{0, 0, 10, 10};
    for (const double scale : {1.25, 1.5, 1.75}) {
        const auto POSITION = Vector2D{2 / scale, 3 / scale};
        auto       pixels   = Render::surfaceDamageToPixels(DAMAGE, POSITION, scale);
        EXPECT_EQ(pixels.getExtents().pos(), Vector2D(2, 3));
        CRegion uncovered{CBox{POSITION, {10, 10}}.scale(scale).round()};
        EXPECT_TRUE(uncovered.subtract(pixels).empty());
    }
}

TEST(SurfaceDamage, IntegerSizeDoesNotExpandPartialDamage) {
    CRegion damage{20, 30, 10, 20};
    Render::clipSurfaceDamage(damage, {100, 100});

    EXPECT_EQ(damage.getExtents(), CBox(20, 30, 10, 20));
    EXPECT_EQ(Render::surfaceDamageToPixels(damage, {4, 8}, 1.25).getExtents(), CBox(30, 47, 13, 26));
}

TEST(SurfaceDamage, NegativeHalfPixelOriginsCoverTheVisibleFarEdge) {
    const CRegion  DAMAGE{0, 0, 5, 5};
    const Vector2D POSITION = {-4.5, -4.5};
    const auto     PIXELS   = Render::surfaceDamageToPixels(DAMAGE, POSITION, 1);
    CRegion        uncovered{CBox{POSITION, {5, 5}}.round()};
    EXPECT_TRUE(uncovered.subtract(PIXELS).empty());
    EXPECT_TRUE(PIXELS.containsPoint({0, 0}));
}

TEST(SurfaceDamage, FractionalSizeKeepsEmptyDamageEmpty) {
    CRegion damage;
    Render::clipSurfaceDamage(damage, {100 / 1.5, 100.0});
    EXPECT_TRUE(damage.empty());
    EXPECT_TRUE(Render::surfaceDamageToPixels(damage, {2 / 1.5, 2 / 1.5}, 1.5).empty());
}

TEST(SurfaceDamage, FractionalSizeKeepsDisjointDamageSeparate) {
    CRegion damage{0, 0, 3, 3};
    damage.add(CBox{20, 20, 3, 3});
    Render::clipSurfaceDamage(damage, {100 / 1.5, 100 / 1.5});

    EXPECT_FALSE(damage.containsPoint({10, 10}));
    EXPECT_TRUE(damage.containsPoint({23, 23}));
    EXPECT_FALSE(damage.containsPoint({-1, -1}));
}

TEST(SurfaceDamage, FractionalSizeExpandsSinglePixelDamageInAllDirections) {
    CRegion damage{20, 20, 1, 1};
    Render::clipSurfaceDamage(damage, {100 / 1.5, 100 / 1.5});

    const CRegion EXPECTED{19, 19, 3, 3};
    EXPECT_TRUE(pixman_region32_equal(damage.pixman(), EXPECTED.pixman()));
}
