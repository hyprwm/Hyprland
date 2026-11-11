#include <layout/algorithm/tiled/scrolling/ScrollingEdges.hpp>

#include <gtest/gtest.h>
#include <array>

using namespace Layout;
using namespace Layout::Tiled;
using enum eTargetEdge;

static void expectEdges(const STargetEdges& edges, eTargetEdge top, eTargetEdge right, eTargetEdge bottom, eTargetEdge left) {
    EXPECT_EQ(edges.top, top);
    EXPECT_EQ(edges.right, right);
    EXPECT_EQ(edges.bottom, bottom);
    EXPECT_EQ(edges.left, left);
}

TEST(ScrollingEdges, SingleTargetHasOnlyOuterEdges) {
    for (const bool horizontal : {false, true}) {
        for (const bool reversed : {false, true}) {
            EXPECT_EQ(scrollingTargetEdges(0, 1, 0, 1, horizontal, reversed), STargetEdges{});
        }
    }
}

TEST(ScrollingEdges, HorizontalTapeUsesColumnAndRowTopology) {
    const std::array<size_t, 3> COLUMNS = {
        2,
        1,
        3,
    };

    expectEdges(scrollingTargetEdges(0, COLUMNS.size(), 0, COLUMNS[0], true, false), OUTER, INNER, INNER, OUTER);
    expectEdges(scrollingTargetEdges(0, COLUMNS.size(), 1, COLUMNS[0], true, false), INNER, INNER, OUTER, OUTER);
    expectEdges(scrollingTargetEdges(1, COLUMNS.size(), 0, COLUMNS[1], true, false), OUTER, INNER, OUTER, INNER);
    expectEdges(scrollingTargetEdges(2, COLUMNS.size(), 1, COLUMNS[2], true, false), INNER, OUTER, INNER, INNER);
    expectEdges(scrollingTargetEdges(2, COLUMNS.size(), 2, COLUMNS[2], true, false), INNER, OUTER, OUTER, INNER);
}

TEST(ScrollingEdges, LeftwardTapeReversesColumnsButNotRows) {
    const std::array<size_t, 3> COLUMNS = {
        2,
        1,
        3,
    };

    expectEdges(scrollingTargetEdges(0, COLUMNS.size(), 0, COLUMNS[0], true, true), OUTER, OUTER, INNER, INNER);
    expectEdges(scrollingTargetEdges(0, COLUMNS.size(), 1, COLUMNS[0], true, true), INNER, OUTER, OUTER, INNER);
    expectEdges(scrollingTargetEdges(1, COLUMNS.size(), 0, COLUMNS[1], true, true), OUTER, INNER, OUTER, INNER);
    expectEdges(scrollingTargetEdges(2, COLUMNS.size(), 0, COLUMNS[2], true, true), OUTER, INNER, INNER, OUTER);
    expectEdges(scrollingTargetEdges(2, COLUMNS.size(), 2, COLUMNS[2], true, true), INNER, INNER, OUTER, OUTER);
}

TEST(ScrollingEdges, DownwardTapeRotatesTheTopology) {
    const std::array<size_t, 3> COLUMNS = {
        2,
        1,
        3,
    };

    expectEdges(scrollingTargetEdges(0, COLUMNS.size(), 0, COLUMNS[0], false, false), OUTER, INNER, INNER, OUTER);
    expectEdges(scrollingTargetEdges(0, COLUMNS.size(), 1, COLUMNS[0], false, false), OUTER, OUTER, INNER, INNER);
    expectEdges(scrollingTargetEdges(1, COLUMNS.size(), 0, COLUMNS[1], false, false), INNER, OUTER, INNER, OUTER);
    expectEdges(scrollingTargetEdges(2, COLUMNS.size(), 1, COLUMNS[2], false, false), INNER, INNER, OUTER, INNER);
    expectEdges(scrollingTargetEdges(2, COLUMNS.size(), 2, COLUMNS[2], false, false), INNER, OUTER, OUTER, INNER);
}

TEST(ScrollingEdges, UpwardTapeReversesColumnsButNotRows) {
    const std::array<size_t, 3> COLUMNS = {
        2,
        1,
        3,
    };

    expectEdges(scrollingTargetEdges(0, COLUMNS.size(), 0, COLUMNS[0], false, true), INNER, INNER, OUTER, OUTER);
    expectEdges(scrollingTargetEdges(0, COLUMNS.size(), 1, COLUMNS[0], false, true), INNER, OUTER, OUTER, INNER);
    expectEdges(scrollingTargetEdges(1, COLUMNS.size(), 0, COLUMNS[1], false, true), INNER, OUTER, INNER, OUTER);
    expectEdges(scrollingTargetEdges(2, COLUMNS.size(), 0, COLUMNS[2], false, true), OUTER, INNER, INNER, OUTER);
    expectEdges(scrollingTargetEdges(2, COLUMNS.size(), 2, COLUMNS[2], false, true), OUTER, OUTER, INNER, INNER);
}

TEST(ScrollingEdges, SingleColumnStillHasInternalRowEdges) {
    for (const bool reversed : {false, true}) {
        expectEdges(scrollingTargetEdges(0, 1, 0, 3, true, reversed), OUTER, OUTER, INNER, OUTER);
        expectEdges(scrollingTargetEdges(0, 1, 1, 3, true, reversed), INNER, OUTER, INNER, OUTER);
        expectEdges(scrollingTargetEdges(0, 1, 2, 3, true, reversed), INNER, OUTER, OUTER, OUTER);

        expectEdges(scrollingTargetEdges(0, 1, 0, 3, false, reversed), OUTER, INNER, OUTER, OUTER);
        expectEdges(scrollingTargetEdges(0, 1, 1, 3, false, reversed), OUTER, INNER, OUTER, INNER);
        expectEdges(scrollingTargetEdges(0, 1, 2, 3, false, reversed), OUTER, OUTER, OUTER, INNER);
    }
}

TEST(ScrollingEdges, MissingTargetsHaveOnlyOuterEdges) {
    EXPECT_EQ(scrollingTargetEdges(0, 0, 0, 0, true, false), STargetEdges{});
    EXPECT_EQ(scrollingTargetEdges(2, 2, 0, 1, true, false), STargetEdges{});
    EXPECT_EQ(scrollingTargetEdges(0, 2, 0, 0, true, false), STargetEdges{});
    EXPECT_EQ(scrollingTargetEdges(1, 2, 2, 2, true, false), STargetEdges{});
}
