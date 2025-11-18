
#include <desktop/reserved/ReservedArea.hpp>

#include <gtest/gtest.h>

TEST(Desktop, reservedArea) {
    Desktop::CReservedArea a{{20, 30}, {40, 50}};
    CBox                   box = {1000, 1000, 1000, 1000};
    a.applyip(box);

    EXPECT_EQ(box.x, 1020);
    EXPECT_EQ(box.y, 1030);
    EXPECT_EQ(box.w, 1000 - 20 - 40);
    EXPECT_EQ(box.h, 1000 - 30 - 50);

    box = a.apply(CBox{1000, 1000, 1000, 1000});

    EXPECT_EQ(box.x, 1020);
    EXPECT_EQ(box.y, 1030);
    EXPECT_EQ(box.w, 1000 - 20 - 40);
    EXPECT_EQ(box.h, 1000 - 30 - 50);

    a.addType(Desktop::RESERVED_DYNAMIC_TYPE_LS, {10, 20}, {30, 40});

    box = a.apply(CBox{1000, 1000, 1000, 1000});

    EXPECT_EQ(box.x, 1000 + 20 + 10);
    EXPECT_EQ(box.y, 1000 + 30 + 20);
    EXPECT_EQ(box.w, 1000 - 20 - 40 - 10 - 30);
    EXPECT_EQ(box.h, 1000 - 30 - 50 - 20 - 40);

    Desktop::CReservedArea b{CBox{10, 10, 1000, 1000}, CBox{20, 30, 900, 900}};

    EXPECT_EQ(b.left(), 20 - 10);
    EXPECT_EQ(b.top(), 30 - 10);
    EXPECT_EQ(b.right(), 1010 - 920);
    EXPECT_EQ(b.bottom(), 1010 - 930);
}