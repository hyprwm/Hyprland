#include <helpers/ByteOperations.hpp>

#include <gtest/gtest.h>

TEST(Helpers, byteOperatorsIntegral) {
    EXPECT_EQ(1_kB, 1024ULL);
    EXPECT_EQ(1_MB, 1024ULL * 1024);
    EXPECT_EQ(1_GB, 1024ULL * 1024 * 1024);
    EXPECT_EQ(1_TB, 1024ULL * 1024 * 1024 * 1024);
    EXPECT_EQ(5_MB, 5ULL * 1024 * 1024);
}

TEST(Helpers, byteOperatorsFloating) {
    EXPECT_DOUBLE_EQ(1.5_kB, 1.5L * 1024);
    EXPECT_DOUBLE_EQ(0.5_MB, 0.5L * 1024 * 1024);
    EXPECT_DOUBLE_EQ(2.5_GB, 2.5L * 1024 * 1024 * 1024);
    EXPECT_DOUBLE_EQ(0.25_TB, 0.25L * 1024 * 1024 * 1024 * 1024);
}

TEST(Helpers, byteOperatorsZero) {
    EXPECT_EQ(0_kB, 0ULL);
    EXPECT_EQ(0_MB, 0ULL);
    EXPECT_EQ(0_GB, 0ULL);
    EXPECT_EQ(0_TB, 0ULL);
}

TEST(Helpers, byteConversionFunctions) {
    EXPECT_EQ(kBtoBytes(1ULL), 1024ULL);
    EXPECT_EQ(MBtoBytes(1ULL), 1024ULL * 1024);
    EXPECT_EQ(GBtoBytes(1ULL), 1024ULL * 1024 * 1024);
    EXPECT_EQ(TBtoBytes(1ULL), 1024ULL * 1024 * 1024 * 1024);
    EXPECT_EQ(kBtoBytes(0ULL), 0ULL);
}

TEST(Helpers, byteOperatorsChain) {
    EXPECT_EQ(1_MB, 1024_kB);
    EXPECT_EQ(1_GB, 1024_MB);
    EXPECT_EQ(1_TB, 1024_GB);
}
