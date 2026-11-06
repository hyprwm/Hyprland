#include <managers/input/BorderIconDirection.hpp>
#include <desktop/view/window/WindowCornerGeometry.hpp>
#include <hyprutils/math/Vector2D.hpp>

#include <gtest/gtest.h>

using Input::borderIconDirection;
using Math::SCornerRadii;
using Hyprutils::Math::Vector2D;

TEST(BorderIconDirection, UniformRoundingKeepsCornerLeewayAndEdgeDirections) {
    const Vector2D     SIZE{200, 120};
    const SCornerRadii ROUNDING{22.75F};
    const struct {
        Vector2D             point;
        eBorderIconDirection direction;
    } CASES[] = {
        {{33.9, -1.0}, BORDERICON_UP_LEFT},    {{34, -1}, BORDERICON_UP},     {{166, -1}, BORDERICON_UP},    {{166.1, -1.0}, BORDERICON_UP_RIGHT},
        {{-1.0, 33.9}, BORDERICON_UP_LEFT},    {{-1, 34}, BORDERICON_LEFT},   {{-1, 86}, BORDERICON_LEFT},   {{-1.0, 86.1}, BORDERICON_DOWN_LEFT},
        {{201.0, 33.9}, BORDERICON_UP_RIGHT},  {{201, 34}, BORDERICON_RIGHT}, {{201, 86}, BORDERICON_RIGHT}, {{201.0, 86.1}, BORDERICON_DOWN_RIGHT},
        {{33.9, 121.0}, BORDERICON_DOWN_LEFT}, {{34, 121}, BORDERICON_DOWN},  {{166, 121}, BORDERICON_DOWN}, {{166.1, 121.0}, BORDERICON_DOWN_RIGHT},
    };

    for (const auto& test : CASES)
        EXPECT_EQ(borderIconDirection(test.point, SIZE, ROUNDING, 2), test.direction) << test.point.x << ", " << test.point.y;
}

TEST(BorderIconDirection, EachCornerUsesItsOwnThreshold) {
    const Vector2D     SIZE{200, 120};
    const SCornerRadii ROUNDING{30, 0, 5, 15};

    EXPECT_EQ(borderIconDirection({20, -1}, SIZE, ROUNDING, 2), BORDERICON_UP_LEFT);
    EXPECT_EQ(borderIconDirection({180, -1}, SIZE, ROUNDING, 2), BORDERICON_UP);
    EXPECT_EQ(borderIconDirection({190, -1}, SIZE, ROUNDING, 2), BORDERICON_UP_RIGHT);
    EXPECT_EQ(borderIconDirection({201, 20}, SIZE, ROUNDING, 2), BORDERICON_RIGHT);
    EXPECT_EQ(borderIconDirection({180, 121}, SIZE, ROUNDING, 2), BORDERICON_DOWN);
    EXPECT_EQ(borderIconDirection({185, 121}, SIZE, ROUNDING, 2), BORDERICON_DOWN_RIGHT);
    EXPECT_EQ(borderIconDirection({-1, 90}, SIZE, ROUNDING, 2), BORDERICON_LEFT);
    EXPECT_EQ(borderIconDirection({-1, 100}, SIZE, ROUNDING, 2), BORDERICON_DOWN_LEFT);
}

TEST(BorderIconDirection, SquareCornersStillHaveTenPixelLeeway) {
    const Vector2D SIZE{200, 120};

    EXPECT_EQ(borderIconDirection({11.9, -1.0}, SIZE, {}, 2), BORDERICON_UP_LEFT);
    EXPECT_EQ(borderIconDirection({12, -1}, SIZE, {}, 2), BORDERICON_UP);
    EXPECT_EQ(borderIconDirection({188.1, 121.0}, SIZE, {}, 2), BORDERICON_DOWN_RIGHT);
    EXPECT_EQ(borderIconDirection({188, 121}, SIZE, {}, 2), BORDERICON_DOWN);
}

TEST(BorderIconDirection, RightAndBottomBoxBoundariesAreOutside) {
    const Vector2D SIZE{200, 120};

    EXPECT_EQ(borderIconDirection({200, 60}, SIZE, SCornerRadii{20}, 2), BORDERICON_RIGHT);
    EXPECT_EQ(borderIconDirection({100, 120}, SIZE, SCornerRadii{20}, 2), BORDERICON_DOWN);
}

TEST(BorderIconDirection, UniformLeewayKeepsPriorityWhenThresholdsOverlap) {
    EXPECT_EQ(borderIconDirection({-1, 20}, {200, 20}, SCornerRadii{10}, 2), BORDERICON_UP_LEFT);
    EXPECT_EQ(borderIconDirection({201, 20}, {200, 20}, SCornerRadii{10}, 2), BORDERICON_UP_RIGHT);
    EXPECT_EQ(borderIconDirection({100, 20}, {200, 20}, SCornerRadii{10}, 2), BORDERICON_UP);
}

TEST(BorderIconDirection, BottomCornerLeewayDoesNotOverrideTheTopEdge) {
    EXPECT_EQ(borderIconDirection({15, -1}, {200, 10}, SCornerRadii{0, 0, 0, 5}, 2), BORDERICON_UP);
    EXPECT_EQ(borderIconDirection({185, -1}, {200, 10}, SCornerRadii{0, 0, 5, 0}, 2), BORDERICON_UP);
}

TEST(BorderIconDirection, RoundedInteriorCutoutsSelectMatchingCorners) {
    const Vector2D     SIZE{200, 120};
    const SCornerRadii ROUNDING{30, 20, 12, 4};
    const struct {
        Vector2D             point;
        eBorderIconDirection direction;
    } CASES[] = {
        {{1, 1}, BORDERICON_UP_LEFT},
        {{199, 1}, BORDERICON_UP_RIGHT},
        {{199, 119}, BORDERICON_DOWN_RIGHT},
        {{0.5, 119.5}, BORDERICON_DOWN_LEFT},
    };

    for (const auto& test : CASES) {
        ASSERT_TRUE(Desktop::View::isInWindowCorner(test.point, SIZE, ROUNDING, 2.5F, 0));
        EXPECT_EQ(borderIconDirection(test.point, SIZE, ROUNDING, 0), test.direction);
    }
}
