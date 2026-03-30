#include <helpers/Format.hpp>

#include <gtest/gtest.h>

#include <drm_fourcc.h>
#include <wayland-server-protocol.h>

using namespace NFormatUtils;

TEST(Helpers, formatDrmToShm) {
    EXPECT_EQ(drmToShm(DRM_FORMAT_XRGB8888), WL_SHM_FORMAT_XRGB8888);
    EXPECT_EQ(drmToShm(DRM_FORMAT_ARGB8888), WL_SHM_FORMAT_ARGB8888);
}

TEST(Helpers, formatShmToDrm) {
    EXPECT_EQ(shmToDRM(WL_SHM_FORMAT_XRGB8888), DRM_FORMAT_XRGB8888);
    EXPECT_EQ(shmToDRM(WL_SHM_FORMAT_ARGB8888), DRM_FORMAT_ARGB8888);
}

TEST(Helpers, formatDrmShmRoundTrip) {
    EXPECT_EQ(shmToDRM(drmToShm(DRM_FORMAT_XRGB8888)), DRM_FORMAT_XRGB8888);
    EXPECT_EQ(shmToDRM(drmToShm(DRM_FORMAT_ARGB8888)), DRM_FORMAT_ARGB8888);
}

TEST(Helpers, formatIsFormatYUV) {
    EXPECT_TRUE(isFormatYUV(DRM_FORMAT_YUYV));
    EXPECT_TRUE(isFormatYUV(DRM_FORMAT_NV12));
    EXPECT_TRUE(isFormatYUV(DRM_FORMAT_NV21));
    EXPECT_FALSE(isFormatYUV(DRM_FORMAT_XRGB8888));
    EXPECT_FALSE(isFormatYUV(DRM_FORMAT_ARGB8888));
}

TEST(Helpers, formatGetPixelFormatFromDRM) {
    const auto* xrgb = getPixelFormatFromDRM(DRM_FORMAT_XRGB8888);
    ASSERT_NE(xrgb, nullptr);
    EXPECT_EQ(xrgb->drmFormat, DRM_FORMAT_XRGB8888);
    EXPECT_FALSE(xrgb->withAlpha);

    const auto* argb = getPixelFormatFromDRM(DRM_FORMAT_ARGB8888);
    ASSERT_NE(argb, nullptr);
    EXPECT_EQ(argb->drmFormat, DRM_FORMAT_ARGB8888);
    EXPECT_TRUE(argb->withAlpha);

    EXPECT_EQ(getPixelFormatFromDRM(0), nullptr);
}

TEST(Helpers, formatIsFormatOpaque) {
    EXPECT_TRUE(isFormatOpaque(DRM_FORMAT_XRGB8888));
    EXPECT_FALSE(isFormatOpaque(DRM_FORMAT_ARGB8888));
}

TEST(Helpers, formatPixelsPerBlock) {
    const auto* fmt = getPixelFormatFromDRM(DRM_FORMAT_XRGB8888);
    ASSERT_NE(fmt, nullptr);
    EXPECT_GT(pixelsPerBlock(fmt), 0);
}

TEST(Helpers, formatMinStride) {
    const auto* fmt = getPixelFormatFromDRM(DRM_FORMAT_XRGB8888);
    ASSERT_NE(fmt, nullptr);
    // XRGB8888 = 4 bytes per pixel, 1920 wide = 7680 bytes stride
    EXPECT_EQ(minStride(fmt, 1920), 1920 * 4);
    EXPECT_EQ(minStride(fmt, 0), 0);
}

TEST(Helpers, formatDrmFormatName) {
    EXPECT_FALSE(drmFormatName(DRM_FORMAT_XRGB8888).empty());
    EXPECT_FALSE(drmFormatName(DRM_FORMAT_ARGB8888).empty());
    EXPECT_EQ(drmFormatName(0), "INVALID");
}

TEST(Helpers, formatAlphaFormat) {
    EXPECT_EQ(alphaFormat(DRM_FORMAT_XRGB8888), DRM_FORMAT_ARGB8888);
    EXPECT_EQ(alphaFormat(DRM_FORMAT_XBGR8888), DRM_FORMAT_ABGR8888);
    // Format without alpha stripped entry returns DRM_FORMAT_INVALID (0)
    EXPECT_EQ(alphaFormat(DRM_FORMAT_ARGB8888), 0u);
}
