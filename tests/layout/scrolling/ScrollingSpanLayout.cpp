#include <layout/algorithm/tiled/scrolling/ScrollTapeController.hpp>
#include <layout/algorithm/tiled/scrolling/ScrollingSpanLayout.hpp>

#include <gtest/gtest.h>

#include <limits>

using namespace Layout::Tiled;

TEST(ScrollingSpanLayout, extractSpanReservationUsesVerticalSecondaryInHorizontalMode) {
    // In horizontal mode (primary=X, secondary=Y): reservation is (y, h) / usable.h
    CScrollTapeController controller(SCROLL_DIR_RIGHT);
    const CBox            anchorBox       = {0, 200, 800, 400};
    const CBox            usable          = {0, 0, 1920, 1080};
    const Vector2D        workspaceOffset = {0, 0};

    const auto            params = controller.extractSpanReservation(anchorBox, usable, workspaceOffset);
    EXPECT_NEAR(params.start, 200.0F / 1080.0F, 0.0001F);
    EXPECT_NEAR(params.size, 400.0F / 1080.0F, 0.0001F);
}

TEST(ScrollingSpanLayout, extractSpanReservationUsesHorizontalSecondaryInVerticalMode) {
    // In vertical mode (primary=Y, secondary=X): reservation is (x, w) / usable.w
    CScrollTapeController controller(SCROLL_DIR_DOWN);
    const CBox            anchorBox       = {200, 0, 400, 800};
    const CBox            usable          = {0, 0, 1920, 1080};
    const Vector2D        workspaceOffset = {0, 0};

    const auto            params = controller.extractSpanReservation(anchorBox, usable, workspaceOffset);
    EXPECT_NEAR(params.start, 200.0F / 1920.0F, 0.0001F);
    EXPECT_NEAR(params.size, 400.0F / 1920.0F, 0.0001F);
}

TEST(ScrollingSpanLayout, extractSpanReservationHandlesWorkspaceOffset) {
    CScrollTapeController controller(SCROLL_DIR_RIGHT);
    const CBox            anchorBox       = {0, 300, 800, 400};
    const CBox            usable          = {0, 100, 1920, 980};
    const Vector2D        workspaceOffset = {0, 100};

    const auto            params = controller.extractSpanReservation(anchorBox, usable, workspaceOffset);
    EXPECT_NEAR(params.start, 200.0F / 980.0F, 0.0001F);  // (300 - 100) / 980
    EXPECT_NEAR(params.size, 400.0F / 980.0F, 0.0001F);
}

TEST(ScrollingSpanLayout, mergeSpanStripBoxesMergesHorizontalInPrimaryHorizontalMode) {
    CScrollTapeController controller(SCROLL_DIR_RIGHT);
    CBox                  result = {10, 100, 200, 300};  // secondary axis: y=100, h=300
    const CBox            first  = {0, 0, 640, 1080};
    const CBox            last   = {1280, 0, 640, 1080};

    controller.mergeSpanStripBoxes(result, first, last);
    EXPECT_NEAR(result.x, 0.0, 0.01);
    EXPECT_NEAR(result.w, 1920.0, 0.01);
    // secondary axis must be untouched
    EXPECT_NEAR(result.y, 100.0, 0.01);
    EXPECT_NEAR(result.h, 300.0, 0.01);
}

TEST(ScrollingSpanLayout, mergeSpanStripBoxesMergesVerticalInPrimaryVerticalMode) {
    CScrollTapeController controller(SCROLL_DIR_DOWN);
    CBox                  result = {100, 10, 300, 200};  // secondary axis: x=100, w=300
    const CBox            first  = {0, 0, 1920, 540};
    const CBox            last   = {0, 540, 1920, 540};

    controller.mergeSpanStripBoxes(result, first, last);
    EXPECT_NEAR(result.y, 0.0, 0.01);
    EXPECT_NEAR(result.h, 1080.0, 0.01);
    // secondary axis must be untouched
    EXPECT_NEAR(result.x, 100.0, 0.01);
    EXPECT_NEAR(result.w, 300.0, 0.01);
}

TEST(ScrollingSpanLayout, mergeSpanStripBoxesHandlesReversedOrder) {
    // first/last might be in any spatial order (reversed scrolling)
    CScrollTapeController controller(SCROLL_DIR_RIGHT);
    CBox                  result = {0, 50, 100, 150};
    const CBox            first  = {1280, 0, 640, 1080};
    const CBox            last   = {0, 0, 640, 1080};

    controller.mergeSpanStripBoxes(result, first, last);
    EXPECT_NEAR(result.x, 0.0, 0.01);
    EXPECT_NEAR(result.w, 1920.0, 0.01);
}

TEST(ScrollingSpanLayout, spanRangeClampsToExistingColumns) {
    const auto range = spanRangeFor(2, SSpanState{.prev = 4, .next = 3}, 5);

    ASSERT_TRUE(range.has_value());
    EXPECT_EQ(range->first, 0);
    EXPECT_EQ(range->last, 4);
    EXPECT_TRUE(range->contains(0));
    EXPECT_TRUE(range->contains(2));
    EXPECT_TRUE(range->contains(4));
    EXPECT_FALSE(range->contains(5));
}

TEST(ScrollingSpanLayout, spanRangeRejectsInvalidAnchor) {
    EXPECT_FALSE(spanRangeFor(3, SSpanState{.prev = 1, .next = 1}, 3).has_value());
    EXPECT_FALSE(spanRangeFor(0, SSpanState{.prev = 0, .next = 0}, 0).has_value());
}

TEST(ScrollingSpanLayout, spanRangeClampsRightWithoutOverflow) {
    const size_t anchorColumn = std::numeric_limits<size_t>::max() - 2;
    const auto   range        = spanRangeFor(anchorColumn, SSpanState{.prev = 0, .next = 5}, anchorColumn + 2);

    ASSERT_TRUE(range.has_value());
    EXPECT_EQ(range->first, anchorColumn);
    EXPECT_EQ(range->last, anchorColumn + 1);
    EXPECT_GE(range->last, anchorColumn);
}

TEST(ScrollingSpanLayout, spanRangeTreatsNegativeSpanAsZero) {
    const auto range = spanRangeFor(2, SSpanState{.prev = -1, .next = -2}, 5);

    ASSERT_TRUE(range.has_value());
    EXPECT_EQ(range->first, 2);
    EXPECT_EQ(range->last, 2);
}

TEST(ScrollingSpanLayout, virtualSlotPreservesTopMiddleAndBottomOrder) {
    EXPECT_EQ(virtualSlotFor(0, 3, 2), 0);
    EXPECT_EQ(virtualSlotFor(1, 3, 2), 1);
    EXPECT_EQ(virtualSlotFor(2, 3, 2), 2);

    EXPECT_EQ(virtualSlotFor(0, 3, 1), 0);
    EXPECT_EQ(virtualSlotFor(1, 3, 1), 0);
    EXPECT_EQ(virtualSlotFor(2, 3, 1), 1);

    EXPECT_EQ(virtualSlotFor(0, 4, 1), 0);
    EXPECT_EQ(virtualSlotFor(1, 4, 1), 0);
    EXPECT_EQ(virtualSlotFor(2, 4, 1), 1);
    EXPECT_EQ(virtualSlotFor(3, 4, 1), 1);
}

TEST(ScrollingSpanLayout, virtualSlotSingleOrEmptySourceReturnsDestinationRealCount) {
    EXPECT_EQ(virtualSlotFor(0, 0, 7), 7);
    EXPECT_EQ(virtualSlotFor(0, 1, 7), 7);
}

TEST(ScrollingSpanLayout, spanDependencyCycleDetectsMutualSpanning) {
    // Column 0 spans right into column 1.
    // Column 1 spans left into column 0. This is a cycle.
    const std::vector<std::vector<SSpanState>> spans = {
        {SSpanState{.next = 1}}, // col 0 → col 1
        {SSpanState{.prev = 1}},  // col 1 → col 0
        {},
    };

    EXPECT_TRUE(hasSpanDependencyCycle(spans, 3));
}

TEST(ScrollingSpanLayout, spanDependencyCycleAllowsLinearChain) {
    // Column 0 → column 1 → column 2. No cycle.
    // Column 1 IS covered by column 0 but this is not a cycle.
    const std::vector<std::vector<SSpanState>> spans = {
        {SSpanState{.next = 2}}, // col 0 → col 1, col 2
        {SSpanState{.next = 1}}, // col 1 → col 2
        {},
    };

    EXPECT_FALSE(hasSpanDependencyCycle(spans, 3));
}

TEST(ScrollingSpanLayout, spanDependencyCycleAllowsUnaffectedActiveAnchor) {
    // Column 0 spans right into column 1. Column 2 has an inactive span.
    // No column is both anchor and covered → no cycle.
    const std::vector<std::vector<SSpanState>> spans = {
        {SSpanState{.next = 1}},
        {},
        {SSpanState{.prev = 0, .next = 0}},
    };

    EXPECT_FALSE(hasSpanDependencyCycle(spans, 3));
}

TEST(ScrollingSpanLayout, columnValidationAcceptsGapsWithEnoughRoom) {
    const auto result = validateColumnReservations(
        SColumnSpanValidation{
            .realTargetCount = 2,
            .reservations =
                {
                    SSpanReservation{.slot = 1, .start = 0.40F, .size = 0.20F},
                },
        },
        0.10F);

    EXPECT_TRUE(result.valid) << result.error;
}

TEST(ScrollingSpanLayout, columnValidationRejectsFullHeightReservationWithRemainingWindow) {
    const auto result = validateColumnReservations(
        SColumnSpanValidation{
            .realTargetCount = 1,
            .reservations =
                {
                    SSpanReservation{.slot = 1, .start = 0.F, .size = 1.F},
                },
        },
        0.10F);

    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.error, "not enough space for real targets around reservations");
}

TEST(ScrollingSpanLayout, columnValidationRejectsOverlappingBands) {
    const auto result = validateColumnReservations(
        SColumnSpanValidation{
            .realTargetCount = 2,
            .reservations =
                {
                    SSpanReservation{.slot = 0, .start = 0.20F, .size = 0.40F},
                    SSpanReservation{.slot = 2, .start = 0.50F, .size = 0.20F},
                },
        },
        0.10F);

    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.error, "reservation bands overlap");
}

TEST(ScrollingSpanLayout, columnValidationAcceptsRepeatedSlotsWhenBandsDoNotOverlap) {
    const auto result = validateColumnReservations(
        SColumnSpanValidation{
            .realTargetCount = 2,
            .reservations =
                {
                    SSpanReservation{.slot = 1, .start = 0.20F, .size = 0.20F},
                    SSpanReservation{.slot = 1, .start = 0.60F, .size = 0.20F},
                },
        },
        0.10F);

    EXPECT_TRUE(result.valid) << result.error;
}

TEST(ScrollingSpanLayout, columnValidationRejectsSlotOutOfRange) {
    const auto result = validateColumnReservations(
        SColumnSpanValidation{
            .realTargetCount = 1,
            .reservations =
                {
                    SSpanReservation{.slot = 2, .start = 0.20F, .size = 0.20F},
                },
        },
        0.10F);

    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.error, "reservation slot out of range");
}

TEST(ScrollingSpanLayout, columnValidationRejectsBandOutOfRange) {
    const auto result = validateColumnReservations(
        SColumnSpanValidation{
            .realTargetCount = 1,
            .reservations =
                {
                    SSpanReservation{.slot = 0, .start = 0.90F, .size = 0.20F},
                },
        },
        0.10F);

    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.error, "reservation band out of range");
}

TEST(ScrollingSpanLayout, columnValidationRejectsSlotOrderConflictingWithBandOrder) {
    const auto result = validateColumnReservations(
        SColumnSpanValidation{
            .realTargetCount = 2,
            .reservations =
                {
                    SSpanReservation{.slot = 0, .start = 0.60F, .size = 0.10F},
                    SSpanReservation{.slot = 1, .start = 0.20F, .size = 0.10F},
                },
        },
        0.10F);

    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.error, "reservation slot order conflicts with band order");
}

TEST(ScrollingSpanLayout, columnLayoutFitsRealTargetsAroundReservation) {
    const auto result = layoutColumnWithReservations(
        SColumnSpanValidation{
            .realTargetCount = 2,
            .reservations =
                {
                    SSpanReservation{.slot = 1, .start = 0.40F, .size = 0.20F},
                },
        },
        {1.F, 1.F}, 0.10F);

    ASSERT_TRUE(result.valid()) << result.validation.error;
    ASSERT_EQ(result.realTargets.size(), 2);
    EXPECT_EQ(result.realTargets[0].index, 0);
    EXPECT_FLOAT_EQ(result.realTargets[0].start, 0.F);
    EXPECT_FLOAT_EQ(result.realTargets[0].size, 0.40F);
    EXPECT_EQ(result.realTargets[1].index, 1);
    EXPECT_FLOAT_EQ(result.realTargets[1].start, 0.60F);
    EXPECT_FLOAT_EQ(result.realTargets[1].size, 0.40F);
}

TEST(ScrollingSpanLayout, columnLayoutReportsVirtualRowsAroundReservations) {
    const auto result = layoutColumnWithReservations(
        SColumnSpanValidation{
            .realTargetCount = 1,
            .reservations =
                {
                    SSpanReservation{.slot = 0, .start = 0.F, .size = 0.50F},
                },
        },
        {1.F}, 0.10F);

    ASSERT_TRUE(result.valid()) << result.validation.error;
    ASSERT_EQ(result.realTargets.size(), 1);
    EXPECT_EQ(result.realTargets[0].index, 0);
    EXPECT_FLOAT_EQ(result.realTargets[0].start, 0.50F);
    EXPECT_FLOAT_EQ(result.realTargets[0].size, 0.50F);
    EXPECT_EQ(result.realTargets[0].virtualIndex, 1);
    EXPECT_EQ(result.realTargets[0].virtualCount, 2);
}

TEST(ScrollingSpanLayout, columnLayoutPlacesRealTargetAfterRepeatedSlotReservations) {
    const auto result = layoutColumnWithReservations(
        SColumnSpanValidation{
            .realTargetCount = 1,
            .reservations =
                {
                    SSpanReservation{.slot = 0, .start = 0.F, .size = 0.33F},
                    SSpanReservation{.slot = 0, .start = 0.33F, .size = 0.33F},
                },
        },
        {1.F}, 0.10F);

    ASSERT_TRUE(result.valid()) << result.validation.error;
    ASSERT_EQ(result.realTargets.size(), 1);
    EXPECT_EQ(result.realTargets[0].index, 0);
    EXPECT_FLOAT_EQ(result.realTargets[0].start, 0.66F);
    EXPECT_FLOAT_EQ(result.realTargets[0].size, 0.34F);
    EXPECT_EQ(result.realTargets[0].virtualIndex, 2);
    EXPECT_EQ(result.realTargets[0].virtualCount, 3);
}

TEST(ScrollingSpanLayout, columnLayoutKeepsUnevenTargetsAtLeastMinimumHeight) {
    const auto result = layoutColumnWithReservations(
        SColumnSpanValidation{
            .realTargetCount = 2,
        },
        {100.F, 1.F}, 0.40F);

    ASSERT_TRUE(result.valid()) << result.validation.error;
    ASSERT_EQ(result.realTargets.size(), 2);
    EXPECT_GE(result.realTargets[0].size, 0.40F);
    EXPECT_GE(result.realTargets[1].size, 0.40F);
}

TEST(ScrollingSpanLayout, spanAfterColumnInsertGrowsRightSpanInsideRange) {
    const auto span = spanAfterColumnInsert(1, SSpanState{.next = 1}, 2, 3);

    ASSERT_TRUE(span.has_value());
    EXPECT_EQ(span->prev, 0);
    EXPECT_EQ(span->next, 2);
}

TEST(ScrollingSpanLayout, spanAfterColumnInsertGrowsLeftSpanInsideRange) {
    const auto span = spanAfterColumnInsert(1, SSpanState{.prev = 1}, 1, 3);

    ASSERT_TRUE(span.has_value());
    EXPECT_EQ(span->prev, 2);
    EXPECT_EQ(span->next, 0);
}

TEST(ScrollingSpanLayout, spanAfterColumnInsertLeavesSpanOutsideRangeUnchanged) {
    const auto span = spanAfterColumnInsert(1, SSpanState{.next = 1}, 3, 3);

    ASSERT_TRUE(span.has_value());
    EXPECT_EQ(span->prev, 0);
    EXPECT_EQ(span->next, 1);
}

TEST(ScrollingSpanLayout, columnInsertAfterFocusedSpanUsesRightEdgeOfSpan) {
    EXPECT_EQ(columnInsertAfterFocusedSpan(1, SSpanState{.next = 1}, 3), 3);
}

TEST(ScrollingSpanLayout, columnInsertAfterFocusedSpanClampsAtColumnCount) {
    EXPECT_EQ(columnInsertAfterFocusedSpan(1, SSpanState{.next = 3}, 3), 3);
}

TEST(ScrollingSpanLayout, columnInsertAfterFocusedSpanUsesAnchorForLeftOnlySpan) {
    EXPECT_EQ(columnInsertAfterFocusedSpan(2, SSpanState{.prev = 2}, 4), 3);
}

TEST(ScrollingSpanLayout, spanAfterColumnRemoveShrinksRightSpanInsideRange) {
    const auto span = spanAfterColumnRemove(1, SSpanState{.next = 3}, 2, 5);

    ASSERT_TRUE(span.has_value());
    EXPECT_EQ(span->prev, 0);
    EXPECT_EQ(span->next, 2);
}

TEST(ScrollingSpanLayout, spanAfterColumnRemoveShrinksLeftSpanInsideRange) {
    const auto span = spanAfterColumnRemove(3, SSpanState{.prev = 3}, 2, 5);

    ASSERT_TRUE(span.has_value());
    EXPECT_EQ(span->prev, 2);
    EXPECT_EQ(span->next, 0);
}

TEST(ScrollingSpanLayout, spanAfterColumnRemoveShiftsSpanOutsideRange) {
    const auto span = spanAfterColumnRemove(3, SSpanState{.next = 1}, 1, 5);

    ASSERT_TRUE(span.has_value());
    EXPECT_EQ(span->prev, 0);
    EXPECT_EQ(span->next, 1);
}

TEST(ScrollingSpanLayout, spanAfterColumnRemoveRejectsRemovedAnchor) {
    EXPECT_FALSE(spanAfterColumnRemove(1, SSpanState{.next = 1}, 1, 3).has_value());
}

TEST(ScrollingSpanLayout, columnLayoutRejectsWhenTotalSpaceCannotFitOverflowTargets) {
    // 2 real targets, reservation occupying 0.30→0.80.
    // Gap 1 (0→0.30): 1 target expected, fitCount=0 → 1 overflows.
    // Final gap (0.80→1.0): 1 target + 1 overflow = 2 needed, fitCount=0 → rejected.
    const auto result = layoutColumnWithReservations(
        SColumnSpanValidation{
            .realTargetCount = 2,
            .reservations =
                {
                    SSpanReservation{.slot = 1, .start = 0.30F, .size = 0.50F},
                },
        },
        {100.F, 1.F}, 0.40F);

    EXPECT_FALSE(result.valid());
    EXPECT_EQ(result.validation.error, "not enough space for real targets around reservations");
}

TEST(ScrollingSpanLayout, columnLayoutOverflowsTargetsWhenAdjacentReservationsTouch) {
    // Two reservations touching end-to-end (0→0.33 and 0.33→0.66) at
    // slots 0 and 1. The gap between them has 0 space for the 1 real
    // target expected between slot 0 and slot 1, so it overflows to
    // the final gap. 2 real targets total, both placed below the
    // reservations.
    const auto result = layoutColumnWithReservations(
        SColumnSpanValidation{
            .realTargetCount = 2,
            .reservations =
                {
                    SSpanReservation{.slot = 0, .start = 0.F, .size = 0.33F},
                    SSpanReservation{.slot = 1, .start = 0.33F, .size = 0.33F},
                },
        },
        {1.F, 1.F}, 0.10F);

    ASSERT_TRUE(result.valid()) << result.validation.error;
    ASSERT_EQ(result.realTargets.size(), 2);
    // Both real targets placed in the final gap below both reservations.
    EXPECT_GE(result.realTargets[0].start, 0.66F);
    EXPECT_GE(result.realTargets[1].start, 0.66F);
    EXPECT_GE(result.realTargets[0].size, 0.10F);
    EXPECT_GE(result.realTargets[1].size, 0.10F);
}

TEST(ScrollingSpanLayout, columnLayoutKeepsFinalTargetAtLeastMinimumHeight) {
    const auto result = layoutColumnWithReservations(
        SColumnSpanValidation{
            .realTargetCount = 2,
            .reservations =
                {
                    SSpanReservation{.slot = 0, .start = 0.F, .size = 0.20F},
                },
        },
        {100.F, 1.F}, 0.40F);

    ASSERT_TRUE(result.valid()) << result.validation.error;
    ASSERT_EQ(result.realTargets.size(), 2);
    EXPECT_GE(result.realTargets[0].size, 0.40F);
    EXPECT_GE(result.realTargets[1].size, 0.40F);
}
