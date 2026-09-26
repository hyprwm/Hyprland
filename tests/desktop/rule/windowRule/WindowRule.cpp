#include <desktop/rule/windowRule/WindowRule.hpp>

#include <gtest/gtest.h>

using namespace Desktop::Rule;

TEST(WindowRule, parsesWorkspaceBlurEffect) {
    CWindowRule rule;

    ASSERT_TRUE(rule.addEffect(WINDOW_RULE_EFFECT_WORKSPACE_BLUR, "false"));
    ASSERT_EQ(rule.effects().size(), 1);
    EXPECT_FALSE(std::get<bool>(rule.effects().front().value));

    ASSERT_TRUE(rule.addEffect(WINDOW_RULE_EFFECT_WORKSPACE_BLUR, "true"));
    ASSERT_EQ(rule.effects().size(), 2);
    EXPECT_TRUE(std::get<bool>(rule.effects().back().value));
}
