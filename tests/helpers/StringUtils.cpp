#include <helpers/string/StringUtils.hpp>

#include <gtest/gtest.h>

TEST(Helpers, stringUtilsTruthy) {
    EXPECT_TRUE(StringUtils::truthy("true"));
    EXPECT_TRUE(StringUtils::truthy("trueee"));
    EXPECT_TRUE(StringUtils::truthy("yes"));
    EXPECT_TRUE(StringUtils::truthy("yes sir"));
    EXPECT_TRUE(StringUtils::truthy("on"));
    EXPECT_TRUE(StringUtils::truthy("on !!!"));
    EXPECT_TRUE(StringUtils::truthy("1"));
    EXPECT_TRUE(StringUtils::truthy("2"));
    EXPECT_TRUE(StringUtils::truthy("3"));
    EXPECT_TRUE(StringUtils::truthy("187473743"));
    EXPECT_FALSE(StringUtils::truthy("0"));
    EXPECT_FALSE(StringUtils::truthy("off"));
    EXPECT_FALSE(StringUtils::truthy("disbale"));
    EXPECT_FALSE(StringUtils::truthy("disable"));
    EXPECT_FALSE(StringUtils::truthy("no"));
    EXPECT_FALSE(StringUtils::truthy("false"));
    EXPECT_FALSE(StringUtils::truthy("my balls itch"));
}
