#include <managers/XWaylandMonitorSelection.hpp>

#include <gtest/gtest.h>

static std::vector<SXWaylandMonitorDesc> sampleLayout() {
    return {
        {{0, 0}, {0, 0}, {1440, 2560}, {1440, 2560}, 1.F},
        {{1440, 560}, {1440, 0}, {2560, 1440}, {2560, 1440}, 1.F},
        {{4000, 0}, {4000, 0}, {1440, 2560}, {1440, 2560}, 1.F},
    };
}

TEST(XWaylandMonitorSelection, selectsXWaylandMonitorOnExactHorizontalBoundaries) {
    const auto monitors = sampleLayout();

    EXPECT_EQ(selectMonitorForXWaylandPoint(monitors, {1439, 10}, false), 0U);
    EXPECT_EQ(selectMonitorForXWaylandPoint(monitors, {1440, 10}, false), 1U);
    EXPECT_EQ(selectMonitorForXWaylandPoint(monitors, {3999, 10}, false), 1U);
    EXPECT_EQ(selectMonitorForXWaylandPoint(monitors, {4000, 10}, false), 2U);
}

TEST(XWaylandMonitorSelection, convertsXWaylandStripPointsToOffsetWaylandCoords) {
    const auto monitors = sampleLayout();

    EXPECT_EQ(xwaylandToWaylandCoords(monitors, {1440, 10}, false), (Vector2D{1440, 570}));
    EXPECT_EQ(xwaylandToWaylandCoords(monitors, {2000, 10}, false), (Vector2D{2000, 570}));
    EXPECT_EQ(xwaylandToWaylandCoords(monitors, {3999, 10}, false), (Vector2D{3999, 570}));
}

TEST(XWaylandMonitorSelection, selectsWaylandMonitorCorrectlyAroundCornerBoundaries) {
    const auto monitors = sampleLayout();

    EXPECT_EQ(selectMonitorForWaylandPoint(monitors, {1439, 559}, false), 0U);
    EXPECT_EQ(selectMonitorForWaylandPoint(monitors, {1440, 560}, false), 1U);
    EXPECT_EQ(selectMonitorForWaylandPoint(monitors, {3999, 1999}, false), 1U);
    EXPECT_EQ(selectMonitorForWaylandPoint(monitors, {4000, 2000}, false), 2U);
}

TEST(XWaylandMonitorSelection, mapsWaylandCornerBoundaryPointsBackToExpectedXWaylandEdges) {
    const auto monitors = sampleLayout();

    EXPECT_EQ(waylandToXWaylandCoords(monitors, {1440, 560}, false), (Vector2D{1440, 0}));
    EXPECT_EQ(waylandToXWaylandCoords(monitors, {3999, 1999}, false), (Vector2D{3999, 1439}));
}

TEST(XWaylandMonitorSelection, roundtripsPointsInsideEachMonitorWithoutPreferredMonitor) {
    const auto monitors = sampleLayout();

    for (const auto& point : {Vector2D{100, 100}, Vector2D{2000, 1000}, Vector2D{4100, 100}}) {
        EXPECT_EQ(xwaylandToWaylandCoords(monitors, waylandToXWaylandCoords(monitors, point, false), false), point);
    }
}

TEST(XWaylandMonitorSelection, usesCenterMonitorForWaylandGapPointsAboveAndBelowCenter) {
    const auto monitors = sampleLayout();

    EXPECT_EQ(selectMonitorForWaylandPoint(monitors, {2720, 559}, false), 1U);
    EXPECT_EQ(selectMonitorForWaylandPoint(monitors, {2720, 2000}, false), 1U);
    EXPECT_EQ(waylandToXWaylandCoords(monitors, {2720, 559}, false), (Vector2D{2720, -1}));
    EXPECT_EQ(waylandToXWaylandCoords(monitors, {2720, 2000}, false), (Vector2D{2720, 1440}));
}

TEST(XWaylandMonitorSelection, centerOriginRoundtripStaysOnCenterMonitor) {
    const auto monitors = sampleLayout();
    const auto wayland  = xwaylandToWaylandCoords(monitors, {1440, 0}, false);

    EXPECT_EQ(wayland, (Vector2D{1440, 560}));
    EXPECT_EQ(waylandToXWaylandCoords(monitors, wayland, false), (Vector2D{1440, 0}));
}

TEST(XWaylandMonitorSelection, reproducesBoundaryMisclassificationFromOffsetLayout) {
    const std::vector<SXWaylandMonitorDesc> monitors = {
        {{0, 0}, {0, 0}, {1728, 3072}, {2160, 3840}, 1.25F},
        {{1728, 865}, {2160, 0}, {3072, 1728}, {3840, 2160}, 1.25F},
        {{4800, 0}, {6000, 0}, {1728, 3072}, {2160, 3840}, 1.25F},
    };

    EXPECT_EQ(selectMonitorForWaylandPoint(monitors, {1728, 865}, true), 1U);

    const auto wayland = xwaylandToWaylandCoords(monitors, {2160, 0}, true);
    EXPECT_EQ(wayland, (Vector2D{1728, 865}));
    EXPECT_EQ(waylandToXWaylandCoords(monitors, wayland, true), (Vector2D{2160, 0}));
}
