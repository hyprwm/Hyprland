#include <helpers/cm/ColorManagement.hpp>
#include <render/types.hpp>

#include <gtest/gtest.h>

using namespace NColorManagement;

static RGBAColor convertWithoutTonemapping(RGBAColor color, PImageDescription source, PImageDescription target) {
    const Render::SCMSettings settings = {
        .sourceTF   = source->value().transferFunction,
        .targetTF   = target->value().transferFunction,
        .srcTFRange = {.min = source->value().getTFMinLuminance(), .max = source->value().getTFMaxLuminance()},
        .dstTFRange = {.min = target->value().getTFMinLuminance(), .max = target->value().getTFMaxLuminance()},
    };
    return convertColor(color, source, target, settings);
}

static void expectNeutralColor(const RGBAColor& color, double luminance, double alpha = 1.0) {
    EXPECT_NEAR(color.c.r, luminance, 0.0001);
    EXPECT_NEAR(color.c.g, luminance, 0.0001);
    EXPECT_NEAR(color.c.b, luminance, 0.0001);
    EXPECT_DOUBLE_EQ(color.c.a, alpha);
}

TEST(ColorManagement, WindowsScRGBDecodesReferenceWhite) {
    const auto color = convertWithoutTonemapping({{.r = 2.5375, .g = 2.5375, .b = 2.5375, .a = 1.0}}, SCRGB_IMAGE_DESCRIPTION, LINEAR_NN_IMAGE_DESCRIPTION);
    expectNeutralColor(color, 203.0);
}

TEST(ColorManagement, WorkBufferPreservesScRGBEncodingWithMonitorMetadata) {
    for (const auto& luminances : {
             SImageDescription::SPCLuminances{.min = 0.2f, .max = 80, .reference = 80},
             SImageDescription::SPCLuminances{.min = 0.005f, .max = 600, .reference = 203},
             SImageDescription::SPCLuminances{.min = 0.0f, .max = 10000, .reference = 203},
         }) {
        const auto workBuffer = LINEAR_IMAGE_DESCRIPTION->with(luminances);
        EXPECT_EQ(workBuffer->value().luminances, luminances);

        const auto encoded = convertWithoutTonemapping({{.r = 203.0, .g = 203.0, .b = 203.0, .a = 1.0}}, LINEAR_NN_IMAGE_DESCRIPTION, workBuffer);
        expectNeutralColor(encoded, 2.5375);

        const auto decoded = convertWithoutTonemapping(encoded, workBuffer, LINEAR_NN_IMAGE_DESCRIPTION);
        expectNeutralColor(decoded, 203.0);
    }
}

TEST(ColorManagement, EquivalentEncodingsDoNotRequireConversion) {
    const auto workBuffer = LINEAR_IMAGE_DESCRIPTION->with({.min = 0.0f, .max = 80, .reference = 203});
    auto       native     = workBuffer->value();
    native.isWindows      = false;
    const auto source     = CImageDescription::from(native);

    ASSERT_NE(source->id(), workBuffer->id());
    EXPECT_FALSE(source->needsCM(workBuffer));
    EXPECT_FALSE(workBuffer->needsCM(source));
    EXPECT_FALSE(workBuffer->needsCM(workBuffer));
}
