#include <layout/target/TargetEdges.hpp>

#include <gtest/gtest.h>

using namespace Layout;

TEST(TargetEdges, SideBySideSplitIntroducesFacingInnerEdges) {
    const auto CHILDREN = splitTargetEdges({}, true);

    EXPECT_EQ(CHILDREN[0],
              (STargetEdges{
                  .right = eTargetEdge::INNER,
              }));
    EXPECT_EQ(CHILDREN[1],
              (STargetEdges{
                  .left = eTargetEdge::INNER,
              }));
}

TEST(TargetEdges, StackedSplitIntroducesFacingInnerEdges) {
    const auto CHILDREN = splitTargetEdges({}, false);

    EXPECT_EQ(CHILDREN[0],
              (STargetEdges{
                  .bottom = eTargetEdge::INNER,
              }));
    EXPECT_EQ(CHILDREN[1],
              (STargetEdges{
                  .top = eTargetEdge::INNER,
              }));
}

TEST(TargetEdges, NestedOrthogonalSplitsPreserveAllAncestorEdges) {
    const auto COLUMNS      = splitTargetEdges({}, true);
    const auto LEFT_COLUMN  = splitTargetEdges(COLUMNS[0], false);
    const auto RIGHT_COLUMN = splitTargetEdges(COLUMNS[1], false);

    EXPECT_EQ(LEFT_COLUMN[0],
              (STargetEdges{
                  .right  = eTargetEdge::INNER,
                  .bottom = eTargetEdge::INNER,
              }));
    EXPECT_EQ(LEFT_COLUMN[1],
              (STargetEdges{
                  .top   = eTargetEdge::INNER,
                  .right = eTargetEdge::INNER,
              }));
    EXPECT_EQ(RIGHT_COLUMN[0],
              (STargetEdges{
                  .bottom = eTargetEdge::INNER,
                  .left   = eTargetEdge::INNER,
              }));
    EXPECT_EQ(RIGHT_COLUMN[1],
              (STargetEdges{
                  .top  = eTargetEdge::INNER,
                  .left = eTargetEdge::INNER,
              }));

    const auto ROWS       = splitTargetEdges({}, false);
    const auto TOP_ROW    = splitTargetEdges(ROWS[0], true);
    const auto BOTTOM_ROW = splitTargetEdges(ROWS[1], true);

    EXPECT_EQ(TOP_ROW[0], LEFT_COLUMN[0]);
    EXPECT_EQ(TOP_ROW[1], RIGHT_COLUMN[0]);
    EXPECT_EQ(BOTTOM_ROW[0], LEFT_COLUMN[1]);
    EXPECT_EQ(BOTTOM_ROW[1], RIGHT_COLUMN[1]);
}

TEST(TargetEdges, RepeatedSplitsCanEncloseATileOnAllSides) {
    const auto COLUMNS       = splitTargetEdges({}, true);
    const auto RIGHT_COLUMNS = splitTargetEdges(COLUMNS[1], true);
    const auto MIDDLE_ROWS   = splitTargetEdges(RIGHT_COLUMNS[0], false);
    const auto BOTTOM_ROWS   = splitTargetEdges(MIDDLE_ROWS[1], false);

    EXPECT_EQ(RIGHT_COLUMNS[0],
              (STargetEdges{
                  .right = eTargetEdge::INNER,
                  .left  = eTargetEdge::INNER,
              }));
    EXPECT_EQ(RIGHT_COLUMNS[1],
              (STargetEdges{
                  .left = eTargetEdge::INNER,
              }));
    EXPECT_EQ(MIDDLE_ROWS[0],
              (STargetEdges{
                  .right  = eTargetEdge::INNER,
                  .bottom = eTargetEdge::INNER,
                  .left   = eTargetEdge::INNER,
              }));
    EXPECT_EQ(BOTTOM_ROWS[0],
              (STargetEdges{
                  .top    = eTargetEdge::INNER,
                  .right  = eTargetEdge::INNER,
                  .bottom = eTargetEdge::INNER,
                  .left   = eTargetEdge::INNER,
              }));
    EXPECT_EQ(BOTTOM_ROWS[1],
              (STargetEdges{
                  .top   = eTargetEdge::INNER,
                  .right = eTargetEdge::INNER,
                  .left  = eTargetEdge::INNER,
              }));
}

TEST(TargetEdges, RotatingSubtreeSplitReplacesOnlyItsOwnDivider) {
    const auto ROOT    = splitTargetEdges({}, true);
    const auto BEFORE  = splitTargetEdges(ROOT[1], true);
    const auto ROTATED = splitTargetEdges(ROOT[1], false);

    EXPECT_EQ(BEFORE[0],
              (STargetEdges{
                  .right = eTargetEdge::INNER,
                  .left  = eTargetEdge::INNER,
              }));
    EXPECT_EQ(ROTATED[0],
              (STargetEdges{
                  .bottom = eTargetEdge::INNER,
                  .left   = eTargetEdge::INNER,
              }));
    EXPECT_EQ(ROTATED[1],
              (STargetEdges{
                  .top  = eTargetEdge::INNER,
                  .left = eTargetEdge::INNER,
              }));
}
