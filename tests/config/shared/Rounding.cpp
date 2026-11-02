#include <config/shared/rounding/Rounding.hpp>
#include <config/shared/complex/CornersData.hpp>

#include <gtest/gtest.h>
#include <array>
#include <limits>

using namespace Config;

TEST(ConfigRounding, rawParsingExpandsCssShorthandClockwise) {
    const std::array cases = {
        std::pair{"4", "4"},
        std::pair{"1 2", "1 2 1 2"},
        std::pair{"1 2 3", "1 2 3 2"},
        std::pair{" 1\t2  3\n4 ", "1 2 3 4"},
        std::pair{"0x10", "16"},
        std::pair{"101 102 103 104", "101 102 103 104"},
        std::pair{"9223372036854775807", "9223372036854775807"},
    };
    for (const auto& [raw, expanded] : cases) {
        SCOPED_TRACE(raw);
        const auto parsed = Rounding::parse(raw);
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(parsed->toString(), expanded);
    }
}

TEST(ConfigRounding, rawParsingRejectsMalformedOrNegativeComponents) {
    for (const auto raw : {"", " ", "1 2 3 4 5", "1 2 bad 4", "1.5", "1px", "-1", "1 2 -3 4", "9223372036854775808", "-9223372036854775809", "true", "+-1", "++1"}) {
        SCOPED_TRACE(raw);
        EXPECT_FALSE(Rounding::parse(raw).has_value());
    }
}

TEST(ConfigRounding, setPropClampsOnlyNegativeComponents) {
    const auto parsed = Rounding::parseProp("-1 200 -3 400", CCornersData(5));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->toString(), "0 200 0 400");
}

TEST(ConfigRounding, relativeSetPropBroadcastsRawSignedDelta) {
    const auto increased = Rounding::parseProp("relative +5", CCornersData(1, 2, 30, 400));
    ASSERT_TRUE(increased.has_value());
    EXPECT_EQ(increased->toString(), "6 7 35 405");

    const auto decreased = Rounding::parseProp("relative -8", *increased);
    ASSERT_TRUE(decreased.has_value());
    EXPECT_EQ(decreased->toString(), "0 0 27 397");

    const auto minimum = Rounding::parseProp("relative -9223372036854775808", CCornersData(std::numeric_limits<int64_t>::max()));
    ASSERT_TRUE(minimum.has_value());
    EXPECT_EQ(minimum->toString(), "0");
}

TEST(ConfigRounding, setPropFailureLeavesAllComponentsUntouched) {
    const CCornersData current(1, 2, 3, std::numeric_limits<int64_t>::max());
    for (const auto raw : {"relative 1", "relative", "relative 1 2", "relative x", "relative 9223372036854775808", "1 2 3 invalid", "1 2 3 9223372036854775808"}) {
        SCOPED_TRACE(raw);
        EXPECT_FALSE(Rounding::parseProp(raw, current).has_value());
        EXPECT_EQ(current.toString(), "1 2 3 9223372036854775807");
    }
}

TEST(ConfigRounding, serializationPreservesUniformScalarAndRawIntegers) {
    EXPECT_TRUE(CCornersData(4).isUniform());
    EXPECT_EQ(CCornersData(4).toString(), "4");
    EXPECT_EQ(CCornersData(4).toJson(), "4");
    EXPECT_TRUE(CCornersData(4, 4, 4, 4).isUniform());
    EXPECT_EQ(CCornersData(4, 4, 4, 4).toString(), "4");
    EXPECT_FALSE(CCornersData(1, 2, 3, 400).isUniform());
    EXPECT_EQ(CCornersData(1, 2, 3, 400).toString(), "1 2 3 400");
    EXPECT_EQ(CCornersData(1, 2, 3, 400).toJson(), "[1,2,3,400]");
    EXPECT_EQ(CCornersData(std::numeric_limits<int64_t>::max()).toJson(), "9223372036854775807");
}
