#include <helpers/Config.hpp>

#include <gtest/gtest.h>

// isDirection

TEST(Helpers, isDirectionString) {
    EXPECT_TRUE(isDirection("l"));
    EXPECT_TRUE(isDirection("r"));
    EXPECT_TRUE(isDirection("u"));
    EXPECT_TRUE(isDirection("d"));
    EXPECT_TRUE(isDirection("t"));
    EXPECT_TRUE(isDirection("b"));
    EXPECT_FALSE(isDirection("x"));
    EXPECT_FALSE(isDirection("left"));
    EXPECT_FALSE(isDirection(""));
}

TEST(Helpers, isDirectionChar) {
    EXPECT_TRUE(isDirection('l'));
    EXPECT_TRUE(isDirection('r'));
    EXPECT_TRUE(isDirection('u'));
    EXPECT_TRUE(isDirection('d'));
    EXPECT_TRUE(isDirection('t'));
    EXPECT_TRUE(isDirection('b'));
    EXPECT_FALSE(isDirection('x'));
    EXPECT_FALSE(isDirection('0'));
    EXPECT_FALSE(isDirection(' '));
}

// truthy

TEST(Helpers, truthyTrue) {
    EXPECT_TRUE(truthy("1"));
    EXPECT_TRUE(truthy("true"));
    EXPECT_TRUE(truthy("True"));
    EXPECT_TRUE(truthy("TRUE"));
    EXPECT_TRUE(truthy("yes"));
    EXPECT_TRUE(truthy("Yes"));
    EXPECT_TRUE(truthy("on"));
    EXPECT_TRUE(truthy("On"));
}

TEST(Helpers, truthyFalse) {
    EXPECT_FALSE(truthy("0"));
    EXPECT_FALSE(truthy("false"));
    EXPECT_FALSE(truthy("no"));
    EXPECT_FALSE(truthy("off"));
    EXPECT_FALSE(truthy(""));
    EXPECT_FALSE(truthy("random"));
}
