#include <layout/target/WindowGeometry.hpp>
#include <layout/target/TargetEdges.hpp>
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

TEST(WindowGeometry, MergedBordersOverlapExactlyAcrossScalesAndPixelPhases) {
    using enum Layout::eTargetEdge;
    for (const float scale : {1.F, 1.2F, 1.25F, 1.5F, 1.75F, 2.F, 3.F}) {
        for (const int border : {0, 1, 2, 3, 5}) {
            for (const int offset : {0, 1, 2, 3}) {
                SCOPED_TRACE(::testing::Message() << "scale=" << scale << " border=" << border << " offset=" << offset);
                const Vector2D MONITOR  = {-2048, 137};
                const CBox     LEFT     = CBox{offset, 31, 801, 600}.translate(MONITOR);
                const CBox     RIGHT    = CBox{offset + 801, 31, 799, 600}.translate(MONITOR);
                const auto     RESERVED = borderExtents(border, scale);
                const auto     L        = pixelBox(Layout::mergedWindowContentBox(LEFT, RESERVED, {.right = INNER}, border, MONITOR, scale), scale, MONITOR);
                const auto     R        = pixelBox(Layout::mergedWindowContentBox(RIGHT, RESERVED, {.left = INNER}, border, MONITOR, scale), scale, MONITOR);
                const auto     WIDTH    = Render::borderSizeInPixels(border, scale);

                EXPECT_EQ(R.x - (L.x + L.w), WIDTH);
                EXPECT_EQ(L.x - WIDTH, pixelBox(LEFT, scale, MONITOR).x);
                EXPECT_EQ(R.x + R.w + WIDTH, pixelBox(RIGHT, scale, MONITOR).x + pixelBox(RIGHT, scale, MONITOR).w);
                EXPECT_EQ(L.y - WIDTH, pixelBox(LEFT, scale, MONITOR).y);
            }
        }
    }
}

TEST(WindowGeometry, MergedBordersMeetAtTJunctionsAndGridCorners) {
    using enum Layout::eTargetEdge;
    for (const float scale : {1.F, 1.25F, 1.5F, 2.F}) {
        for (const int border : {1, 2, 3}) {
            const auto RESERVED = borderExtents(border, scale);
            const auto WIDTH    = Render::borderSizeInPixels(border, scale);
            const auto content  = [&](const CBox& box, const Layout::STargetEdges& edges) {
                return pixelBox(Layout::mergedWindowContentBox(box, RESERVED, edges, border, {}, scale), scale);
            };
            const auto LEFT         = content({0, 0, 401, 602}, {.right = INNER});
            const auto TOP_RIGHT    = content({401, 0, 399, 301}, {.bottom = INNER, .left = INNER});
            const auto BOTTOM_RIGHT = content({401, 301, 399, 301}, {.top = INNER, .left = INNER});

            EXPECT_EQ(TOP_RIGHT.x - (LEFT.x + LEFT.w), WIDTH);
            EXPECT_EQ(BOTTOM_RIGHT.x - (LEFT.x + LEFT.w), WIDTH);
            EXPECT_EQ(BOTTOM_RIGHT.y - (TOP_RIGHT.y + TOP_RIGHT.h), WIDTH);

            const auto TOP_LEFT    = content({0, 0, 401, 301}, {.right = INNER, .bottom = INNER});
            const auto BOTTOM_LEFT = content({0, 301, 401, 301}, {.top = INNER, .right = INNER});
            EXPECT_EQ(BOTTOM_LEFT.y - (TOP_LEFT.y + TOP_LEFT.h), WIDTH);
            EXPECT_EQ(TOP_LEFT.y + TOP_LEFT.h, TOP_RIGHT.y + TOP_RIGHT.h);
            EXPECT_EQ(BOTTOM_LEFT.y, BOTTOM_RIGHT.y);
            EXPECT_TRUE(TOP_LEFT.copy().expand(WIDTH).intersection(BOTTOM_RIGHT).empty());
        }
    }
}

TEST(WindowGeometry, MergingPreservesOuterEdgesAndOtherReservations) {
    using enum Layout::eTargetEdge;
    const auto BORDER   = borderExtents(3, 1.5F);
    auto       reserved = BORDER;
    reserved.topLeft.y += 24;
    EXPECT_EQ(Layout::mergedBorderReserved(reserved, {}, 3, 1.5F), reserved);

    const Layout::STargetEdges EDGES         = {.top = INNER, .right = INNER, .bottom = INNER, .left = INNER};
    const auto                 MERGED        = Layout::mergedBorderReserved(reserved, EDGES, 3, 1.5F);
    const auto                 MERGED_BORDER = Layout::mergedBorderReserved(BORDER, EDGES, 3, 1.5F);
    EXPECT_DOUBLE_EQ(MERGED.topLeft.y - MERGED_BORDER.topLeft.y, 24);
    EXPECT_EQ(MERGED.topLeft.x, MERGED_BORDER.topLeft.x);
    EXPECT_EQ(MERGED.bottomRight, MERGED_BORDER.bottomRight);
}

TEST(WindowGeometry, MergedBordersShareNegativeHalfCoordinateBoundaries) {
    using enum Layout::eTargetEdge;
    for (const float scale : {0.5F, 1.F, 1.25F, 1.5F, 2.F}) {
        for (const double seam : {-959.5, -959.0, -0.5, 0.5, 959.5}) {
            for (const Vector2D monitor : {Vector2D{}, Vector2D{-1920, -1080}}) {
                for (const bool snapTarget : {false, true}) {
                    SCOPED_TRACE(::testing::Message() << "scale=" << scale << " seam=" << seam << " snapTarget=" << snapTarget);
                    const auto WIDTH   = Render::borderSizeInPixels(3, scale);
                    const auto content = [&](CBox box, const Layout::STargetEdges& edges) {
                        if (snapTarget)
                            box = Layout::roundTargetBox(box);
                        return pixelBox(Layout::mergedWindowContentBox(box, borderExtents(3, scale), edges, 3, monitor, scale), scale, monitor);
                    };
                    const auto LEFT         = content({seam - 801, seam - 301, 801, 602}, {.right = INNER});
                    const auto TOP_RIGHT    = content({seam, seam - 301, 799, 301}, {.bottom = INNER, .left = INNER});
                    const auto BOTTOM_RIGHT = content({seam, seam, 799, 301}, {.top = INNER, .left = INNER});

                    EXPECT_EQ(TOP_RIGHT.x - (LEFT.x + LEFT.w), WIDTH);
                    EXPECT_EQ(BOTTOM_RIGHT.x - (LEFT.x + LEFT.w), WIDTH);
                    EXPECT_EQ(BOTTOM_RIGHT.y - (TOP_RIGHT.y + TOP_RIGHT.h), WIDTH);
                    EXPECT_TRUE(LEFT.copy().expand(WIDTH).intersection(TOP_RIGHT).empty());
                    EXPECT_TRUE(TOP_RIGHT.copy().expand(WIDTH).intersection(BOTTOM_RIGHT).empty());
                }
            }
        }
    }
}
