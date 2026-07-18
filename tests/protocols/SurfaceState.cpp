#include <protocols/types/SurfaceState.hpp>

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <span>

static void expectInputHitTestMatchesMaterializedRegion(const SSurfaceState& state, const std::span<const Vector2D> points) {
    const auto region = state.effectiveInputRegion();
    for (const auto& point : points)
        EXPECT_EQ(state.inputContainsPoint(point), region.containsPoint(point)) << "point: " << point.x << ", " << point.y;
}

static void expectTranslatedInputHitTestMatchesMaterializedRegion(const SSurfaceState& state, const Vector2D& offset, const std::span<const Vector2D> points) {
    auto region = state.effectiveInputRegion();
    region.translate(offset);
    for (const auto& point : points)
        EXPECT_EQ(state.inputContainsPoint(point, offset), region.containsPoint(point)) << "point: " << point.x << ", " << point.y;
}

TEST(SurfaceState, infiniteInputHitTestPreservesIntegerTruncation) {
    SSurfaceState state;
    state.size = {10.75, 8.25};

    constexpr std::array points = {
        Vector2D{-1.1, 0.0}, Vector2D{-0.5, 0.0}, Vector2D{0.0, 0.0}, Vector2D{9.999, 7.999}, Vector2D{10.0, 7.0}, Vector2D{9.0, 8.0},
    };

    expectInputHitTestMatchesMaterializedRegion(state, points);
    EXPECT_FALSE(state.inputContainsPoint({-1.1, 0.0}));
    EXPECT_TRUE(state.inputContainsPoint({-0.5, 0.0}));
    EXPECT_TRUE(state.inputContainsPoint({9.999, 7.999}));
    EXPECT_FALSE(state.inputContainsPoint({10.0, 7.0}));
}

TEST(SurfaceState, finiteInputHitTestPreservesFractionalEdges) {
    SSurfaceState state;
    state.size            = {10, 8};
    state.inputIsInfinite = false;
    state.input           = CRegion{2, 1, 3, 2};

    constexpr std::array points = {
        Vector2D{1.999, 1.0}, Vector2D{2.0, 1.0}, Vector2D{4.999, 2.999}, Vector2D{5.0, 2.0}, Vector2D{4.0, 3.0}, Vector2D{9.0, 7.0},
    };

    expectInputHitTestMatchesMaterializedRegion(state, points);
    EXPECT_FALSE(state.inputContainsPoint({1.999, 1.0}));
    EXPECT_TRUE(state.inputContainsPoint({2.0, 1.0}));
    EXPECT_TRUE(state.inputContainsPoint({4.999, 2.999}));
    EXPECT_FALSE(state.inputContainsPoint({5.0, 2.0}));
    EXPECT_FALSE(state.inputContainsPoint({4.0, 3.0}));
}

TEST(SurfaceState, translatedHitTestTruncatesPointAndOffsetSeparately) {
    SSurfaceState state;
    state.size = {1, 1};

    constexpr std::array infinitePoints = {
        Vector2D{0.9, 0.0},
        Vector2D{1.1, 0.0},
    };
    expectTranslatedInputHitTestMatchesMaterializedRegion(state, {0.9, 0.0}, infinitePoints);
    EXPECT_FALSE(state.inputContainsPoint({1.1, 0.0}, {0.9, 0.0}));

    state.size            = {3, 1};
    state.inputIsInfinite = false;
    state.input           = CRegion{1, 0, 1, 1};

    constexpr std::array finitePoints = {
        Vector2D{0.9, 0.0},
        Vector2D{1.1, 0.0},
        Vector2D{2.1, 0.0},
    };
    expectTranslatedInputHitTestMatchesMaterializedRegion(state, {0.9, 0.0}, finitePoints);
    EXPECT_TRUE(state.inputContainsPoint({1.1, 0.0}, {0.9, 0.0}));
    EXPECT_TRUE(state.inputContainsPoint({2.1, 0.0}, {1.9, 0.0}));
}

TEST(SurfaceState, rejectsNonFiniteAndOutOfRangePoints) {
    SSurfaceState state;
    state.size = {10, 8};

    const auto nan      = std::numeric_limits<double>::quiet_NaN();
    const auto infinity = std::numeric_limits<double>::infinity();
    const auto tooLarge = sc<double>(std::numeric_limits<int32_t>::max()) + 1.0;
    const auto tooSmall = sc<double>(std::numeric_limits<int32_t>::min()) - 1.0;

    EXPECT_FALSE(state.inputContainsPoint({nan, 0.0}));
    EXPECT_FALSE(state.inputContainsPoint({0.0, nan}));
    EXPECT_FALSE(state.inputContainsPoint({infinity, 0.0}));
    EXPECT_FALSE(state.inputContainsPoint({-infinity, 0.0}));
    EXPECT_FALSE(state.inputContainsPoint({tooLarge, 0.0}));
    EXPECT_FALSE(state.inputContainsPoint({tooSmall, 0.0}));
    EXPECT_FALSE(state.inputContainsPoint({std::numeric_limits<double>::max(), 0.0}));
    EXPECT_FALSE(state.inputContainsPoint({0.0, 0.0}, {nan, 0.0}));
    EXPECT_FALSE(state.inputContainsPoint({0.0, 0.0}, {infinity, 0.0}));
    EXPECT_FALSE(state.inputContainsPoint({0.0, 0.0}, {tooLarge, 0.0}));

    state.size = {sc<double>(std::numeric_limits<int32_t>::max()), 1.0};
    EXPECT_TRUE(state.inputContainsPoint({sc<double>(std::numeric_limits<int32_t>::max()) - 1.0, 0.0}));
    EXPECT_FALSE(state.inputContainsPoint({sc<double>(std::numeric_limits<int32_t>::max()), 0.0}));
}

TEST(SurfaceState, rejectsTranslatedRegionsThatOverflowPixmanCoordinates) {
    SSurfaceState state;
    state.size          = {10, 1};
    const auto intMax   = sc<double>(std::numeric_limits<int32_t>::max());
    const auto maxStart = intMax - 10.0;

    EXPECT_TRUE(state.inputContainsPoint({intMax - 1.0, 0.0}, {maxStart, 0.0}));
    EXPECT_FALSE(state.inputContainsPoint({intMax, 0.0}, {intMax, 0.0}));

    state.inputIsInfinite = false;
    state.input           = CRegion{0, 0, 1, 1};
    EXPECT_TRUE(state.inputContainsPoint({intMax - 1.0, 0.0}, {intMax - 1.0, 0.0}));
}

TEST(SurfaceState, rejectsInvalidSurfaceSizes) {
    SSurfaceState state;
    const auto    infinity = std::numeric_limits<double>::infinity();
    const auto    tooLarge = sc<double>(std::numeric_limits<int32_t>::max()) + 1.0;

    state.size = {0, 10};
    EXPECT_FALSE(state.inputContainsPoint({0, 0}));

    state.size = {10, -1};
    EXPECT_FALSE(state.inputContainsPoint({0, 0}));

    state.size = {infinity, 10.0};
    EXPECT_FALSE(state.inputContainsPoint({0, 0}));
    EXPECT_TRUE(state.effectiveInputRegion().empty());

    state.size = {std::numeric_limits<double>::quiet_NaN(), 10.0};
    EXPECT_FALSE(state.inputContainsPoint({0, 0}));
    EXPECT_TRUE(state.effectiveInputRegion().empty());

    state.size = {tooLarge, 10.0};
    EXPECT_FALSE(state.inputContainsPoint({0, 0}));
    EXPECT_TRUE(state.effectiveInputRegion().empty());
}
