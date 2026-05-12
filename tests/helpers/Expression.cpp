#include <helpers/math/Expression.hpp>

#include <gtest/gtest.h>

using namespace Math;

TEST(Helpers, expressionBasicArithmetic) {
    CExpression expr;
    EXPECT_DOUBLE_EQ(expr.compute("2 + 3").value(), 5.0);
    EXPECT_DOUBLE_EQ(expr.compute("10 - 4").value(), 6.0);
    EXPECT_DOUBLE_EQ(expr.compute("3 * 7").value(), 21.0);
    EXPECT_DOUBLE_EQ(expr.compute("10 / 4").value(), 2.5);
}

TEST(Helpers, expressionOrderOfOperations) {
    CExpression expr;
    EXPECT_DOUBLE_EQ(expr.compute("2 + 3 * 4").value(), 14.0);
    EXPECT_DOUBLE_EQ(expr.compute("(2 + 3) * 4").value(), 20.0);
    EXPECT_DOUBLE_EQ(expr.compute("10 - 2 * 3").value(), 4.0);
    EXPECT_DOUBLE_EQ(expr.compute("(10 - 2) * 3").value(), 24.0);
}

TEST(Helpers, expressionNegativeNumbers) {
    CExpression expr;
    EXPECT_DOUBLE_EQ(expr.compute("-5 + 3").value(), -2.0);
    EXPECT_DOUBLE_EQ(expr.compute("-5 * -3").value(), 15.0);
    EXPECT_DOUBLE_EQ(expr.compute("0 - 10").value(), -10.0);
}

TEST(Helpers, expressionFloatingPoint) {
    CExpression expr;
    EXPECT_DOUBLE_EQ(expr.compute("1.5 + 2.5").value(), 4.0);
    EXPECT_DOUBLE_EQ(expr.compute("0.1 * 10").value(), 1.0);
    EXPECT_NEAR(expr.compute("1 / 3").value(), 0.3333, 0.001);
}

TEST(Helpers, expressionWithVariables) {
    CExpression expr;
    expr.addVariable("x", 10);
    EXPECT_DOUBLE_EQ(expr.compute("x * 2").value(), 20.0);
    EXPECT_DOUBLE_EQ(expr.compute("x + 5").value(), 15.0);
    EXPECT_DOUBLE_EQ(expr.compute("x * x").value(), 100.0);
}

TEST(Helpers, expressionMultipleVariables) {
    CExpression expr;
    expr.addVariable("w", 1920);
    expr.addVariable("h", 1080);
    EXPECT_DOUBLE_EQ(expr.compute("w / 2").value(), 960.0);
    EXPECT_DOUBLE_EQ(expr.compute("w + h").value(), 3000.0);
    EXPECT_DOUBLE_EQ(expr.compute("w * h").value(), 1920.0 * 1080.0);
}

TEST(Helpers, expressionInvalidInput) {
    CExpression expr;
    EXPECT_EQ(expr.compute(""), std::nullopt);
    EXPECT_EQ(expr.compute("2 +"), std::nullopt);
    EXPECT_EQ(expr.compute("abc"), std::nullopt);
    EXPECT_EQ(expr.compute("* 5"), std::nullopt);
}

TEST(Helpers, expressionZero) {
    CExpression expr;
    EXPECT_DOUBLE_EQ(expr.compute("0").value(), 0.0);
    EXPECT_DOUBLE_EQ(expr.compute("0 * 999").value(), 0.0);
    EXPECT_DOUBLE_EQ(expr.compute("5 - 5").value(), 0.0);
}

TEST(Helpers, expressionVec2ParsesLegacyString) {
    auto parsed = parseExpressionVec2("monitor_w*0.5 monitor_h*0.25");
    ASSERT_TRUE(parsed.has_value()) << parsed.error();

    EXPECT_EQ(parsed->x, "monitor_w*0.5");
    EXPECT_EQ(parsed->y, "monitor_h*0.25");
    EXPECT_EQ(parsed->toString(), "monitor_w*0.5 monitor_h*0.25");

    EXPECT_FALSE(parseExpressionVec2("monitor_w*0.5").has_value());
}
