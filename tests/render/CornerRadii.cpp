#include <helpers/CornerRadii.hpp>
#include <gtest/gtest.h>

using namespace Hyprutils::Math;

TEST(CornerRadii, SquareCornersStaySquareWhenBordersExpand) {
    const auto radii = SCornerRadii{0, 10, 20, 0}.expanded(3) * 1.5;
    EXPECT_EQ(radii.values, (std::array<float, 4>{0, 19.5, 34.5, 0}));
    EXPECT_EQ((radii - 20).values, (std::array<float, 4>{0, 0, 14.5, 0}));
}

TEST(CornerRadii, HitTestingUsesEachCorner) {
    const SCornerRadii radii{0, 20, 0, 20};
    EXPECT_TRUE(radii.contains({1, 1}, {100, 80}, 2));
    EXPECT_FALSE(radii.contains({99, 1}, {100, 80}, 2));
    EXPECT_TRUE(radii.contains({99, 79}, {100, 80}, 2));
    EXPECT_FALSE(radii.contains({1, 79}, {100, 80}, 2));
    EXPECT_TRUE(radii.contains({50, 40}, {100, 80}, 2));
}

TEST(CornerRadii, SuperellipseKeepsMoreOfTheCornerThanACircle) {
    const SCornerRadii radii{20};
    EXPECT_FALSE(radii.contains({5, 5}, {100, 80}, 2));
    EXPECT_TRUE(radii.contains({5, 5}, {100, 80}, 4));
    EXPECT_FALSE(radii.contains({5, 5}, {100, 80}, 2.1));
    EXPECT_TRUE(radii.contains({5, 5}, {100, 80}, 3.9));
}

TEST(CornerRadii, DamagePartitionHasNoMissingOrDoubleBlendedPixels) {
    const CBox box{10.25, -20.75, 101, 79};
    CRegion    damage{CBox{-30, -40, 200, 160}};
    damage.subtract(CBox{45, 10, 9, 11});
    const auto regions = cornerRegions(box, damage, false);
    CRegion    combined;
    for (size_t i = 0; i < regions.size(); ++i) {
        for (size_t j = 0; j < i; ++j)
            EXPECT_TRUE(regions[i].copy().intersect(regions[j]).empty());
        combined.add(regions[i]);
    }
    EXPECT_TRUE(damage.copy().subtract(combined).empty());
    EXPECT_TRUE(combined.copy().subtract(damage).empty());
}

TEST(CornerRadii, DamageOutsideWindowBelongsToTheCorrespondingShadowCorner) {
    const CRegion damage{CBox{80, 80, 10, 10}};
    const auto    regions = cornerRegions({0, 0, 50, 50}, damage, false);
    EXPECT_TRUE(regions[0].empty());
    EXPECT_TRUE(regions[1].empty());
    EXPECT_TRUE(regions[3].empty());
    EXPECT_TRUE(damage.copy().subtract(regions[2]).empty());
}

TEST(CornerRadii, UniformRadiiKeepOneRegion) {
    const SCornerRadii radii{24};
    const CRegion      damage{CBox{0, 0, 100, 100}};
    const auto         regions = cornerRegions({10, 10, 80, 80}, damage, radii.uniform());
    EXPECT_EQ(radii.max(), 24);
    EXPECT_TRUE(damage.copy().subtract(regions[0]).empty());
    EXPECT_TRUE(regions[1].empty());
    EXPECT_TRUE(regions[2].empty());
    EXPECT_TRUE(regions[3].empty());
}

TEST(CornerRadii, SmallWindowsKeepCornerArcsWithinTheirQuadrants) {
    const auto radii = SCornerRadii{0, 500, 20, 40}.clamped({30, 80});
    EXPECT_EQ(radii.values, (std::array<float, 4>{0, 15, 15, 15}));
    EXPECT_TRUE(radii.contains({0, 0}, {30, 80}, 4));
    EXPECT_TRUE(radii.contains({15, 40}, {30, 80}, 4));
}
