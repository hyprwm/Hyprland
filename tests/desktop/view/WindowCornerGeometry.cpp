#include <desktop/view/window/WindowCornerGeometry.hpp>
#include <hyprutils/math/Vector2D.hpp>

#include <gtest/gtest.h>

using namespace Desktop::View;
using Math::SCornerRadii;
using Hyprutils::Math::Vector2D;

TEST(WindowCornerGeometry, UniformRadiiRespectCurrentBounds) {
    const SCornerRadii RAW{80};

    EXPECT_EQ(effectiveWindowRounding(RAW, 2.F, {100, 20}), SCornerRadii{10});
    EXPECT_EQ(effectiveWindowRounding(RAW, 2.F, {20, 100}), SCornerRadii{10});
    EXPECT_EQ(effectiveWindowRounding(RAW, 2.F, {100, 100}), SCornerRadii{50});
}

TEST(WindowCornerGeometry, ClampFollowsPowerAndPrecedesInsets) {
    const auto RADII = effectiveWindowRounding(SCornerRadii{80}, 1.F, {100, 20});

    EXPECT_EQ(RADII, SCornerRadii{10});
    EXPECT_EQ(RADII.inset(1), SCornerRadii{9});
    EXPECT_EQ(RADII.expanded(2), SCornerRadii{12});
    EXPECT_EQ(effectiveWindowRounding(SCornerRadii{3}, 2.5F, {100, 20}), SCornerRadii{3.75F});
}

TEST(WindowCornerGeometry, AsymmetricFractionalPowerPreservesSquareCorners) {
    const SCornerRadii RAW{0, 3, 40, 100};

    EXPECT_EQ(effectiveWindowRounding(RAW, 2.5F, {200, 80}), (SCornerRadii{0, 3.75F, 40, 40}));
    EXPECT_EQ(effectiveWindowRounding(RAW, 2.5F, {101.5, 31.5}), (SCornerRadii{0, 3.75F, 15.75F, 15.75F}));
}

TEST(WindowCornerGeometry, PixelBoundsClampDerivedRadiiWithoutReusingRawValues) {
    const auto BASE   = effectiveWindowRounding(SCornerRadii{0, 80, 4, 100}, 2.F, {100.0, 20.5});
    const auto PIXELS = BASE * 1.5F;

    EXPECT_EQ(PIXELS.inset(1).clamped({150, 31}), (SCornerRadii{0, 14.375F, 5, 14.375F}));
    EXPECT_EQ(PIXELS.expanded(3).clamped({156, 36}), (SCornerRadii{0, 18, 9, 18}));
}

TEST(WindowCornerGeometry, ResizeRecomputesWithoutChangingRawRadii) {
    const SCornerRadii RAW{0, 3, 40, 100};
    const auto         ORIGINAL = RAW;
    const auto         LARGE    = effectiveWindowRounding(RAW, 2.5F, {400, 400});

    EXPECT_EQ(LARGE, (SCornerRadii{0, 3.75F, 50, 125}));
    EXPECT_EQ(effectiveWindowRounding(RAW, 2.5F, {30, 300}), (SCornerRadii{0, 3.75F, 15, 15}));
    EXPECT_EQ(effectiveWindowRounding(RAW, 2.5F, {0, 300}), SCornerRadii{});
    EXPECT_EQ(effectiveWindowRounding(RAW, 2.5F, {400, 400}), LARGE);
    EXPECT_EQ(RAW, ORIGINAL);
}

TEST(WindowCornerGeometry, HitAndBorderGuardUseClampedGeometry) {
    const SCornerRadii RAW{80};
    const Vector2D     LARGE{200, 200};
    const Vector2D     SMALL{100, 20};
    const auto         SMALL_RADII = effectiveWindowRounding(RAW, 2.F, SMALL);

    EXPECT_TRUE(isInWindowCorner({5, 5}, LARGE, effectiveWindowRounding(RAW, 2.F, LARGE), 2.F, 0));
    EXPECT_FALSE(isInWindowCorner({5, 5}, SMALL, SMALL_RADII, 2.F, 0));
    EXPECT_TRUE(isInWindowCorner({1, 1}, SMALL, SMALL_RADII, 2.F, 9));
    EXPECT_FALSE(isInWindowCorner({1, 1}, SMALL, SMALL_RADII, 2.F, 10));
}

TEST(WindowCornerGeometry, BorderGuardUsesOnlyTheSelectedCorner) {
    const Vector2D     SIZE{200, 120};
    const SCornerRadii RADII{30, 0, 12, 4};

    EXPECT_TRUE(isInWindowCorner({1, 1}, SIZE, RADII, 2.F, 6));
    EXPECT_FALSE(isInWindowCorner({199, 1}, SIZE, RADII, 2.F, 6));
    EXPECT_TRUE(isInWindowCorner({199, 119}, SIZE, RADII, 2.F, 6));
    EXPECT_FALSE(isInWindowCorner({1, 119}, SIZE, RADII, 2.F, 6));
    EXPECT_TRUE(isInWindowCorner({1, 119}, SIZE, RADII, 2.F, 3));
    EXPECT_FALSE(isInWindowCorner({1, 119}, SIZE, RADII, 2.F, 4));
}

TEST(WindowCornerGeometry, HitTestingRetainsFractionalRadiusAndPower) {
    const Vector2D SIZE{200, 120};

    EXPECT_TRUE(isInWindowCorner({0.4, 0.4}, SIZE, SCornerRadii{1.5F}, 2.F, 1));
    EXPECT_FALSE(isInWindowCorner({0.5, 0.5}, SIZE, SCornerRadii{1.5F}, 2.F, 1));
    EXPECT_TRUE(isInWindowCorner({5, 5}, SIZE, SCornerRadii{20}, 2.F, 0));
    EXPECT_FALSE(isInWindowCorner({5, 5}, SIZE, SCornerRadii{20}, 2.5F, 0));
}

TEST(WindowCornerGeometry, StraightEdgesAndInteriorRemainVisible) {
    const Vector2D     SIZE{200, 120};
    const SCornerRadii RADII{30, 0, 12, 4};

    EXPECT_FALSE(isInWindowCorner({30, 0}, SIZE, RADII, 2.F, 0));
    EXPECT_FALSE(isInWindowCorner({200, 108}, SIZE, RADII, 2.F, 0));
    EXPECT_FALSE(isInWindowCorner({4, 120}, SIZE, RADII, 2.F, 0));
    EXPECT_FALSE(isInWindowCorner({100, 60}, SIZE, RADII, 2.F, 0));
}
