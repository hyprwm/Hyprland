#include <helpers/math/CornerRadii.hpp>
#include <config/shared/complex/ComplexDataTypes.hpp>
#include <hyprutils/math/Vector2D.hpp>

#include <gtest/gtest.h>

using namespace Math;
using Hyprutils::Math::Vector2D;

TEST(CornerRadii, CssFieldsFollowClockwiseCornerOrder) {
    const SCornerRadii RADII{Config::CCssGapData{3, 7, 11, 19}};
    EXPECT_EQ(RADII.at({0, 0}, {100, 80}), 3.F);
    EXPECT_EQ(RADII.at({100, 0}, {100, 80}), 7.F);
    EXPECT_EQ(RADII.at({100, 80}, {100, 80}), 11.F);
    EXPECT_EQ(RADII.at({0, 80}, {100, 80}), 19.F);
}

TEST(CornerRadii, ScalingPreservesFractionalPowerAndMonitorScale) {
    const auto RADII = SCornerRadii{3, 0, 7, 11} * 1.25F * 1.5F;
    EXPECT_EQ(RADII, (SCornerRadii{5.625F, 0.F, 13.125F, 20.625F}));
    EXPECT_EQ(RADII / 1.5F, (SCornerRadii{3.75F, 0.F, 8.75F, 13.75F}));
}

TEST(CornerRadii, ClampUsesSmallerDimensionAndPrecedesInset) {
    EXPECT_EQ(SCornerRadii{80}.clamped({100, 20}), SCornerRadii{10});
    EXPECT_EQ(SCornerRadii{80}.clamped({100, 20}).inset(1), SCornerRadii{9});
    EXPECT_EQ((SCornerRadii{0, 4, 80, -1}.clamped({20, 100})), (SCornerRadii{0, 4, 10, 0}));
    EXPECT_EQ(SCornerRadii{80}.clamped({0, 20}), SCornerRadii{0});
    EXPECT_EQ(SCornerRadii{80}.clamped({20, -1}), SCornerRadii{0});
}

TEST(CornerRadii, BorderExpansionAndInsetPreserveSquareCorners) {
    const SCornerRadii RADII{0, 0.5F, 5, 10};
    EXPECT_EQ(RADII.expanded(2), (SCornerRadii{0, 2.5F, 7, 12}));
    EXPECT_EQ(RADII.inset(1), (SCornerRadii{0, 0, 4, 9}));
    EXPECT_FALSE(RADII.inset(10).isRounded());
    EXPECT_EQ(RADII.max(), 10.F);
}

TEST(CornerRadii, HitTestingUsesSelectedCornerAndFractionalPower) {
    const Vector2D     SIZE{100, 80};
    const SCornerRadii RADII{20, 0, 10, 5};
    EXPECT_TRUE(isInRoundedCorner({1, 1}, SIZE, RADII, 2.F));
    EXPECT_FALSE(isInRoundedCorner({99, 1}, SIZE, RADII, 2.F));
    EXPECT_TRUE(isInRoundedCorner({99, 79}, SIZE, RADII, 2.F));
    EXPECT_TRUE(isInRoundedCorner({0.1, 79.9}, SIZE, RADII, 2.F));
    EXPECT_FALSE(isInRoundedCorner({20, 1}, SIZE, RADII, 2.F));
    EXPECT_FALSE(isInRoundedCorner({50, 40}, SIZE, RADII, 2.F));
    EXPECT_TRUE(isInRoundedCorner({5, 5}, SIZE, RADII, 2.F));
    EXPECT_FALSE(isInRoundedCorner({5, 5}, SIZE, RADII, 2.5F));
}
