#include <helpers/TransferFunction.hpp>

#include <gtest/gtest.h>

using namespace NTransferFunction;

TEST(Helpers, transferFunctionFromStringNamed) {
    EXPECT_EQ(fromString("default"), TF_DEFAULT);
    EXPECT_EQ(fromString("auto"), TF_AUTO);
    EXPECT_EQ(fromString("srgb"), TF_SRGB);
    EXPECT_EQ(fromString("gamma22"), TF_GAMMA22);
    EXPECT_EQ(fromString("gamma22force"), TF_FORCED_GAMMA22);
}

TEST(Helpers, transferFunctionFromStringNumeric) {
    EXPECT_EQ(fromString("0"), TF_DEFAULT);
    EXPECT_EQ(fromString("1"), TF_GAMMA22);
    EXPECT_EQ(fromString("2"), TF_FORCED_GAMMA22);
    EXPECT_EQ(fromString("3"), TF_SRGB);
}

TEST(Helpers, transferFunctionFromStringInvalid) {
    EXPECT_EQ(fromString(""), TF_DEFAULT);
    EXPECT_EQ(fromString("invalid"), TF_DEFAULT);
    EXPECT_EQ(fromString("SRGB"), TF_DEFAULT);
    EXPECT_EQ(fromString("Gamma22"), TF_DEFAULT);
}

TEST(Helpers, transferFunctionToString) {
    EXPECT_FALSE(toString(TF_DEFAULT).empty());
    EXPECT_FALSE(toString(TF_AUTO).empty());
    EXPECT_FALSE(toString(TF_SRGB).empty());
    EXPECT_FALSE(toString(TF_GAMMA22).empty());
    EXPECT_FALSE(toString(TF_FORCED_GAMMA22).empty());
}

TEST(Helpers, transferFunctionRoundTrip) {
    EXPECT_EQ(fromString(toString(TF_DEFAULT)), TF_DEFAULT);
    EXPECT_EQ(fromString(toString(TF_AUTO)), TF_AUTO);
    EXPECT_EQ(fromString(toString(TF_SRGB)), TF_SRGB);
    EXPECT_EQ(fromString(toString(TF_GAMMA22)), TF_GAMMA22);
    EXPECT_EQ(fromString(toString(TF_FORCED_GAMMA22)), TF_FORCED_GAMMA22);
}
