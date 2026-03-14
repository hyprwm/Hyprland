#include <xwayland/MonitorSelection.hpp>

#include <gtest/gtest.h>

#include <helpers/memory/Memory.hpp>

static PHLMONITOR makeMonitor(const Vector2D& waylandPosition, const Vector2D& xwaylandPosition, const Vector2D& size, const Vector2D& transformedSize, float scale) {
    auto monitor                = makeShared<CMonitor>(nullptr);
    monitor->m_position         = waylandPosition;
    monitor->m_xwaylandPosition = xwaylandPosition;
    monitor->m_size             = size;
    monitor->m_transformedSize  = transformedSize;
    monitor->m_scale            = scale;
    return monitor;
}

static std::vector<PHLMONITOR> sampleLayout() {
    return {
        makeMonitor({0, 0}, {0, 0}, {1440, 2560}, {1440, 2560}, 1.F),
        makeMonitor({1440, 560}, {1440, 0}, {2560, 1440}, {2560, 1440}, 1.F),
        makeMonitor({4000, 0}, {4000, 0}, {1440, 2560}, {1440, 2560}, 1.F),
    };
}

TEST(XWaylandMonitorSelection, selectsXWaylandMonitorOnExactHorizontalBoundaries) {
    const auto monitors = sampleLayout();

    EXPECT_EQ(XWayland::selectMonitorForXWaylandPoint(monitors, {1439, 10}, false), 0U);
    EXPECT_EQ(XWayland::selectMonitorForXWaylandPoint(monitors, {1440, 10}, false), 1U);
    EXPECT_EQ(XWayland::selectMonitorForXWaylandPoint(monitors, {3999, 10}, false), 1U);
    EXPECT_EQ(XWayland::selectMonitorForXWaylandPoint(monitors, {4000, 10}, false), 2U);
}

TEST(XWaylandMonitorSelection, convertsXWaylandStripPointsToOffsetWaylandCoords) {
    const auto monitors = sampleLayout();

    EXPECT_EQ(XWayland::xwaylandToWaylandCoords(monitors, {1440, 10}, false), (Vector2D{1440, 570}));
    EXPECT_EQ(XWayland::xwaylandToWaylandCoords(monitors, {2000, 10}, false), (Vector2D{2000, 570}));
    EXPECT_EQ(XWayland::xwaylandToWaylandCoords(monitors, {3999, 10}, false), (Vector2D{3999, 570}));
}

TEST(XWaylandMonitorSelection, selectsWaylandMonitorCorrectlyAroundCornerBoundaries) {
    const auto monitors = sampleLayout();

    EXPECT_EQ(XWayland::selectMonitorForWaylandPoint(monitors, {1439, 559}, false), 0U);
    EXPECT_EQ(XWayland::selectMonitorForWaylandPoint(monitors, {1440, 560}, false), 1U);
    EXPECT_EQ(XWayland::selectMonitorForWaylandPoint(monitors, {3999, 1999}, false), 1U);
    EXPECT_EQ(XWayland::selectMonitorForWaylandPoint(monitors, {4000, 2000}, false), 2U);
}

TEST(XWaylandMonitorSelection, mapsWaylandCornerBoundaryPointsBackToExpectedXWaylandEdges) {
    const auto monitors = sampleLayout();

    EXPECT_EQ(XWayland::waylandToXWaylandCoords(monitors, {1440, 560}, false), (Vector2D{1440, 0}));
    EXPECT_EQ(XWayland::waylandToXWaylandCoords(monitors, {3999, 1999}, false), (Vector2D{3999, 1439}));
}

TEST(XWaylandMonitorSelection, roundtripsPointsInsideEachMonitorWithoutPreferredMonitor) {
    const auto monitors = sampleLayout();

    for (const auto& point : {Vector2D{100, 100}, Vector2D{2000, 1000}, Vector2D{4100, 100}}) {
        EXPECT_EQ(XWayland::xwaylandToWaylandCoords(monitors, XWayland::waylandToXWaylandCoords(monitors, point, false), false), point);
    }
}

TEST(XWaylandMonitorSelection, usesCenterMonitorForWaylandGapPointsAboveAndBelowCenter) {
    const auto monitors = sampleLayout();

    EXPECT_EQ(XWayland::selectMonitorForWaylandPoint(monitors, {2720, 559}, false), 1U);
    EXPECT_EQ(XWayland::selectMonitorForWaylandPoint(monitors, {2720, 2000}, false), 1U);
    EXPECT_EQ(XWayland::waylandToXWaylandCoords(monitors, {2720, 559}, false), (Vector2D{2720, -1}));
    EXPECT_EQ(XWayland::waylandToXWaylandCoords(monitors, {2720, 2000}, false), (Vector2D{2720, 1440}));
}

TEST(XWaylandMonitorSelection, centerOriginRoundtripStaysOnCenterMonitor) {
    const auto monitors = sampleLayout();
    const auto wayland  = XWayland::xwaylandToWaylandCoords(monitors, {1440, 0}, false);

    EXPECT_EQ(wayland, (Vector2D{1440, 560}));
    EXPECT_EQ(XWayland::waylandToXWaylandCoords(monitors, wayland, false), (Vector2D{1440, 0}));
}

TEST(XWaylandMonitorSelection, reproducesBoundaryMisclassificationFromOffsetLayout) {
    const std::vector<PHLMONITOR> monitors = {
        makeMonitor({0, 0}, {0, 0}, {1728, 3072}, {2160, 3840}, 1.25F),
        makeMonitor({1728, 865}, {2160, 0}, {3072, 1728}, {3840, 2160}, 1.25F),
        makeMonitor({4800, 0}, {6000, 0}, {1728, 3072}, {2160, 3840}, 1.25F),
    };

    EXPECT_EQ(XWayland::selectMonitorForWaylandPoint(monitors, {1728, 865}, true), 1U);

    const auto wayland = XWayland::xwaylandToWaylandCoords(monitors, {2160, 0}, true);
    EXPECT_EQ(wayland, (Vector2D{1728, 865}));
    EXPECT_EQ(XWayland::waylandToXWaylandCoords(monitors, wayland, true), (Vector2D{2160, 0}));
}
