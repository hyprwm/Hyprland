#include <helpers/string/StringUtils.hpp>

#include <gtest/gtest.h>

TEST(Helpers, stringUtilsContainsCaseInsensitive) {
    EXPECT_TRUE(StringUtils::containsCaseInsensitive("LLVMpipe (LLVM 20.1.8, 128 bits)", "llvmpipe"));
    EXPECT_TRUE(StringUtils::containsCaseInsensitive("Mesa software rasterizer", "Software Rasterizer"));
    EXPECT_TRUE(StringUtils::containsCaseInsensitive("aaAb", "AAB"));
    EXPECT_TRUE(StringUtils::containsCaseInsensitive("renderer", ""));
    EXPECT_FALSE(StringUtils::containsCaseInsensitive("AMD Radeon RX 7900", "llvmpipe"));
    EXPECT_FALSE(StringUtils::containsCaseInsensitive("softpip", "softpipe"));
    EXPECT_FALSE(StringUtils::containsCaseInsensitive("", "softpipe"));
    EXPECT_FALSE(StringUtils::containsCaseInsensitive("@", "`"));
    EXPECT_FALSE(StringUtils::containsCaseInsensitive("[", "{"));
    EXPECT_FALSE(StringUtils::containsCaseInsensitive("\xC0", "\xE0"));
    EXPECT_TRUE(StringUtils::containsCaseInsensitive("\xFF!Ab", "\xFF!aB"));
    EXPECT_TRUE(StringUtils::containsCaseInsensitive(std::string_view{"xA\0By", 5}.substr(1, 3), std::string_view{"a\0b", 3}));
    EXPECT_FALSE(StringUtils::containsCaseInsensitive(std::string_view{"softpipe", 4}, "pipe"));
}

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

TEST(Helpers, cmpCaseInsensitive) {
    EXPECT_TRUE(StringUtils::cmpCaseInsensitive("AAAABBBB", "aaaAbbbB"));
    EXPECT_TRUE(StringUtils::cmpCaseInsensitive("FUCK YOU", "fUcK yOu"));
    EXPECT_TRUE(StringUtils::cmpCaseInsensitive("a 優しい friend", "A 優しい friend"));
    EXPECT_FALSE(StringUtils::cmpCaseInsensitive("a 優しい foe", "A 優しい friend"));
    EXPECT_FALSE(StringUtils::cmpCaseInsensitive("a", "B"));
    EXPECT_FALSE(StringUtils::cmpCaseInsensitive("a", ""));
    EXPECT_FALSE(StringUtils::cmpCaseInsensitive("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", ""));
}

TEST(Helpers, truncUTF8InPlaceByteLimit) {
    for (const size_t LENGTH : {0, 1, 511, 512, 513, 8192}) {
        const std::string INPUT(LENGTH, 'a');
        auto              text = INPUT;
        StringUtils::truncUTF8InPlace(text);
        EXPECT_EQ(text, INPUT.substr(0, 512));

        const auto TRUNCATED = text;
        StringUtils::truncUTF8InPlace(text);
        EXPECT_EQ(text, TRUNCATED);
    }
}

TEST(Helpers, truncUTF8InPlaceCodepointBoundaries) {
    for (const std::string CODEPOINT : {"\xC3\xA9", "\xE2\x82\xAC", "\xF0\x9F\x98\x80"}) {
        for (size_t bytesBeforeLimit = 0; bytesBeforeLimit <= CODEPOINT.size(); ++bytesBeforeLimit) {
            const std::string PREFIX(512 - bytesBeforeLimit, 'a');
            auto              text = PREFIX + CODEPOINT + "suffix";
            StringUtils::truncUTF8InPlace(text);
            EXPECT_EQ(text, bytesBeforeLimit == CODEPOINT.size() ? PREFIX + CODEPOINT : PREFIX);
        }
    }
}

TEST(Helpers, truncUTF8InPlaceSmallLimits) {
    const std::string INPUT = "\xF0\x9F\x98\x80";
    for (size_t limit = 0; limit <= INPUT.size() + 1; ++limit) {
        auto text = INPUT;
        StringUtils::truncUTF8InPlace(text, limit);
        EXPECT_EQ(text, limit < INPUT.size() ? "" : INPUT);
    }
}
