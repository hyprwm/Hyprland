#include <helpers/CMType.hpp>

#include <gtest/gtest.h>

using namespace NCMType;

TEST(Helpers, cmTypeFromStringValid) {
    EXPECT_EQ(fromString("auto"), CM_AUTO);
    EXPECT_EQ(fromString("srgb"), CM_SRGB);
    EXPECT_EQ(fromString("wide"), CM_WIDE);
    EXPECT_EQ(fromString("edid"), CM_EDID);
    EXPECT_EQ(fromString("hdr"), CM_HDR);
    EXPECT_EQ(fromString("hdredid"), CM_HDR_EDID);
    EXPECT_EQ(fromString("dcip3"), CM_DCIP3);
    EXPECT_EQ(fromString("dp3"), CM_DP3);
    EXPECT_EQ(fromString("adobe"), CM_ADOBE);
}

TEST(Helpers, cmTypeFromStringInvalid) {
    EXPECT_EQ(fromString(""), std::nullopt);
    EXPECT_EQ(fromString("invalid"), std::nullopt);
    EXPECT_EQ(fromString("SRGB"), std::nullopt);
    EXPECT_EQ(fromString("HDR"), std::nullopt);
    EXPECT_EQ(fromString("Auto"), std::nullopt);
}

TEST(Helpers, cmTypeToString) {
    EXPECT_EQ(toString(CM_AUTO), "auto");
    EXPECT_EQ(toString(CM_SRGB), "srgb");
    EXPECT_EQ(toString(CM_WIDE), "wide");
    EXPECT_EQ(toString(CM_EDID), "edid");
    EXPECT_EQ(toString(CM_HDR), "hdr");
    EXPECT_EQ(toString(CM_HDR_EDID), "hdredid");
    EXPECT_EQ(toString(CM_DCIP3), "dcip3");
    EXPECT_EQ(toString(CM_DP3), "dp3");
    EXPECT_EQ(toString(CM_ADOBE), "adobe");
}

TEST(Helpers, cmTypeRoundTrip) {
    EXPECT_EQ(fromString(toString(CM_AUTO)), CM_AUTO);
    EXPECT_EQ(fromString(toString(CM_SRGB)), CM_SRGB);
    EXPECT_EQ(fromString(toString(CM_WIDE)), CM_WIDE);
    EXPECT_EQ(fromString(toString(CM_EDID)), CM_EDID);
    EXPECT_EQ(fromString(toString(CM_HDR)), CM_HDR);
    EXPECT_EQ(fromString(toString(CM_HDR_EDID)), CM_HDR_EDID);
    EXPECT_EQ(fromString(toString(CM_DCIP3)), CM_DCIP3);
    EXPECT_EQ(fromString(toString(CM_DP3)), CM_DP3);
    EXPECT_EQ(fromString(toString(CM_ADOBE)), CM_ADOBE);
}
