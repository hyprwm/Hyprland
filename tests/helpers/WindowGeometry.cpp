#include <layout/target/WindowGeometry.hpp>
#include <render/decorations/BorderGeometry.hpp>

#include <gtest/gtest.h>
#include <cmath>
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

TEST(WindowGeometry, NegativeGapsCancelBordersAcrossSplitPhases) {
    for (const float scale : {1.F, 1.25F, 1.5F, 1.75F, 2.F, 1.3F, 7.F / 6.F}) {
        for (const int border : {1, 2, 3, 5}) {
            const auto GAPS     = Layout::innerGapExtentsInPixels({{-border, -border}, {-border, -border}}, border, scale);
            const auto RESERVED = borderExtents(border, scale);
            const auto PIXELS   = Render::borderSizeInPixels(border, scale);
            EXPECT_EQ(GAPS.topLeft, Vector2D(-PIXELS, -PIXELS));
            EXPECT_EQ(GAPS.bottomRight, GAPS.topLeft);

            for (const double split : {-2., -1.5, -0.5, 0., 0.5, 1.5, 100., 100.25, 100.5, 100.75}) {
                for (const Vector2D origin : {Vector2D{}, Vector2D{-2048, 137}}) {
                    SCOPED_TRACE(::testing::Message() << "scale=" << scale << " border=" << border << " split=" << split);
                    const CBox LEFT             = CBox{-200, -200, split + 200, split + 200}.translate(origin);
                    const CBox RIGHT            = CBox{split, split, 200, 200}.translate(origin);
                    const auto LEFT_ALLOCATION  = Layout::boxWithPixelGaps(LEFT, GAPS, origin, scale);
                    const auto RIGHT_ALLOCATION = Layout::boxWithPixelGaps(RIGHT, GAPS, origin, scale);
                    const auto LEFT_CONTENT     = pixelBox(Layout::windowContentBox(LEFT_ALLOCATION, RESERVED, origin, scale), scale, origin);
                    const auto RIGHT_CONTENT    = pixelBox(Layout::windowContentBox(RIGHT_ALLOCATION, RESERVED, origin, scale), scale, origin);
                    const auto EDGE             = std::round(split * scale);

                    EXPECT_EQ(LEFT_CONTENT.pos() + LEFT_CONTENT.size(), Vector2D(EDGE, EDGE));
                    EXPECT_EQ(RIGHT_CONTENT.pos(), Vector2D(EDGE, EDGE));
                    const auto LEFT_PIXELS  = pixelBox(LEFT_ALLOCATION, scale, origin);
                    const auto RIGHT_PIXELS = pixelBox(RIGHT_ALLOCATION, scale, origin);
                    EXPECT_EQ(LEFT_PIXELS.x + LEFT_PIXELS.w - RIGHT_PIXELS.x, 2 * PIXELS);
                }
            }
        }
    }
}

TEST(WindowGeometry, GapLengthsAreRoundedPerSide) {
    const auto GAPS = Layout::gapExtentsInPixels({{2.0, 2.5}, {-0.5, 0.25}}, 1.25F);
    EXPECT_EQ(GAPS.topLeft, Vector2D(3, 3));
    EXPECT_EQ(GAPS.bottomRight, Vector2D(-1, 0));

    const auto LEFT  = Layout::boxWithPixelGaps({0, 0, 100.5, 100}, GAPS, {}, 1.25);
    const auto RIGHT = Layout::boxWithPixelGaps({100.5, 0, 100, 100}, GAPS, {}, 1.25);
    EXPECT_EQ(pixelBox(RIGHT, 1.25).x - (pixelBox(LEFT, 1.25).x + pixelBox(LEFT, 1.25).w), 2);

    const auto SYMMETRIC = Layout::gapExtentsInPixels({{2, 2}, {2, 2}}, 1.25F);
    const auto A         = pixelBox(Layout::boxWithPixelGaps({0, 0, 100.5, 100}, SYMMETRIC, {}, 1.25), 1.25);
    const auto B         = pixelBox(Layout::boxWithPixelGaps({100.5, 0, 100, 100}, SYMMETRIC, {}, 1.25), 1.25);
    EXPECT_EQ(B.x - (A.x + A.w), 6);
}

TEST(WindowGeometry, HalfBorderGapsMakeFacingBorderStripsCoincide) {
    for (const float scale : {1.F, 1.25F, 1.5F, 1.75F, 2.F, 1.3F, 7.F / 6.F}) {
        for (const int border : {1, 2, 3, 5}) {
            const double GAP      = -border / 2.0;
            const auto   GAPS     = Layout::innerGapExtentsInPixels({{GAP, GAP}, {GAP, GAP}}, border, scale);
            const auto   RESERVED = borderExtents(border, scale);
            const auto   PIXELS   = Render::borderSizeInPixels(border, scale);
            EXPECT_EQ(GAPS.topLeft + GAPS.bottomRight, Vector2D(-PIXELS, -PIXELS));

            for (const double split : {-2., -1.5, -0.5, 0., 0.5, 1.5, 100., 100.25, 100.5, 100.75}) {
                for (const Vector2D origin : {Vector2D{}, Vector2D{-2048, 137}}) {
                    SCOPED_TRACE(::testing::Message() << "scale=" << scale << " border=" << border << " split=" << split);
                    const auto content = [&](CBox allocation) {
                        allocation.translate(origin);
                        const auto GAPPED = Layout::boxWithPixelGaps(allocation, GAPS, origin, scale);
                        return pixelBox(Layout::windowContentBox(GAPPED, RESERVED, origin, scale), scale, origin);
                    };
                    const auto LEFT   = content({-200, -200, split + 200, 400});
                    const auto RIGHT  = content({split, -200, 200, 400});
                    const auto TOP    = content({-200, -200, 400, split + 200});
                    const auto BOTTOM = content({-200, split, 400, 200});

                    EXPECT_EQ(CBox(LEFT.x + LEFT.w, LEFT.y, PIXELS, LEFT.h), CBox(RIGHT.x - PIXELS, RIGHT.y, PIXELS, RIGHT.h));
                    EXPECT_EQ(CBox(TOP.x, TOP.y + TOP.h, TOP.w, PIXELS), CBox(BOTTOM.x, BOTTOM.y - PIXELS, BOTTOM.w, PIXELS));
                    EXPECT_EQ(RIGHT.x - (LEFT.x + LEFT.w), PIXELS);
                    EXPECT_EQ(BOTTOM.y - (TOP.y + TOP.h), PIXELS);
                }
            }
        }
    }
}

TEST(WindowGeometry, InnerGapRoundingPreservesPositiveAndBorderlessGaps) {
    const SBoxExtents POSITIVE = {{0.0, 2.5}, {0.5, 2.0}};
    const SBoxExtents MIXED    = {{-0.5, 2.5}, {-2.5, 0.0}};
    for (const float scale : {1.F, 1.25F, 1.5F, 2.F}) {
        const auto EXPECTED = Layout::gapExtentsInPixels(POSITIVE, scale);
        for (const int border : {0, 1, 2, 3}) {
            const auto ACTUAL = Layout::innerGapExtentsInPixels(POSITIVE, border, scale);
            EXPECT_EQ(ACTUAL.topLeft, EXPECTED.topLeft);
            EXPECT_EQ(ACTUAL.bottomRight, EXPECTED.bottomRight);
        }
        const auto BORDERLESS = Layout::innerGapExtentsInPixels(MIXED, 0, scale);
        const auto OLD        = Layout::gapExtentsInPixels(MIXED, scale);
        EXPECT_EQ(BORDERLESS.topLeft, OLD.topLeft);
        EXPECT_EQ(BORDERLESS.bottomRight, OLD.bottomRight);
    }

    const auto INVISIBLE = Layout::innerGapExtentsInPixels(MIXED, 1, 0.25F);
    const auto OLD       = Layout::gapExtentsInPixels(MIXED, 0.25F);
    EXPECT_EQ(INVISIBLE.topLeft, OLD.topLeft);
    EXPECT_EQ(INVISIBLE.bottomRight, OLD.bottomRight);
}

TEST(WindowGeometry, IntermediateNegativeGapsAssignOddPixelsToLeadingEdges) {
    for (const auto& [gap, leading, trailing] : {std::tuple{-0.25, -1, 0}, std::tuple{-0.5, -1, -1}, std::tuple{-1.0, -2, -1}, std::tuple{-1.5, -3, -2}}) {
        SCOPED_TRACE(gap);
        const auto GAPS = Layout::innerGapExtentsInPixels({{gap, gap}, {gap, gap}}, 2, 1.25F);
        EXPECT_EQ(GAPS.topLeft, Vector2D(leading, leading));
        EXPECT_EQ(GAPS.bottomRight, Vector2D(trailing, trailing));
        EXPECT_EQ(Layout::innerGapPairInPixels(gap, 2, 1.25F), leading + trailing);
    }
}

TEST(WindowGeometry, FractionalAllocationsAndGapsSurviveIntegerScale) {
    const CBox ALLOCATION = {10.25, 20.25, 100.5, 50.5};
    const auto GAPS       = Layout::gapExtentsInPixels({{0.5, 1.5}, {2.5, 0.0}}, 2.F);
    const auto GAPPED     = Layout::boxWithPixelGaps(ALLOCATION, GAPS, {}, 2);
    EXPECT_EQ(pixelBox(GAPPED, 2), CBox(22, 44, 195, 98));
    EXPECT_EQ(Layout::windowContentBox(GAPPED, {}, {}, 2), GAPPED);
}

TEST(WindowGeometry, NegativeHalfPixelEndpointsShareOneBoundary) {
    const auto LEFT  = Layout::windowContentBox({-10, 0, 8.5, 10}, {}, {}, 1);
    const auto RIGHT = Layout::windowContentBox({-1.5, 0, 10, 10}, {}, {}, 1);
    EXPECT_EQ(LEFT.x + LEFT.w, -2);
    EXPECT_EQ(RIGHT.x, -2);
}

TEST(WindowGeometry, CenteredScrollingColumnsSharePixelEdges) {
    constexpr float SCALE = 1.25F;
    for (const Vector2D origin : {Vector2D{}, Vector2D{-2048, 137}}) {
        const auto OUTER  = Layout::gapExtentsInPixels({{5, 5}, {5, 5}}, SCALE);
        const auto AREA   = Layout::boxWithPixelGaps(CBox{origin, {1536, 864}}, OUTER, origin, SCALE);
        const auto WIDTH  = AREA.w * 0.25F;
        double     extent = 0;
        for (int i = 0; i < 3; ++i) {
            extent += WIDTH;
        }
        const auto OFFSET = (extent - AREA.w) / 2.0;
        // Match consecutive centered scrolling columns: the shared edge becomes 721.5 on one
        // side and 721.4999999999999 on the other before device-pixel snapping.
        const CBox LEFT  = {Vector2D{-OFFSET, 0.0} + AREA.pos(), {WIDTH, AREA.h}};
        const CBox RIGHT = {Vector2D{WIDTH - OFFSET, 0.0} + AREA.pos(), {WIDTH, AREA.h}};
        for (const double gap : {0.0, 5.0, -1.0, -0.5}) {
            const auto GAPS = Layout::innerGapExtentsInPixels({{gap, gap}, {gap, gap}}, 1, SCALE);
            const auto A    = pixelBox(Layout::boxWithPixelGaps(LEFT, GAPS, origin, SCALE), SCALE, origin);
            const auto B    = pixelBox(Layout::boxWithPixelGaps(RIGHT, GAPS, origin, SCALE), SCALE, origin);
            EXPECT_EQ(A.x + A.w + GAPS.bottomRight.x, 722);
            EXPECT_EQ(B.x - GAPS.topLeft.x, 722);
            EXPECT_EQ(B.x - (A.x + A.w), GAPS.bottomRight.x + GAPS.topLeft.x);
            if (gap != -0.5)
                continue;
            EXPECT_EQ(B.x - (A.x + A.w), -1);
        }
    }
}

TEST(WindowGeometry, HalfTiesAccountForMonitorOriginCancellation) {
    constexpr double SCALE = 1.25;
    for (const double origin : {-4096.0, 0.0, 4096.0}) {
        for (const double half : {-1.5, -0.5, 0.5, 1.5}) {
            for (const double direction : {-1.0, 1.0}) {
                const auto POSITION  = origin + half / SCALE;
                const auto PERTURBED = std::nextafter(POSITION, POSITION + direction);
                const auto CONTENT   = Layout::windowContentBox({PERTURBED, PERTURBED, 80, 80}, {}, {origin, origin}, SCALE);
                EXPECT_EQ(pixelBox(CONTENT, SCALE, {origin, origin}).pos(), Vector2D(std::round(half), std::round(half)));
            }
        }
    }
}

TEST(WindowGeometry, CoordinatesOutsideHalfTieToleranceKeepNormalRounding) {
    for (const double half : {-1.5, -0.5, 0.5, 1.5}) {
        for (const double delta : {-1e-6, 1e-6}) {
            const auto VALUE   = half + delta;
            const auto CONTENT = Layout::windowContentBox({VALUE, VALUE, 80, 80}, {}, {}, 1);
            EXPECT_EQ(CONTENT.pos(), Vector2D(std::round(VALUE), std::round(VALUE)));
        }
    }
}
