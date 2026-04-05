#include <helpers/TagKeeper.hpp>

#include <gtest/gtest.h>

// --- applyTag: set with + prefix ---

TEST(TagKeeper, applyTagSetAddsTag) {
    CTagKeeper keeper;
    EXPECT_TRUE(keeper.applyTag("+myTag"));
    EXPECT_TRUE(keeper.isTagged("myTag"));
}

TEST(TagKeeper, applyTagSetReturnsFalseIfAlreadySet) {
    CTagKeeper keeper;
    keeper.applyTag("+myTag");
    EXPECT_FALSE(keeper.applyTag("+myTag"));
}

// --- applyTag: unset with - prefix ---

TEST(TagKeeper, applyTagUnsetRemovesTag) {
    CTagKeeper keeper;
    keeper.applyTag("+myTag");
    EXPECT_TRUE(keeper.applyTag("-myTag"));
    EXPECT_FALSE(keeper.isTagged("myTag"));
}

TEST(TagKeeper, applyTagUnsetReturnsFalseIfNotSet) {
    CTagKeeper keeper;
    EXPECT_FALSE(keeper.applyTag("-myTag"));
}

// --- applyTag: toggle without prefix ---

TEST(TagKeeper, applyTagToggleSetsWhenAbsent) {
    CTagKeeper keeper;
    EXPECT_TRUE(keeper.applyTag("myTag"));
    EXPECT_TRUE(keeper.isTagged("myTag"));
}

TEST(TagKeeper, applyTagToggleUnsetsWhenPresent) {
    CTagKeeper keeper;
    keeper.applyTag("+myTag");
    EXPECT_TRUE(keeper.applyTag("myTag"));
    EXPECT_FALSE(keeper.isTagged("myTag"));
}

// --- applyTag: dynamic tags ---

TEST(TagKeeper, applyTagDynamicAppendsStar) {
    CTagKeeper keeper;
    keeper.applyTag("myTag", true);
    EXPECT_TRUE(keeper.getTags().contains("myTag*"));
    EXPECT_FALSE(keeper.getTags().contains("myTag"));
}

TEST(TagKeeper, applyTagDynamicDoesNotDoubleAppendStar) {
    CTagKeeper keeper;
    keeper.applyTag("myTag*", true);
    EXPECT_TRUE(keeper.getTags().contains("myTag*"));
    EXPECT_FALSE(keeper.getTags().contains("myTag**"));
}

// --- isTagged: basic matching ---

TEST(TagKeeper, isTaggedReturnsFalseWhenEmpty) {
    CTagKeeper keeper;
    EXPECT_FALSE(keeper.isTagged("myTag"));
}

TEST(TagKeeper, isTaggedExactMatch) {
    CTagKeeper keeper;
    keeper.applyTag("+myTag");
    EXPECT_TRUE(keeper.isTagged("myTag"));
    EXPECT_FALSE(keeper.isTagged("otherTag"));
}

// --- isTagged: dynamic (star) matching ---

TEST(TagKeeper, isTaggedMatchesDynamicWhenNotStrict) {
    CTagKeeper keeper;
    keeper.applyTag("myTag", true); // stores "myTag*"
    EXPECT_TRUE(keeper.isTagged("myTag"));
}

TEST(TagKeeper, isTaggedStrictDoesNotMatchDynamic) {
    CTagKeeper keeper;
    keeper.applyTag("myTag", true); // stores "myTag*"
    EXPECT_FALSE(keeper.isTagged("myTag", true));
}

TEST(TagKeeper, isTaggedStrictMatchesExact) {
    CTagKeeper keeper;
    keeper.applyTag("+myTag");
    EXPECT_TRUE(keeper.isTagged("myTag", true));
}

// --- isTagged: negative prefix ---

TEST(TagKeeper, isTaggedNegativeInvertsResult) {
    CTagKeeper keeper;
    EXPECT_TRUE(keeper.isTagged("negative:myTag"));

    keeper.applyTag("+myTag");
    EXPECT_FALSE(keeper.isTagged("negative:myTag"));
}

TEST(TagKeeper, isTaggedNegativeWithDynamic) {
    CTagKeeper keeper;
    keeper.applyTag("myTag", true); // stores "myTag*"
    EXPECT_FALSE(keeper.isTagged("negative:myTag"));
}

// --- removeDynamicTag ---

TEST(TagKeeper, removeDynamicTagRemovesStarVariant) {
    CTagKeeper keeper;
    keeper.applyTag("myTag", true); // stores "myTag*"
    EXPECT_TRUE(keeper.removeDynamicTag("myTag"));
    EXPECT_FALSE(keeper.isTagged("myTag"));
}

TEST(TagKeeper, removeDynamicTagReturnsFalseIfNoDynamic) {
    CTagKeeper keeper;
    keeper.applyTag("+myTag"); // stores "myTag" (not dynamic)
    EXPECT_FALSE(keeper.removeDynamicTag("myTag"));
    EXPECT_TRUE(keeper.isTagged("myTag")); // static tag untouched
}

TEST(TagKeeper, removeDynamicTagOnEmpty) {
    CTagKeeper keeper;
    EXPECT_FALSE(keeper.removeDynamicTag("myTag"));
}

// --- getTags ---

TEST(TagKeeper, getTagsReturnsAllStoredTags) {
    CTagKeeper keeper;
    keeper.applyTag("+a");
    keeper.applyTag("+b");
    keeper.applyTag("c", true);

    const auto& tags = keeper.getTags();
    EXPECT_EQ(tags.size(), 3u);
    EXPECT_TRUE(tags.contains("a"));
    EXPECT_TRUE(tags.contains("b"));
    EXPECT_TRUE(tags.contains("c*"));
}
