#include <render/OpenGL.hpp>

#include <gtest/gtest.h>

static Vector2D gradientSample(const CBox& box, const Vector2D& monitorSize, const Vector2D& point, bool wholeMonitor, const Vector2D& monitorOffset = {}) {
    const auto UV = Render::GL::resolveBorderGradientUV(box, monitorSize, wholeMonitor, monitorOffset);
    return UV.pos() + ((point - box.pos()) / box.size()) * UV.size();
}

TEST(BorderGradient, MonitorCoordinatesAgreeAcrossDifferentWindows) {
    const Vector2D MONITOR = {1920, 1080};
    const Vector2D POINT   = {720, 450};
    const auto     FIRST   = gradientSample({100, 200, 900, 600}, MONITOR, POINT, true);
    const auto     SECOND  = gradientSample({600, 350, 800, 500}, MONITOR, POINT, true);

    EXPECT_NEAR(FIRST.x, POINT.x / MONITOR.x, 1e-12);
    EXPECT_NEAR(FIRST.y, POINT.y / MONITOR.y, 1e-12);
    EXPECT_NEAR(FIRST.x, SECOND.x, 1e-12);
    EXPECT_NEAR(FIRST.y, SECOND.y, 1e-12);
}

TEST(BorderGradient, FractionalScalingPreservesMonitorCoordinates) {
    const CBox     BOX     = {127, 43, 641, 479};
    const Vector2D MONITOR = {1920, 1080};
    const Vector2D POINT   = {333, 222};
    const auto     LOGICAL = gradientSample(BOX, MONITOR, POINT, true);
    const auto     SCALED  = gradientSample(BOX.copy().scale(1.5), MONITOR * 1.5, POINT * 1.5, true);

    EXPECT_NEAR(LOGICAL.x, SCALED.x, 1e-12);
    EXPECT_NEAR(LOGICAL.y, SCALED.y, 1e-12);
}

TEST(BorderGradient, PortraitMonitorAndOffscreenBordersKeepTheirOrigin) {
    const Vector2D MONITOR = {1080, 1920};
    const CBox     BOX     = {-100, -200, 1500, 2400};
    const auto     TOPLEFT = gradientSample(BOX, MONITOR, {}, true);
    const auto     END     = gradientSample(BOX, MONITOR, MONITOR, true);
    const auto     OUTSIDE = gradientSample(BOX, MONITOR, BOX.pos(), true);

    EXPECT_NEAR(TOPLEFT.x, 0.0, 1e-12);
    EXPECT_NEAR(TOPLEFT.y, 0.0, 1e-12);
    EXPECT_NEAR(END.x, 1.0, 1e-12);
    EXPECT_NEAR(END.y, 1.0, 1e-12);
    EXPECT_LT(OUTSIDE.x, 0.0);
    EXPECT_LT(OUTSIDE.y, 0.0);
}

TEST(BorderGradient, LocalAndMonitorFadeEndpointsHaveIndependentCoordinates) {
    const Vector2D MONITOR = {1920, 1080};
    const CBox     BOX     = {480, 270, 480, 270};
    const auto     LOCAL   = gradientSample(BOX, MONITOR, BOX.pos(), false);
    const auto     GLOBAL  = gradientSample(BOX, MONITOR, BOX.pos(), true);

    EXPECT_DOUBLE_EQ(LOCAL.x, 0.0);
    EXPECT_DOUBLE_EQ(LOCAL.y, 0.0);
    EXPECT_DOUBLE_EQ(GLOBAL.x, 0.25);
    EXPECT_DOUBLE_EQ(GLOBAL.y, 0.25);
    EXPECT_EQ(gradientSample(BOX, MONITOR, BOX.pos() + BOX.size(), false), Vector2D(1, 1));
}

TEST(BorderGradient, InvalidMonitorSizeFallsBackToLocalCoordinates) {
    const CBox BOX = {100, 200, 300, 400};
    EXPECT_EQ(gradientSample(BOX, {}, BOX.pos(), true), Vector2D());
    EXPECT_EQ(gradientSample(BOX, {0, 1080}, BOX.pos() + BOX.size(), true), Vector2D(1, 1));
}

TEST(BorderGradient, CroppedTransformerCanvasPreservesMonitorOrigin) {
    const Vector2D MONITOR = {1920, 1080};
    const Vector2D ORIGIN  = {479, 269};
    const CBox     BOX     = {480, 270, 640, 360};
    const Vector2D POINT   = {800, 450};
    const auto     DIRECT  = gradientSample(BOX, MONITOR, POINT, true);
    const auto     CROPPED = gradientSample(BOX.copy().translate(-ORIGIN), MONITOR, POINT - ORIGIN, true, ORIGIN);

    EXPECT_NEAR(DIRECT.x, CROPPED.x, 1e-12);
    EXPECT_NEAR(DIRECT.y, CROPPED.y, 1e-12);
    EXPECT_EQ(gradientSample(BOX.copy().translate(-ORIGIN), MONITOR, BOX.pos() - ORIGIN, false, ORIGIN), Vector2D());
}
