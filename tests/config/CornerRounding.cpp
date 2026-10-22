#include <desktop/rule/windowRule/WindowRule.hpp>
#include <gtest/gtest.h>

using namespace Desktop::Rule;

TEST(CornerRounding, RulesAcceptInheritanceSquareAndPositiveRadii) {
    for (const auto effect :
         {WINDOW_RULE_EFFECT_ROUNDING_TOP_LEFT, WINDOW_RULE_EFFECT_ROUNDING_TOP_RIGHT, WINDOW_RULE_EFFECT_ROUNDING_BOTTOM_RIGHT, WINDOW_RULE_EFFECT_ROUNDING_BOTTOM_LEFT}) {
        for (const auto value : {-1, 0, 24}) {
            CWindowRule rule;
            ASSERT_TRUE(rule.addEffect(effect, std::to_string(value)));
            ASSERT_EQ(rule.effects().size(), 1);
            EXPECT_EQ(std::get<int64_t>(rule.effects().front().value), value);
        }
        CWindowRule invalid;
        EXPECT_FALSE(invalid.addEffect(effect, "-2"));
        EXPECT_FALSE(invalid.addEffect(effect, "not a radius"));
    }
}

TEST(CornerRounding, LegacyRoundingStillRejectsInheritanceSentinel) {
    CWindowRule rule;
    EXPECT_FALSE(rule.addEffect(WINDOW_RULE_EFFECT_ROUNDING, "-1"));
    EXPECT_TRUE(rule.addEffect(WINDOW_RULE_EFFECT_ROUNDING, "0"));
}
