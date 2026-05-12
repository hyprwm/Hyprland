#include <desktop/rule/matchEngine/IntMatchEngine.hpp>

#include <gtest/gtest.h>

using namespace Desktop::Rule;

TEST(IntMatchEngine, positiveInteger) {
    CIntMatchEngine engine("42");
    EXPECT_TRUE(engine.match(42));
    EXPECT_FALSE(engine.match(41));
    EXPECT_FALSE(engine.match(0));
}

TEST(IntMatchEngine, zero) {
    CIntMatchEngine engine("0");
    EXPECT_TRUE(engine.match(0));
    EXPECT_FALSE(engine.match(1));
}

TEST(IntMatchEngine, negativeInteger) {
    CIntMatchEngine engine("-5");
    EXPECT_TRUE(engine.match(-5));
    EXPECT_FALSE(engine.match(5));
}

TEST(IntMatchEngine, invalidStringDefaultsToZero) {
    CIntMatchEngine engine("abc");
    EXPECT_TRUE(engine.match(0));
    EXPECT_FALSE(engine.match(1));
}

TEST(IntMatchEngine, emptyStringDefaultsToZero) {
    CIntMatchEngine engine("");
    EXPECT_TRUE(engine.match(0));
    EXPECT_FALSE(engine.match(1));
}

TEST(IntMatchEngine, leadingWhitespace) {
    CIntMatchEngine engine(" 123");
    EXPECT_TRUE(engine.match(123));
}

TEST(IntMatchEngine, trailingNonDigits) {
    CIntMatchEngine engine("123abc");
    EXPECT_TRUE(engine.match(123));
}

TEST(IntMatchEngine, largeValue) {
    CIntMatchEngine engine("999999");
    EXPECT_TRUE(engine.match(999999));
    EXPECT_FALSE(engine.match(0));
}
