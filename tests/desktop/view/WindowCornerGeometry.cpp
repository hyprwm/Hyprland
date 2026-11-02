#include <desktop/view/window/WindowCornerGeometry.hpp>
#include <hyprutils/math/Vector2D.hpp>

#include <gtest/gtest.h>

using namespace Desktop::View;
using Math::SCornerRadii;
using Hyprutils::Math::Vector2D;

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
