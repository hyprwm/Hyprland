#include <helpers/math/Direction.hpp>

#include <gtest/gtest.h>

using namespace Math;

TEST(Helpers, directionFromCharValid) {
    EXPECT_EQ(fromChar('r'), DIRECTION_RIGHT);
    EXPECT_EQ(fromChar('l'), DIRECTION_LEFT);
    EXPECT_EQ(fromChar('u'), DIRECTION_UP);
    EXPECT_EQ(fromChar('d'), DIRECTION_DOWN);
    EXPECT_EQ(fromChar('t'), DIRECTION_UP);
    EXPECT_EQ(fromChar('b'), DIRECTION_DOWN);
}

TEST(Helpers, directionFromCharInvalid) {
    EXPECT_EQ(fromChar('x'), DIRECTION_DEFAULT);
    EXPECT_EQ(fromChar('z'), DIRECTION_DEFAULT);
    EXPECT_EQ(fromChar('0'), DIRECTION_DEFAULT);
    EXPECT_EQ(fromChar(' '), DIRECTION_DEFAULT);
    EXPECT_EQ(fromChar('\0'), DIRECTION_DEFAULT);
}

TEST(Helpers, directionToString) {
    EXPECT_STREQ(toString(DIRECTION_UP), "up");
    EXPECT_STREQ(toString(DIRECTION_DOWN), "down");
    EXPECT_STREQ(toString(DIRECTION_LEFT), "left");
    EXPECT_STREQ(toString(DIRECTION_RIGHT), "right");
    EXPECT_STREQ(toString(DIRECTION_DEFAULT), "default");
}

TEST(Helpers, directionFromCharToString) {
    EXPECT_STREQ(toString(fromChar('r')), "right");
    EXPECT_STREQ(toString(fromChar('l')), "left");
    EXPECT_STREQ(toString(fromChar('u')), "up");
    EXPECT_STREQ(toString(fromChar('d')), "down");
    EXPECT_STREQ(toString(fromChar('t')), "up");
    EXPECT_STREQ(toString(fromChar('b')), "down");
    EXPECT_STREQ(toString(fromChar('x')), "default");
}
