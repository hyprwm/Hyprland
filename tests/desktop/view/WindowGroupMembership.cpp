#include <desktop/view/window/WindowGroupMembership.hpp>

#include <gtest/gtest.h>

using namespace Desktop::View;

TEST(WindowGroupMembership, PreservesNumericRuleFlags) {
    EXPECT_EQ(GROUP_NONE, 0);
    EXPECT_EQ(GROUP_SET, 1);
    EXPECT_EQ(GROUP_SET_ALWAYS, 2);
    EXPECT_EQ(GROUP_BARRED, 4);
    EXPECT_EQ(GROUP_LOCK, 8);
    EXPECT_EQ(GROUP_LOCK_ALWAYS, 16);
    EXPECT_EQ(GROUP_INVADE, 32);
    EXPECT_EQ(GROUP_OVERRIDE, 64);
    EXPECT_EQ(GROUP_DENY, 128);
}

TEST(WindowGroupMembership, ParsesAndAccumulatesRules) {
    auto rules = parseGroupRules("group set always");
    rules      = parseGroupRules("group lock always invade barred deny", rules);

    EXPECT_EQ(rules, GROUP_SET | GROUP_SET_ALWAYS | GROUP_BARRED | GROUP_LOCK | GROUP_LOCK_ALWAYS | GROUP_INVADE | GROUP_DENY);
    EXPECT_EQ(parseGroupRules("group new"), GROUP_SET | GROUP_BARRED);
}

TEST(WindowGroupMembership, OverrideAndUnsetMakeRulesSticky) {
    EXPECT_EQ(parseGroupRules("group override lock", GROUP_SET | GROUP_DENY), GROUP_OVERRIDE | GROUP_LOCK);

    const auto unset = parseGroupRules("group unset lock", GROUP_SET | GROUP_DENY);
    EXPECT_EQ(unset, GROUP_OVERRIDE);
    EXPECT_EQ(parseGroupRules("group set lock", unset), GROUP_OVERRIDE);
}

TEST(WindowGroupMembership, BareGroupDoesNotChangeRules) {
    EXPECT_EQ(parseGroupRules(" group ", GROUP_DENY), GROUP_DENY);
}
