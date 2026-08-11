#include <layout/algorithm/tiled/scrolling/ScrollTapeController.hpp>

#include <gtest/gtest.h>

using namespace Layout::Tiled;

TEST(Layout, scrollCameraOffsetResetsAtExactExtent) {
    const CBox            USABLE_AREA{0, 0, 1920, 1080};
    CScrollTapeController controller;

    controller.addStrip(0.25F);
    controller.addStrip(0.5F);
    controller.addStrip(0.5F);
    controller.fitStrip(2, USABLE_AREA);

    EXPECT_GT(controller.getOffset(), 0.0);

    controller.getStrip(2).size = 0.25F;

    EXPECT_DOUBLE_EQ(controller.calculateMaxExtent(USABLE_AREA), USABLE_AREA.w);
    EXPECT_DOUBLE_EQ(controller.calculateCameraOffset(USABLE_AREA), 0.0);
    EXPECT_DOUBLE_EQ(controller.getOffset(), 0.0);
}
