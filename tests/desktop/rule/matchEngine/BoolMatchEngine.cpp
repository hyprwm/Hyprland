#include <desktop/rule/matchEngine/BoolMatchEngine.hpp>

#include <gtest/gtest.h>

using namespace Desktop::Rule;

TEST(BoolMatchEngine, truthyStringOne) {
    CBoolMatchEngine engine("1");
    EXPECT_TRUE(engine.match(true));
    EXPECT_FALSE(engine.match(false));
}

TEST(BoolMatchEngine, truthyStringTrue) {
    CBoolMatchEngine engine("true");
    EXPECT_TRUE(engine.match(true));
    EXPECT_FALSE(engine.match(false));
}

TEST(BoolMatchEngine, truthyStringYes) {
    CBoolMatchEngine engine("yes");
    EXPECT_TRUE(engine.match(true));
    EXPECT_FALSE(engine.match(false));
}

TEST(BoolMatchEngine, truthyStringOn) {
    CBoolMatchEngine engine("on");
    EXPECT_TRUE(engine.match(true));
    EXPECT_FALSE(engine.match(false));
}

TEST(BoolMatchEngine, truthyCaseInsensitive) {
    CBoolMatchEngine upper("TRUE");
    EXPECT_TRUE(upper.match(true));

    CBoolMatchEngine mixed("Yes");
    EXPECT_TRUE(mixed.match(true));

    CBoolMatchEngine onUpper("ON");
    EXPECT_TRUE(onUpper.match(true));
}

TEST(BoolMatchEngine, falsyStringZero) {
    CBoolMatchEngine engine("0");
    EXPECT_TRUE(engine.match(false));
    EXPECT_FALSE(engine.match(true));
}

TEST(BoolMatchEngine, falsyStringFalse) {
    CBoolMatchEngine engine("false");
    EXPECT_TRUE(engine.match(false));
    EXPECT_FALSE(engine.match(true));
}

TEST(BoolMatchEngine, falsyStringNo) {
    CBoolMatchEngine engine("no");
    EXPECT_TRUE(engine.match(false));
    EXPECT_FALSE(engine.match(true));
}

TEST(BoolMatchEngine, falsyStringEmpty) {
    CBoolMatchEngine engine("");
    EXPECT_TRUE(engine.match(false));
    EXPECT_FALSE(engine.match(true));
}

TEST(BoolMatchEngine, falsyStringArbitrary) {
    CBoolMatchEngine engine("banana");
    EXPECT_TRUE(engine.match(false));
    EXPECT_FALSE(engine.match(true));
}
