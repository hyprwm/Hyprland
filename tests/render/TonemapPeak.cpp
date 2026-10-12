#include <render/Renderer.hpp>

#include <gtest/gtest.h>

using namespace NColorManagement;

namespace {
    constexpr auto PQ     = CM_TRANSFER_FUNCTION_ST2084_PQ;
    constexpr auto HLG    = CM_TRANSFER_FUNCTION_HLG;
    constexpr auto LINEAR = CM_TRANSFER_FUNCTION_EXT_LINEAR;
    constexpr auto SRGB   = CM_TRANSFER_FUNCTION_SRGB;
    constexpr auto G22    = CM_TRANSFER_FUNCTION_GAMMA22;
}

// PQ output with a 630 nit peak and the default 203 nit reference composites into a linear work buffer
// with the same luminances. A client with a raised reference white must keep its real peak, otherwise
// 1000 * 203 / 446 = 455 falls under 630 and the shader clamps instead of tonemapping.
TEST(Render, tonemapPeakKeepsAbsolutePQPeakOnHDRWorkBuffer) {
    EXPECT_FLOAT_EQ(Render::tonemapShaderMaxLuminance(PQ, LINEAR, 1000, 630, 446, 203), 1000);
}

TEST(Render, tonemapPeakKeepsAbsolutePQPeakOnPQTarget) {
    EXPECT_FLOAT_EQ(Render::tonemapShaderMaxLuminance(PQ, PQ, 1000, 630, 446, 203), 1000);
}

TEST(Render, tonemapPeakKeepsAbsoluteHLGPeak) {
    EXPECT_FLOAT_EQ(Render::tonemapShaderMaxLuminance(HLG, LINEAR, 1000, 630, 446, 203), 1000);
}

TEST(Render, tonemapPeakKeepsAbsoluteLinearSourcePeak) {
    EXPECT_FLOAT_EQ(Render::tonemapShaderMaxLuminance(LINEAR, LINEAR, 1000, 630, 446, 203), 1000);
}

// An explicit PQ/HLG target is HDR even when its peak does not exceed its reference white.
TEST(Render, tonemapPeakTreatsExplicitPQTargetAsHDRWithoutHeadroom) {
    EXPECT_FLOAT_EQ(Render::tonemapShaderMaxLuminance(PQ, PQ, 1000, 203, 1000, 203), 1000);
}

TEST(Render, tonemapPeakUnchangedWhenReferencesMatch) {
    EXPECT_FLOAT_EQ(Render::tonemapShaderMaxLuminance(PQ, LINEAR, 1000, 630, 203, 203), 1000);
}

// An SDR output's linear work buffer has no headroom, so it keeps the reference-scaled peak.
TEST(Render, tonemapPeakScalesOnSDRWorkBuffer) {
    EXPECT_FLOAT_EQ(Render::tonemapShaderMaxLuminance(PQ, LINEAR, 1000, 80, 203, 80), 1000.F * 80 / 203);
}

// HDR to SDR paths (capture, mirrors, SDR outputs) keep the existing behaviour.
TEST(Render, tonemapPeakScalesOnSDRTargets) {
    EXPECT_FLOAT_EQ(Render::tonemapShaderMaxLuminance(LINEAR, SRGB, 630, 80, 203, 80), 630.F * 80 / 203);
    EXPECT_FLOAT_EQ(Render::tonemapShaderMaxLuminance(PQ, G22, 10000, 80, 203, 80), 10000.F * 80 / 203);
}

TEST(Render, tonemapPeakScalesForSDRSources) {
    EXPECT_FLOAT_EQ(Render::tonemapShaderMaxLuminance(G22, LINEAR, 400, 630, 400, 203), 400.F * 203 / 400);
}
