#include <helpers/math/Math.hpp>
#include <helpers/memory/Memory.hpp>

#include <gtest/gtest.h>

using namespace Math;

// wlTransformToHyprutils

TEST(Helpers, mathWlTransformToHyprutils) {
    EXPECT_EQ(wlTransformToHyprutils(WL_OUTPUT_TRANSFORM_NORMAL), eTransform::HYPRUTILS_TRANSFORM_NORMAL);
    EXPECT_EQ(wlTransformToHyprutils(WL_OUTPUT_TRANSFORM_90), eTransform::HYPRUTILS_TRANSFORM_90);
    EXPECT_EQ(wlTransformToHyprutils(WL_OUTPUT_TRANSFORM_180), eTransform::HYPRUTILS_TRANSFORM_180);
    EXPECT_EQ(wlTransformToHyprutils(WL_OUTPUT_TRANSFORM_270), eTransform::HYPRUTILS_TRANSFORM_270);
    EXPECT_EQ(wlTransformToHyprutils(WL_OUTPUT_TRANSFORM_FLIPPED), eTransform::HYPRUTILS_TRANSFORM_FLIPPED);
    EXPECT_EQ(wlTransformToHyprutils(WL_OUTPUT_TRANSFORM_FLIPPED_90), eTransform::HYPRUTILS_TRANSFORM_FLIPPED_90);
    EXPECT_EQ(wlTransformToHyprutils(WL_OUTPUT_TRANSFORM_FLIPPED_180), eTransform::HYPRUTILS_TRANSFORM_FLIPPED_180);
    EXPECT_EQ(wlTransformToHyprutils(WL_OUTPUT_TRANSFORM_FLIPPED_270), eTransform::HYPRUTILS_TRANSFORM_FLIPPED_270);
}

TEST(Helpers, mathWlTransformToHyprutilsInvalid) {
    // Invalid value falls back to NORMAL
    EXPECT_EQ(wlTransformToHyprutils(sc<wl_output_transform>(99)), eTransform::HYPRUTILS_TRANSFORM_NORMAL);
}

// invertTransform

TEST(Helpers, mathInvertTransformNonRotated) {
    // Non-rotated transforms are their own inverse
    EXPECT_EQ(invertTransform(WL_OUTPUT_TRANSFORM_NORMAL), WL_OUTPUT_TRANSFORM_NORMAL);
    EXPECT_EQ(invertTransform(WL_OUTPUT_TRANSFORM_180), WL_OUTPUT_TRANSFORM_180);
    EXPECT_EQ(invertTransform(WL_OUTPUT_TRANSFORM_FLIPPED), WL_OUTPUT_TRANSFORM_FLIPPED);
    EXPECT_EQ(invertTransform(WL_OUTPUT_TRANSFORM_FLIPPED_180), WL_OUTPUT_TRANSFORM_FLIPPED_180);
}

TEST(Helpers, mathInvertTransformRotated) {
    // 90 and 270 swap when inverted (non-flipped)
    EXPECT_EQ(invertTransform(WL_OUTPUT_TRANSFORM_90), WL_OUTPUT_TRANSFORM_270);
    EXPECT_EQ(invertTransform(WL_OUTPUT_TRANSFORM_270), WL_OUTPUT_TRANSFORM_90);
}

TEST(Helpers, mathInvertTransformFlippedRotated) {
    // Flipped rotations: flipped bit stays, 90/270 don't swap
    EXPECT_EQ(invertTransform(WL_OUTPUT_TRANSFORM_FLIPPED_90), WL_OUTPUT_TRANSFORM_FLIPPED_90);
    EXPECT_EQ(invertTransform(WL_OUTPUT_TRANSFORM_FLIPPED_270), WL_OUTPUT_TRANSFORM_FLIPPED_270);
}

TEST(Helpers, mathInvertTransformDoubleInvert) {
    // Double invert returns original for all transforms
    for (int i = 0; i <= 7; i++) {
        auto t = sc<wl_output_transform>(i);
        EXPECT_EQ(invertTransform(invertTransform(t)), t);
    }
}

// composeTransform

TEST(Helpers, mathComposeTransformIdentity) {
    // Composing with NORMAL is identity
    for (int i = 0; i <= 7; i++) {
        auto t = sc<eTransform>(i);
        EXPECT_EQ(composeTransform(t, eTransform::HYPRUTILS_TRANSFORM_NORMAL), t);
        EXPECT_EQ(composeTransform(eTransform::HYPRUTILS_TRANSFORM_NORMAL, t), t);
    }
}

TEST(Helpers, mathComposeTransformRotation) {
    // 90 + 90 = 180
    EXPECT_EQ(composeTransform(eTransform::HYPRUTILS_TRANSFORM_90, eTransform::HYPRUTILS_TRANSFORM_90), eTransform::HYPRUTILS_TRANSFORM_180);
    // 90 + 180 = 270
    EXPECT_EQ(composeTransform(eTransform::HYPRUTILS_TRANSFORM_90, eTransform::HYPRUTILS_TRANSFORM_180), eTransform::HYPRUTILS_TRANSFORM_270);
    // 180 + 180 = NORMAL (360)
    EXPECT_EQ(composeTransform(eTransform::HYPRUTILS_TRANSFORM_180, eTransform::HYPRUTILS_TRANSFORM_180), eTransform::HYPRUTILS_TRANSFORM_NORMAL);
}

TEST(Helpers, outputDamageTransformRoundTrip) {
    constexpr Vector2D SCENE_SIZE = {320, 180};
    const CBox         DAMAGE     = {17, 29, 43, 31};

    for (int i = 0; i <= 7; ++i) {
        const auto TRANSFORM   = sc<wl_output_transform>(i);
        const auto BUFFER_SIZE = i % 2 == 0 ? SCENE_SIZE : Vector2D{SCENE_SIZE.y, SCENE_SIZE.x};
        CRegion    transformed{DAMAGE};

        transformed.transform(wlTransformToHyprutils(invertTransform(TRANSFORM)), SCENE_SIZE.x, SCENE_SIZE.y);
        const auto EXTENTS = transformed.getExtents();

        EXPECT_GE(EXTENTS.x, 0);
        EXPECT_GE(EXTENTS.y, 0);
        EXPECT_LE(EXTENTS.x + EXTENTS.w, BUFFER_SIZE.x);
        EXPECT_LE(EXTENTS.y + EXTENTS.h, BUFFER_SIZE.y);

        transformed.transform(wlTransformToHyprutils(TRANSFORM), BUFFER_SIZE.x, BUFFER_SIZE.y);
        EXPECT_EQ(transformed.getExtents(), DAMAGE);
    }
}
