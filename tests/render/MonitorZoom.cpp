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
