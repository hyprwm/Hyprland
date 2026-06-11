#include <helpers/MiscFunctions.hpp>

#include <gtest/gtest.h>

// escapeJSONStrings

TEST(Helpers, escapeJSONStringsBasic) {
    EXPECT_EQ(escapeJSONStrings("hello"), "hello");
    EXPECT_EQ(escapeJSONStrings(""), "");
}

TEST(Helpers, escapeJSONStringsSpecialChars) {
    EXPECT_EQ(escapeJSONStrings("say \"hello\""), "say \\\"hello\\\"");
    EXPECT_EQ(escapeJSONStrings("back\\slash"), "back\\\\slash");
    EXPECT_EQ(escapeJSONStrings("line\nbreak"), "line\\nbreak");
    EXPECT_EQ(escapeJSONStrings("tab\there"), "tab\\there");
    EXPECT_EQ(escapeJSONStrings("cr\rhere"), "cr\\rhere");
}

// normalizeAngleRad

TEST(Helpers, normalizeAngleRadInRange) {
    EXPECT_DOUBLE_EQ(normalizeAngleRad(0.0), 0.0);
    EXPECT_DOUBLE_EQ(normalizeAngleRad(M_PI), M_PI);
    EXPECT_DOUBLE_EQ(normalizeAngleRad(M_PI * 2), M_PI * 2);
}

TEST(Helpers, normalizeAngleRadNegative) {
    EXPECT_NEAR(normalizeAngleRad(-M_PI), M_PI, 0.001);
    EXPECT_NEAR(normalizeAngleRad(-M_PI / 2), 3 * M_PI / 2, 0.001);
}

TEST(Helpers, normalizeAngleRadLarge) {
    EXPECT_NEAR(normalizeAngleRad(3 * M_PI), M_PI, 0.001);
    EXPECT_NEAR(normalizeAngleRad(5 * M_PI), M_PI, 0.001);
}

// stringToPercentage

TEST(Helpers, stringToPercentagePercent) {
    EXPECT_FLOAT_EQ(stringToPercentage("50%", 200.0f), 100.0f);
    EXPECT_FLOAT_EQ(stringToPercentage("100%", 500.0f), 500.0f);
    EXPECT_FLOAT_EQ(stringToPercentage("0%", 1000.0f), 0.0f);
    EXPECT_FLOAT_EQ(stringToPercentage("25%", 400.0f), 100.0f);
}

TEST(Helpers, stringToPercentageAbsolute) {
    EXPECT_FLOAT_EQ(stringToPercentage("42", 999.0f), 42.0f);
    EXPECT_FLOAT_EQ(stringToPercentage("0", 999.0f), 0.0f);
    EXPECT_FLOAT_EQ(stringToPercentage("1.5", 999.0f), 1.5f);
}
