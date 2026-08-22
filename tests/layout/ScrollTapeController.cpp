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

TEST(Layout, scrollCameraOffsetPreservesAlignmentAtExactExtent) {
    const CBox            USABLE_AREA{0, 0, 1920, 1080};
    CScrollTapeController controller;

    controller.addStrip(0.5F);
    controller.addStrip(0.5F);

    controller.centerStrip(1, USABLE_AREA);
    const double centeredOffset = controller.getOffset();
    EXPECT_GT(centeredOffset, 0.0);
    EXPECT_DOUBLE_EQ(controller.calculateCameraOffset(USABLE_AREA), centeredOffset);

    controller.setOffset(USABLE_AREA.w);
    controller.fitStrip(1, USABLE_AREA);
    const double fittedOffset = controller.getOffset();
    EXPECT_GT(fittedOffset, 0.0);
    EXPECT_DOUBLE_EQ(controller.calculateCameraOffset(USABLE_AREA), fittedOffset);
}
