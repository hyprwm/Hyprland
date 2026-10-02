#include <output/MonitorZoomController.hpp>

#include <gtest/gtest.h>

TEST(Render, monitorZoomDamagesActiveAndTransitionFrames) {
    Monitor::CMonitorZoomController zoom;

    EXPECT_FALSE(zoom.shouldDamageEntire(1.F));
    EXPECT_TRUE(zoom.shouldDamageEntire(2.F));
    EXPECT_TRUE(zoom.shouldDamageEntire(2.F));
    EXPECT_TRUE(zoom.shouldDamageEntire(1.F));
    EXPECT_FALSE(zoom.shouldDamageEntire(1.F));
}

TEST(Render, cursorZoomOnlyAffectsPointersMonitor) {
    EXPECT_FLOAT_EQ(Monitor::CMonitorZoomController::zoomFactor(2.F, 1.F, true), 2.F);
    EXPECT_FLOAT_EQ(Monitor::CMonitorZoomController::zoomFactor(2.F, 1.F, false), 1.F);
    EXPECT_FLOAT_EQ(Monitor::CMonitorZoomController::zoomFactor(0.5F, 1.F, true), 1.F);
    EXPECT_FLOAT_EQ(Monitor::CMonitorZoomController::zoomFactor(1.F, 1.F, true), 1.F);
}

TEST(Render, startupZoomOverridesCursorZoomOnEitherMonitor) {
    for (const bool pointerOnMonitor : {false, true}) {
        EXPECT_FLOAT_EQ(Monitor::CMonitorZoomController::zoomFactor(3.F, 0.F, pointerOnMonitor), 2.F);
        EXPECT_FLOAT_EQ(Monitor::CMonitorZoomController::zoomFactor(3.F, 0.5F, pointerOnMonitor), 1.5F);
    }
}
