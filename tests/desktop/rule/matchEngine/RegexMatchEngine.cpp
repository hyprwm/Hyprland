#include <desktop/rule/matchEngine/RegexMatchEngine.hpp>

#include <gtest/gtest.h>

using namespace Desktop::Rule;

TEST(RegexMatchEngine, exactMatch) {
    CRegexMatchEngine engine("firefox");
    EXPECT_TRUE(engine.match("firefox"));
    EXPECT_FALSE(engine.match("Firefox"));
    EXPECT_FALSE(engine.match("firefox2"));
}

TEST(RegexMatchEngine, wildcardPattern) {
    CRegexMatchEngine engine("fire.*");
    EXPECT_TRUE(engine.match("firefox"));
    EXPECT_TRUE(engine.match("firewall"));
    EXPECT_FALSE(engine.match("ice"));
}

TEST(RegexMatchEngine, fullMatchRequired) {
    CRegexMatchEngine engine("fire");
    EXPECT_TRUE(engine.match("fire"));
    EXPECT_FALSE(engine.match("firefox"));
    EXPECT_FALSE(engine.match("campfire"));
}

TEST(RegexMatchEngine, characterClass) {
    CRegexMatchEngine engine("kitty_[ABC]");
    EXPECT_TRUE(engine.match("kitty_A"));
    EXPECT_TRUE(engine.match("kitty_B"));
    EXPECT_TRUE(engine.match("kitty_C"));
    EXPECT_FALSE(engine.match("kitty_D"));
}

TEST(RegexMatchEngine, negativePrefix) {
    CRegexMatchEngine engine("negative:firefox");
    EXPECT_FALSE(engine.match("firefox"));
    EXPECT_TRUE(engine.match("chromium"));
    EXPECT_TRUE(engine.match("anything"));
}

TEST(RegexMatchEngine, negativeWithWildcard) {
    CRegexMatchEngine engine("negative:.*\\.tmp");
    EXPECT_FALSE(engine.match("file.tmp"));
    EXPECT_TRUE(engine.match("file.txt"));
    EXPECT_TRUE(engine.match("file.cpp"));
}

TEST(RegexMatchEngine, emptyPattern) {
    CRegexMatchEngine engine("");
    EXPECT_TRUE(engine.match(""));
    EXPECT_FALSE(engine.match("anything"));
}

TEST(RegexMatchEngine, dotMatchesSingleChar) {
    CRegexMatchEngine engine("a.c");
    EXPECT_TRUE(engine.match("abc"));
    EXPECT_TRUE(engine.match("axc"));
    EXPECT_FALSE(engine.match("ac"));
    EXPECT_FALSE(engine.match("abbc"));
}

TEST(RegexMatchEngine, alternation) {
    CRegexMatchEngine engine("cat|dog");
    EXPECT_TRUE(engine.match("cat"));
    EXPECT_TRUE(engine.match("dog"));
    EXPECT_FALSE(engine.match("bird"));
}
