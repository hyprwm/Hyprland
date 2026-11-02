#include <desktop/rule/windowRule/WindowRule.hpp>

#include <gtest/gtest.h>

using namespace Desktop::Rule;

TEST(WindowRuleRounding, storesRawCornerValuesWithoutLuaUpperBound) {
    CWindowRule rule;
    ASSERT_TRUE(rule.addEffect(WINDOW_RULE_EFFECT_ROUNDING, "101 202 303 404").has_value());
    ASSERT_EQ(rule.effects().size(), 1);
    const auto& effect = rule.effects().back();
    ASSERT_TRUE(std::holds_alternative<Config::CCornersData>(effect.value));
    EXPECT_EQ(std::get<Config::CCornersData>(effect.value).toString(), "101 202 303 404");

    for (const auto raw : {"-1", "1 2 -3 4", "1 2 3 nope", "9223372036854775808"}) {
        EXPECT_FALSE(rule.addEffect(WINDOW_RULE_EFFECT_ROUNDING, raw).has_value());
        EXPECT_EQ(rule.effects().size(), 1);
    }
}

TEST(WindowRuleRounding, scalarRuleExpandsAndPowerRemainsScalar) {
    CWindowRule rule;
    ASSERT_TRUE(rule.addEffect(WINDOW_RULE_EFFECT_ROUNDING, "7").has_value());
    const auto& corners = std::get<Config::CCornersData>(rule.effects().back().value);
    EXPECT_TRUE(corners.isUniform());
    EXPECT_EQ(corners.toString(), "7");
    ASSERT_TRUE(rule.addEffect(WINDOW_RULE_EFFECT_ROUNDING_POWER, "4").has_value());
    EXPECT_FLOAT_EQ(std::get<float>(rule.effects().back().value), 4.F);
}
