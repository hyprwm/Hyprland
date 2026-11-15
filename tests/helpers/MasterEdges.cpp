#include <layout/algorithm/tiled/master/MasterEdges.hpp>

#include <gtest/gtest.h>
#include <array>
#include <tuple>

using namespace Layout;
using namespace Layout::Tiled;

TEST(MasterEdges, MasterAndStackShareOnlyTheirFacingEdges) {
    for (const auto& [orientation, master, slave] : {
             std::tuple{ORIENTATION_LEFT, STargetEdges{.right = eTargetEdge::INNER}, STargetEdges{.left = eTargetEdge::INNER}},
             std::tuple{ORIENTATION_RIGHT, STargetEdges{.left = eTargetEdge::INNER}, STargetEdges{.right = eTargetEdge::INNER}},
             std::tuple{ORIENTATION_TOP, STargetEdges{.bottom = eTargetEdge::INNER}, STargetEdges{.top = eTargetEdge::INNER}},
             std::tuple{ORIENTATION_BOTTOM, STargetEdges{.top = eTargetEdge::INNER}, STargetEdges{.bottom = eTargetEdge::INNER}},
         }) {
        SCOPED_TRACE(orientation);
        EXPECT_EQ(masterTargetEdges(orientation, true, 0, 1, 1, false), master);
        EXPECT_EQ(masterTargetEdges(orientation, false, 0, 1, 1, false), slave);
    }
}

TEST(MasterEdges, SingletonIsExposedEvenWhenItsPositionIsKeptOrCentered) {
    // Exposure depends on occupied groups, not whether the allocation fills the work area.
    for (const auto orientation : {ORIENTATION_LEFT, ORIENTATION_RIGHT, ORIENTATION_TOP, ORIENTATION_BOTTOM, ORIENTATION_CENTER}) {
        for (const bool extraToRight : {false, true})
            EXPECT_EQ(masterTargetEdges(orientation, true, 0, 1, 0, extraToRight), STargetEdges{});
    }
}

TEST(MasterEdges, MultipleMastersWithoutSlavesExposeBothSidesOfTheirGroup) {
    for (const auto orientation : {ORIENTATION_LEFT, ORIENTATION_RIGHT, ORIENTATION_CENTER}) {
        EXPECT_EQ(masterTargetEdges(orientation, true, 0, 3, 0, false), (STargetEdges{.bottom = eTargetEdge::INNER}));
        EXPECT_EQ(masterTargetEdges(orientation, true, 1, 3, 0, false), (STargetEdges{.top = eTargetEdge::INNER, .bottom = eTargetEdge::INNER}));
        EXPECT_EQ(masterTargetEdges(orientation, true, 2, 3, 0, false), (STargetEdges{.top = eTargetEdge::INNER}));
    }

    for (const auto orientation : {ORIENTATION_TOP, ORIENTATION_BOTTOM}) {
        EXPECT_EQ(masterTargetEdges(orientation, true, 0, 3, 0, false), (STargetEdges{.right = eTargetEdge::INNER}));
        EXPECT_EQ(masterTargetEdges(orientation, true, 1, 3, 0, false), (STargetEdges{.right = eTargetEdge::INNER, .left = eTargetEdge::INNER}));
        EXPECT_EQ(masterTargetEdges(orientation, true, 2, 3, 0, false), (STargetEdges{.left = eTargetEdge::INNER}));
    }
}

TEST(MasterEdges, VerticalGroupsKeepIndependentFirstAndLastEdges) {
    EXPECT_EQ(masterTargetEdges(ORIENTATION_LEFT, true, 0, 2, 3, false), (STargetEdges{.right = eTargetEdge::INNER, .bottom = eTargetEdge::INNER}));
    EXPECT_EQ(masterTargetEdges(ORIENTATION_LEFT, true, 1, 2, 3, false), (STargetEdges{.top = eTargetEdge::INNER, .right = eTargetEdge::INNER}));
    EXPECT_EQ(masterTargetEdges(ORIENTATION_LEFT, false, 0, 2, 3, false), (STargetEdges{.bottom = eTargetEdge::INNER, .left = eTargetEdge::INNER}));
    EXPECT_EQ(masterTargetEdges(ORIENTATION_LEFT, false, 1, 2, 3, false), (STargetEdges{.top = eTargetEdge::INNER, .bottom = eTargetEdge::INNER, .left = eTargetEdge::INNER}));
    EXPECT_EQ(masterTargetEdges(ORIENTATION_LEFT, false, 2, 2, 3, false), (STargetEdges{.top = eTargetEdge::INNER, .left = eTargetEdge::INNER}));
}

TEST(MasterEdges, HorizontalGroupsKeepIndependentFirstAndLastEdges) {
    EXPECT_EQ(masterTargetEdges(ORIENTATION_TOP, true, 0, 3, 2, false), (STargetEdges{.right = eTargetEdge::INNER, .bottom = eTargetEdge::INNER}));
    EXPECT_EQ(masterTargetEdges(ORIENTATION_TOP, true, 1, 3, 2, false), (STargetEdges{.right = eTargetEdge::INNER, .bottom = eTargetEdge::INNER, .left = eTargetEdge::INNER}));
    EXPECT_EQ(masterTargetEdges(ORIENTATION_TOP, true, 2, 3, 2, false), (STargetEdges{.bottom = eTargetEdge::INNER, .left = eTargetEdge::INNER}));
    EXPECT_EQ(masterTargetEdges(ORIENTATION_TOP, false, 0, 3, 2, false), (STargetEdges{.top = eTargetEdge::INNER, .right = eTargetEdge::INNER}));
    EXPECT_EQ(masterTargetEdges(ORIENTATION_TOP, false, 1, 3, 2, false), (STargetEdges{.top = eTargetEdge::INNER, .left = eTargetEdge::INNER}));
}

TEST(MasterEdges, CenteredMasterExposesAnAbsentSideStack) {
    EXPECT_EQ(masterTargetEdges(ORIENTATION_CENTER, true, 0, 1, 1, false), (STargetEdges{.left = eTargetEdge::INNER}));
    EXPECT_EQ(masterTargetEdges(ORIENTATION_CENTER, false, 0, 1, 1, false), (STargetEdges{.right = eTargetEdge::INNER}));
    EXPECT_EQ(masterTargetEdges(ORIENTATION_CENTER, true, 0, 1, 1, true), (STargetEdges{.right = eTargetEdge::INNER}));
    EXPECT_EQ(masterTargetEdges(ORIENTATION_CENTER, false, 0, 1, 1, true), (STargetEdges{.left = eTargetEdge::INNER}));

    EXPECT_EQ(masterTargetEdges(ORIENTATION_CENTER, true, 0, 2, 1, false), (STargetEdges{.bottom = eTargetEdge::INNER, .left = eTargetEdge::INNER}));
    EXPECT_EQ(masterTargetEdges(ORIENTATION_CENTER, true, 1, 2, 1, false), (STargetEdges{.top = eTargetEdge::INNER, .left = eTargetEdge::INNER}));
    EXPECT_EQ(masterTargetEdges(ORIENTATION_CENTER, true, 0, 2, 1, true), (STargetEdges{.right = eTargetEdge::INNER, .bottom = eTargetEdge::INNER}));
    EXPECT_EQ(masterTargetEdges(ORIENTATION_CENTER, true, 1, 2, 1, true), (STargetEdges{.top = eTargetEdge::INNER, .right = eTargetEdge::INNER}));
}

TEST(MasterEdges, CenteredColumnsAlternateFromTheFallbackSide) {
    const STargetEdges LEFT_FIRST   = {.right = eTargetEdge::INNER, .bottom = eTargetEdge::INNER};
    const STargetEdges LEFT_MIDDLE  = {.top = eTargetEdge::INNER, .right = eTargetEdge::INNER, .bottom = eTargetEdge::INNER};
    const STargetEdges LEFT_LAST    = {.top = eTargetEdge::INNER, .right = eTargetEdge::INNER};
    const STargetEdges RIGHT_FIRST  = {.bottom = eTargetEdge::INNER, .left = eTargetEdge::INNER};
    const STargetEdges RIGHT_MIDDLE = {.top = eTargetEdge::INNER, .bottom = eTargetEdge::INNER, .left = eTargetEdge::INNER};
    const STargetEdges RIGHT_LAST   = {.top = eTargetEdge::INNER, .left = eTargetEdge::INNER};

    for (const bool extraToRight : {false, true}) {
        SCOPED_TRACE(extraToRight);
        // The same topology applies when center_ignores_reserved shifts and resizes the columns.
        EXPECT_EQ(masterTargetEdges(ORIENTATION_CENTER, true, 0, 2, 5, extraToRight),
                  (STargetEdges{.right = eTargetEdge::INNER, .bottom = eTargetEdge::INNER, .left = eTargetEdge::INNER}));
        EXPECT_EQ(masterTargetEdges(ORIENTATION_CENTER, true, 1, 2, 5, extraToRight),
                  (STargetEdges{.top = eTargetEdge::INNER, .right = eTargetEdge::INNER, .left = eTargetEdge::INNER}));

        const auto ODD =
            extraToRight ? std::array{RIGHT_FIRST, LEFT_FIRST, RIGHT_MIDDLE, LEFT_LAST, RIGHT_LAST} : std::array{LEFT_FIRST, RIGHT_FIRST, LEFT_MIDDLE, RIGHT_LAST, LEFT_LAST};
        for (int i = 0; i < 5; ++i)
            EXPECT_EQ(masterTargetEdges(ORIENTATION_CENTER, false, i, 2, 5, extraToRight), ODD[i]);

        const auto EVEN = extraToRight ? std::array{RIGHT_FIRST, LEFT_FIRST, RIGHT_LAST, LEFT_LAST} : std::array{LEFT_FIRST, RIGHT_FIRST, LEFT_LAST, RIGHT_LAST};
        for (int i = 0; i < 4; ++i)
            EXPECT_EQ(masterTargetEdges(ORIENTATION_CENTER, false, i, 2, 4, extraToRight), EVEN[i]);
    }
}
