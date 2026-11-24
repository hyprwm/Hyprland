#include <debug/crash/SignalSafe.hpp>

#include <gtest/gtest.h>

static std::string numToString(size_t num) {
    SignalSafe::CMaxLengthCString<32> str;
    str.writeNum(num);
    return str.getStr();
}

TEST(SignalSafe, maxLengthCStringWriteNum) {
    EXPECT_EQ(numToString(0), "0");
    EXPECT_EQ(numToString(7), "7");
    EXPECT_EQ(numToString(10), "10");
    EXPECT_EQ(numToString(108960), "108960");
    EXPECT_EQ(numToString(1000000), "1000000");
}

TEST(SignalSafe, maxLengthCStringWriteNumBounds) {
    SignalSafe::CMaxLengthCString<4> str;
    str.writeNum(123);
    EXPECT_FALSE(str.boundsExceeded());
    EXPECT_STREQ(str.getStr(), "123");

    str.writeNum(4);
    EXPECT_TRUE(str.boundsExceeded());
    EXPECT_STREQ(str.getStr(), "123");
}
