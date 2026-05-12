
#include <config/shared/parserUtils/ParserUtils.hpp>

#include <gtest/gtest.h>

using namespace Config;

TEST(ParserUtils, parseIntDecimal) {
    EXPECT_EQ(ParserUtils::parseInt("42").value(), 42);
    EXPECT_EQ(ParserUtils::parseInt("0").value(), 0);
    EXPECT_EQ(ParserUtils::parseInt("-1").value(), -1);
}

TEST(ParserUtils, parseIntHex) {
    EXPECT_EQ(ParserUtils::parseInt("0xFF").value(), 255);
    EXPECT_EQ(ParserUtils::parseInt("0x00").value(), 0);
    EXPECT_EQ(ParserUtils::parseInt("0x10").value(), 16);
}

TEST(ParserUtils, parseIntBooleanStrings) {
    // "true", "yes", "on" -> 1; "false", "no", "off" -> 0
    EXPECT_EQ(ParserUtils::parseInt("true").value(), 1);
    EXPECT_EQ(ParserUtils::parseInt("yes").value(), 1);
    EXPECT_EQ(ParserUtils::parseInt("on").value(), 1);
    EXPECT_EQ(ParserUtils::parseInt("false").value(), 0);
    EXPECT_EQ(ParserUtils::parseInt("no").value(), 0);
    EXPECT_EQ(ParserUtils::parseInt("off").value(), 0);
}

TEST(ParserUtils, parseIntInvalid) {
    EXPECT_FALSE(ParserUtils::parseInt("").has_value());
    EXPECT_FALSE(ParserUtils::parseInt("abc").has_value());
    EXPECT_FALSE(ParserUtils::parseInt("rgba(20,20,20,5)").has_value());
    EXPECT_FALSE(ParserUtils::parseInt("#fff").has_value());
}

TEST(ParserUtils, parseColor) {
    EXPECT_EQ(ParserUtils::parseColor("0xDEADBEEF").value_or(0), 0xDEADBEEF);
    EXPECT_EQ(ParserUtils::parseColor("rgba(20, 21, 22, 0.5)").value_or(0), 0x7F141516);
    EXPECT_EQ(ParserUtils::parseColor("rgb(20, 21, 22)").value_or(0), 0xFF141516);
    EXPECT_EQ(ParserUtils::parseColor("rgb(141516)").value_or(0), 0xFF141516);
    EXPECT_EQ(ParserUtils::parseColor("rgba(1415167f)").value_or(0), 0x7F141516);
    EXPECT_EQ(ParserUtils::parseColor("4279506198").value_or(0), 0xFF141516);
    EXPECT_EQ(ParserUtils::parseColor("1").value_or(0), 0x1);
    EXPECT_EQ(ParserUtils::parseColor("#fed").value_or(0), 0xFFFFEEDD);
    EXPECT_EQ(ParserUtils::parseColor("#FED").value_or(0), 0xFFFFEEDD);
    EXPECT_EQ(ParserUtils::parseColor("#deffad").value_or(0), 0xFFDEFFAD);
    EXPECT_EQ(ParserUtils::parseColor("#DEFFaD").value_or(0), 0xFFDEFFAD);
    EXPECT_EQ(ParserUtils::parseColor("#DEFFaDAA").value_or(0), 0xAADEFFAD);
    EXPECT_EQ(ParserUtils::parseColor("#DEFFaDaa").value_or(0), 0xAADEFFAD);
}

TEST(ParserUtils, parseColorBad) {
    EXPECT_FALSE(!!ParserUtils::parseColor("mak"));
    EXPECT_FALSE(!!ParserUtils::parseColor("true"));
    EXPECT_FALSE(!!ParserUtils::parseColor("on"));
    EXPECT_FALSE(!!ParserUtils::parseColor("fucker"));
    EXPECT_FALSE(!!ParserUtils::parseColor("I sniff glue"));
    EXPECT_FALSE(!!ParserUtils::parseColor("0DEADBEEF"));
    EXPECT_FALSE(!!ParserUtils::parseColor("6270000000"));
    EXPECT_FALSE(!!ParserUtils::parseColor("rgba(20, 21, 22)"));
    EXPECT_FALSE(!!ParserUtils::parseColor("rgb(20, 21, 22, 0.2)"));
    EXPECT_FALSE(!!ParserUtils::parseColor("#afed"));
    EXPECT_FALSE(!!ParserUtils::parseColor("#FE"));
    EXPECT_FALSE(!!ParserUtils::parseColor("#defd"));
    EXPECT_FALSE(!!ParserUtils::parseColor("#DEFFD"));
    EXPECT_FALSE(!!ParserUtils::parseColor("#DEFFaDAAe"));
    EXPECT_FALSE(!!ParserUtils::parseColor("#DEFFaDa"));
}