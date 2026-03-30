#include <desktop/rule/matchEngine/TagMatchEngine.hpp>
#include <helpers/TagKeeper.hpp>

#include <gtest/gtest.h>

using namespace Desktop::Rule;

TEST(TagMatchEngine, matchesExistingTag) {
    CTagKeeper keeper;
    keeper.applyTag("myTag");

    CTagMatchEngine engine("myTag");
    EXPECT_TRUE(engine.match(keeper));
}

TEST(TagMatchEngine, doesNotMatchMissingTag) {
    CTagKeeper keeper;
    keeper.applyTag("otherTag");

    CTagMatchEngine engine("myTag");
    EXPECT_FALSE(engine.match(keeper));
}

TEST(TagMatchEngine, emptyKeeper) {
    CTagKeeper      keeper;

    CTagMatchEngine engine("myTag");
    EXPECT_FALSE(engine.match(keeper));
}

TEST(TagMatchEngine, negativeTagMatching) {
    CTagKeeper keeper;
    keeper.applyTag("myTag");

    CTagMatchEngine negative("negative:myTag");
    EXPECT_FALSE(negative.match(keeper));

    CTagMatchEngine negativeOther("negative:otherTag");
    EXPECT_TRUE(negativeOther.match(keeper));
}

TEST(TagMatchEngine, dynamicTagMatchesWithoutStrict) {
    CTagKeeper keeper;
    keeper.applyTag("myTag", true); // dynamic adds "*" suffix

    CTagMatchEngine engine("myTag");
    EXPECT_TRUE(engine.match(keeper));
}

TEST(TagMatchEngine, caseSensitive) {
    CTagKeeper keeper;
    keeper.applyTag("MyTag");

    CTagMatchEngine lower("mytag");
    EXPECT_FALSE(lower.match(keeper));

    CTagMatchEngine exact("MyTag");
    EXPECT_TRUE(exact.match(keeper));
}

TEST(TagMatchEngine, tagRemoval) {
    CTagKeeper keeper;
    keeper.applyTag("myTag");
    keeper.applyTag("-myTag");

    CTagMatchEngine engine("myTag");
    EXPECT_FALSE(engine.match(keeper));
}
