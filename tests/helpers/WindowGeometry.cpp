#include <layout/target/WindowGeometry.hpp>
#include <render/decorations/BorderGeometry.hpp>

#include <gtest/gtest.h>
#include <tuple>

static SBoxExtents borderExtents(int borderSize, float scale) {
    const auto EXTENT = Render::borderExtentInLogical(borderSize, scale);
    return {{EXTENT, EXTENT}, {EXTENT, EXTENT}};
}

static CBox pixelBox(CBox box, double scale, const Vector2D& monitorPosition = {}) {
    return box.translate(-monitorPosition).scale(scale).round();
}

TEST(WindowGeometry, FractionalBordersFitFourKWorkArea) {
    const CBox ALLOCATION = {0, 30, 2560, 1410};
    const auto CONTENT    = Layout::windowContentBox(ALLOCATION, borderExtents(1, 1.5), {}, 1.5);
    const auto PIXELS     = pixelBox(CONTENT, 1.5);

    EXPECT_EQ(PIXELS, CBox(2, 47, 3836, 2111));
    EXPECT_EQ(PIXELS.copy().expand(2), pixelBox(ALLOCATION, 1.5));
    EXPECT_EQ(CONTENT.size().floor(), Vector2D(2557, 1407));
    EXPECT_EQ((CONTENT.size().floor() * 1.5).round(), PIXELS.size());
}

TEST(WindowGeometry, OddBarReservationKeepsBothVerticalBorders) {
    const CBox ALLOCATION = {0, 31, 2560, 1409};
    const auto CONTENT    = Layout::windowContentBox(ALLOCATION, borderExtents(1, 1.5), {}, 1.5);
    const auto PIXELS     = pixelBox(CONTENT, 1.5);

    EXPECT_EQ(PIXELS, CBox(2, 49, 3836, 2109));
    EXPECT_EQ(PIXELS.copy().expand(2), pixelBox(ALLOCATION, 1.5));
}

TEST(WindowGeometry, QuarterScalePreservesVisibleGeometry) {
    const CBox ALLOCATION = {0, 30, 2048, 1122};
    const auto CONTENT    = Layout::windowContentBox(ALLOCATION, borderExtents(1, 1.25), {}, 1.25);

    EXPECT_EQ(pixelBox(CONTENT, 1.25), pixelBox(ALLOCATION.copy().expand(-1), 1.25));
    EXPECT_EQ(pixelBox(CONTENT, 1.25).expand(1), pixelBox(ALLOCATION, 1.25));
    EXPECT_EQ(CONTENT.size().floor(), Vector2D(2046, 1120));
}

TEST(WindowGeometry, BordersStayInsideAllocationAcrossPixelPhases) {
    for (const double scale : {1.0, 1.25, 1.5, 1.75, 2.0}) {
        for (const int border : {0, 1, 2, 3}) {
            for (const int offset : {0, 1, 2, 3}) {
                SCOPED_TRACE(::testing::Message() << "scale=" << scale << " border=" << border << " offset=" << offset);
                const CBox ALLOCATION = {offset, offset, 801, 601};
                const auto CONTENT    = Layout::windowContentBox(ALLOCATION, borderExtents(border, scale), {}, scale);
                const auto PIXELS     = pixelBox(CONTENT, scale);

                EXPECT_EQ(PIXELS.copy().expand(Render::borderSizeInPixels(border, scale)), pixelBox(ALLOCATION, scale));
            }
        }
    }
}

TEST(WindowGeometry, PixelSnappingIsMonitorLocal) {
    const CBox LOCAL_ALLOCATION = {11, 31, 801, 601};
    const auto EXPECTED         = pixelBox(Layout::windowContentBox(LOCAL_ALLOCATION, borderExtents(1, 1.5), {}, 1.5), 1.5);

    for (const Vector2D position : {Vector2D{-2048, -173}, Vector2D{2560, 137}}) {
        const auto CONTENT = Layout::windowContentBox(LOCAL_ALLOCATION.copy().translate(position), borderExtents(1, 1.5), position, 1.5);
        EXPECT_EQ(pixelBox(CONTENT, 1.5, position), EXPECTED);
    }
}

TEST(WindowGeometry, AdjacentTilesShareTheSamePhysicalBoundary) {
    const CBox LEFT          = {0, 30, 1235, 1410};
    const CBox RIGHT         = {1235, 30, 1325, 1410};
    const auto RESERVED      = borderExtents(1, 1.5);
    const auto LEFT_CONTENT  = pixelBox(Layout::windowContentBox(LEFT, RESERVED, {}, 1.5), 1.5);
    const auto RIGHT_CONTENT = pixelBox(Layout::windowContentBox(RIGHT, RESERVED, {}, 1.5), 1.5);

    EXPECT_EQ(LEFT_CONTENT.x + LEFT_CONTENT.w + 2, RIGHT_CONTENT.x - 2);
    EXPECT_EQ(LEFT_CONTENT.copy().expand(2), pixelBox(LEFT, 1.5));
    EXPECT_EQ(RIGHT_CONTENT.copy().expand(2), pixelBox(RIGHT, 1.5));
}

TEST(WindowGeometry, OtherDecorationReservationsRemainSeparate) {
    const CBox ALLOCATION = {0, 30, 2560, 1410};
    auto       reserved   = borderExtents(1, 1.5);
    reserved.topLeft.y += 24;

    const auto CONTENT = pixelBox(Layout::windowContentBox(ALLOCATION, reserved, {}, 1.5), 1.5);
    EXPECT_EQ(CONTENT, CBox(2, 83, 3836, 2075));
    EXPECT_EQ(CONTENT.y - 2 - 36, pixelBox(ALLOCATION, 1.5).y);
    EXPECT_EQ(CONTENT.y + CONTENT.h + 2, 2160);
}

TEST(WindowGeometry, IntegerScalesPreserveLogicalContent) {
    const CBox        ALLOCATION = {17, 31, 800, 600};
    const SBoxExtents RESERVED   = {{2, 26}, {2, 2}};

    for (const double scale : {1.0, 2.0, 3.0})
        EXPECT_EQ(Layout::windowContentBox(ALLOCATION, RESERVED, {}, scale), CBox(19, 57, 796, 572));
}

TEST(WindowGeometry, FloatScaleDoesNotReduceAnIntegralClientSize) {
    const double SCALE      = 1.2F;
    const CBox   ALLOCATION = {0, 0, 252, 252};
    const auto   CONTENT    = Layout::windowContentBox(ALLOCATION, borderExtents(1, SCALE), {}, SCALE);

    EXPECT_EQ(CONTENT.size(), Vector2D(250, 250));
    EXPECT_EQ(CONTENT.size().floor(), Vector2D(250, 250));
    EXPECT_EQ(pixelBox(CONTENT, SCALE), CBox(1, 1, 300, 300));
    EXPECT_EQ(pixelBox(CONTENT, SCALE).expand(1), pixelBox(ALLOCATION, SCALE));
}

TEST(WindowGeometry, ReservedBordersUseTheRenderersFloatRounding) {
    for (const auto& [border, scale, expected] : {std::tuple{5, 1.3F, 7}, std::tuple{3, 7.F / 6.F, 4}}) {
        EXPECT_EQ(Render::borderSizeInPixels(border, scale), expected);
        const CBox ALLOCATION = {0, 0, 800, 600};
        const auto CONTENT    = Layout::windowContentBox(ALLOCATION, borderExtents(border, scale), {}, scale);
        EXPECT_EQ(pixelBox(CONTENT, scale).expand(expected), pixelBox(ALLOCATION, scale));
    }
}
