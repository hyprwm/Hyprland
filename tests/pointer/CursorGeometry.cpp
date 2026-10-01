#include <gtest/gtest.h>

#include <pointer/PointerManager.hpp>
#include <cairo/cairo.h>

#include <array>

using SCursorImageData = Pointer::CPointerManager::SCursorImageData;

static void expectPoint(const Vector2D& actual, const Vector2D& expected) {
    EXPECT_NEAR(actual.x, expected.x, 1e-6);
    EXPECT_NEAR(actual.y, expected.y, 1e-6);
}

static Vector2D sample(cairo_matrix_t matrix, Vector2D point) {
    cairo_matrix_transform_point(&matrix, &point.x, &point.y);
    return point;
}

TEST(CursorGeometry, NativeMixedScalesUseReturnedPixelsRatherThanRequestedSize) {
    const SCursorImageData unit{.hotspot = {3, 5}, .size = {24, 18}, .scale = 1.F};
    // A nominal 24px cursor requests 30px at 1.25x, but this theme returns a 32px image.
    const SCursorImageData fractional{.hotspot = {7 / 1.25, 11 / 1.25}, .size = {32, 24}, .scale = 1.25F};

    expectPoint(unit.logicalSize(), {24, 18});
    expectPoint(fractional.logicalSize(), {25.6, 19.2});
    expectPoint(unit.outputSize(1.F), {24, 18});
    expectPoint(fractional.outputSize(1.25F), {32, 24});
    expectPoint(fractional.planeHotspot(1.25F, WL_OUTPUT_TRANSFORM_NORMAL, {80, 64}), {7, 11});

    const auto box = fractional.logicalBox({100, 200});
    expectPoint(box.pos(), {94.4, 191.2});
    expectPoint(box.size(), {25.6, 19.2});

    // The output's own image determines overlap, even when its logical extent differs from the representative.
    const CBox neighboringOutput{120.5, 0, 100, 100};
    EXPECT_TRUE(unit.logicalBox({100, 50}).overlaps(neighboringOutput));
    EXPECT_FALSE(fractional.logicalBox({100, 50}).overlaps(neighboringOutput));
}

struct STransformExpectation {
    wl_output_transform transform = WL_OUTPUT_TRANSFORM_NORMAL;
    Vector2D            footprint;
    Vector2D            hotspot;
    Vector2D            origin;
    Vector2D            corner;
};

// A 32x24 image and asymmetric (7,11) pixel hotspot in an 80x64 padded plane.
// Coordinates are explicit so neither the CPU matrix nor the hotspot transform serves as the other's oracle.
static const std::array TRANSFORMS = {
    STransformExpectation{WL_OUTPUT_TRANSFORM_NORMAL, {32, 24}, {7, 11}, {0, 0}, {32, 24}},
    STransformExpectation{WL_OUTPUT_TRANSFORM_90, {24, 32}, {11, 57}, {0, 64}, {24, 32}},
    STransformExpectation{WL_OUTPUT_TRANSFORM_180, {32, 24}, {73, 53}, {80, 64}, {48, 40}},
    STransformExpectation{WL_OUTPUT_TRANSFORM_270, {24, 32}, {69, 7}, {80, 0}, {56, 32}},
    STransformExpectation{WL_OUTPUT_TRANSFORM_FLIPPED, {32, 24}, {73, 11}, {80, 0}, {48, 24}},
    STransformExpectation{WL_OUTPUT_TRANSFORM_FLIPPED_90, {24, 32}, {11, 7}, {0, 0}, {24, 32}},
    STransformExpectation{WL_OUTPUT_TRANSFORM_FLIPPED_180, {32, 24}, {7, 53}, {0, 64}, {32, 40}},
    STransformExpectation{WL_OUTPUT_TRANSFORM_FLIPPED_270, {24, 32}, {69, 57}, {80, 64}, {56, 32}},
};

class CCursorTransformTest : public ::testing::TestWithParam<STransformExpectation> {};

TEST_P(CCursorTransformTest, FootprintAndHotspotUseOutputPixelsAndPaddedPlane) {
    const auto& expected = GetParam();
    for (const float scale : {1.F, 1.25F}) {
        SCOPED_TRACE(scale);
        const SCursorImageData image{.hotspot = Vector2D{7, 11} / scale, .size = {32, 24}, .scale = scale};

        expectPoint(image.planeSize(scale, expected.transform), expected.footprint);
        expectPoint(image.planeHotspot(scale, expected.transform, {80, 64}), expected.hotspot);
    }
}

TEST_P(CCursorTransformTest, CairoSamplesExpectedSourcePointsForEveryTransform) {
    const auto&            expected = GetParam();
    const SCursorImageData image{.hotspot = {5.6, 8.8}, .size = {32, 24}, .scale = 1.25F};
    const auto             native = image.cairoMatrix({32, 24}, 1.25F, expected.transform, {80, 64});

    expectPoint(sample(native, expected.origin), {0, 0});
    expectPoint(sample(native, expected.hotspot), {7, 11});
    expectPoint(sample(native, expected.corner), {32, 24});

    // Nonuniform source scaling catches swapped X/Y factors on quarter-turns.
    const auto scaled = image.cairoMatrix({64, 72}, 1.25F, expected.transform, {80, 64});
    expectPoint(sample(scaled, expected.origin), {0, 0});
    expectPoint(sample(scaled, expected.hotspot), {14, 33});
    expectPoint(sample(scaled, expected.corner), {64, 72});
}

INSTANTIATE_TEST_SUITE_P(AllOutputTransforms, CCursorTransformTest, ::testing::ValuesIn(TRANSFORMS));

TEST(CursorGeometry, ClientBufferScaleCanDifferFromOutputScale) {
    // Client surfaces retain their buffer scale rather than adopting the output's scale.
    const SCursorImageData client{.hotspot = {3.5, 5.5}, .size = {48, 32}, .scale = 2.F};

    expectPoint(client.logicalSize(), {24, 16});
    expectPoint(client.logicalBox({100, 200}).pos(), {96.5, 194.5});
    expectPoint(client.outputSize(1.25F), {30, 20});
    expectPoint(client.planeSize(1.25F, WL_OUTPUT_TRANSFORM_90), {20, 30});
    expectPoint(client.planeHotspot(1.25F, WL_OUTPUT_TRANSFORM_90, {80, 64}), {6.875, 59.625});

    const auto matrix = client.cairoMatrix({48, 32}, 1.25F, WL_OUTPUT_TRANSFORM_90, {80, 64});
    expectPoint(sample(matrix, {0, 64}), {0, 0});
    expectPoint(sample(matrix, {6.875, 59.625}), {7, 11});
    expectPoint(sample(matrix, {20, 34}), {48, 32});
}

TEST(CursorGeometry, CairoUsesTheSameRoundedDestinationAsHardwareRendering) {
    const SCursorImageData image{.size = {31, 21}, .scale = 2.F};
    expectPoint(image.outputSize(1.25F), {19, 13});
    expectPoint(image.planeSize(1.25F, WL_OUTPUT_TRANSFORM_270), {13, 19});

    const auto matrix = image.cairoMatrix({31, 21}, 1.25F, WL_OUTPUT_TRANSFORM_270, {80, 64});
    expectPoint(sample(matrix, {80, 0}), {0, 0});
    expectPoint(sample(matrix, {67, 19}), {31, 21});
}

TEST(CursorGeometry, PlaneFootprintUsesScaledDestinationRatherThanSourceDimensions) {
    const SCursorImageData downscaled{.size = {96, 64}, .scale = 2.F};
    const SCursorImageData upscaled{.size = {48, 32}, .scale = 1.F};

    // The large source fits a 64x64 plane after downscaling; the smaller source does not after upscaling.
    expectPoint(downscaled.planeSize(1.F, WL_OUTPUT_TRANSFORM_90), {32, 48});
    expectPoint(upscaled.planeSize(2.F, WL_OUTPUT_TRANSFORM_90), {64, 96});
}
