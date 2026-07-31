#include <render/gl/blur/Kawase.hpp>
#include <render/gl/blur/Glass.hpp>
#include <render/gl/blur/FluidJar.hpp>
#include <render/gl/blur/HeatShimmer.hpp>
#include <render/gl/blur/Prism.hpp>
#include <render/gl/blur/Ripple.hpp>
#include <render/gl/blur/Water.hpp>
#include <render/ShaderLoader.hpp>

#include <gtest/gtest.h>

using namespace Render::GL;

TEST(BlurMaterial, DefaultUsesPlainFinish) {
    const CDefaultBlurMaterial material;
    const auto                 requirements = material.requirements();

    EXPECT_EQ(material.type(), Render::eBlurType::BLUR_DUAL_KAWASE);
    EXPECT_EQ(requirements.finishFragment, Render::SH_FRAG_BLURFINISH);
    EXPECT_FALSE(requirements.preparedInput);
    EXPECT_FALSE(requirements.liveBlur);
    EXPECT_FALSE(material.isAnimated());
    EXPECT_EQ(material.blurSizeForDamage(100), 40);
    EXPECT_FLOAT_EQ(material.sampleRadius(), 0.F);
}

TEST(BlurMaterial, GlassCapabilitiesAreConfiguredByMaterial) {
    const CGlassBlurMaterial frost(Render::eBlurType::BLUR_FROST, Render::SH_FRAG_FROSTFINISH);
    const auto               frostRequirements = frost.requirements();
    EXPECT_EQ(frost.type(), Render::eBlurType::BLUR_FROST);
    EXPECT_EQ(frostRequirements.finishFragment, Render::SH_FRAG_FROSTFINISH);
    EXPECT_FALSE(frostRequirements.preparedInput);

    const CPrismBlurMaterial prism;
    const auto               prismRequirements = prism.requirements();
    EXPECT_EQ(prism.type(), Render::eBlurType::BLUR_PRISM);
    EXPECT_EQ(prismRequirements.finishFragment, Render::SH_FRAG_PRISMFINISH);
    EXPECT_TRUE(prismRequirements.preparedInput);
}

TEST(BlurMaterial, HeatShimmerUsesAnimatedGlassFinish) {
    const CHeatShimmerBlurMaterial heatShimmer;
    const auto                     requirements = heatShimmer.requirements();

    EXPECT_EQ(heatShimmer.type(), Render::eBlurType::BLUR_HEAT_SHIMMER);
    EXPECT_EQ(requirements.finishFragment, Render::SH_FRAG_HEATSHIMMERFINISH);
    EXPECT_FALSE(requirements.preparedInput);
    EXPECT_FALSE(requirements.liveBlur);
}

TEST(BlurDamage, DualKawaseUsesOperationalMinimums) {
    EXPECT_FLOAT_EQ(dualKawaseDamageRadius(0, 0), 2.F);
    EXPECT_FLOAT_EQ(dualKawaseDamageRadius(-10, -10), 2.F);
}

TEST(BlurDamage, DualKawaseCalculatesConfiguredRadius) {
    EXPECT_FLOAT_EQ(dualKawaseDamageRadius(8, 1), 16.F);
    EXPECT_FLOAT_EQ(dualKawaseDamageRadius(8, 2), 48.F);
    EXPECT_FLOAT_EQ(dualKawaseDamageRadius(12, 3), 168.F);
}

TEST(BlurDamage, DualKawaseUsesOperationalMaximums) {
    EXPECT_FLOAT_EQ(dualKawaseDamageRadius(40, 8), 20400.F);
    EXPECT_FLOAT_EQ(dualKawaseDamageRadius(100, 10), 51000.F);
}

TEST(BlurDamage, GlassIncludesRefractionReach) {
    EXPECT_FLOAT_EQ(glassDamageRadius(8, 1, 3.F), 19.F);
    EXPECT_FLOAT_EQ(glassDamageRadius(12, 3, 4.25F), 173.F);
}

TEST(BlurDamage, GlassClampsRefractionReach) {
    EXPECT_FLOAT_EQ(glassDamageRadius(8, 1, -1.F), 16.F);
    EXPECT_FLOAT_EQ(glassDamageRadius(8, 1, 100.F), 36.F);
}

TEST(BlurDamage, RippleIncludesBoundedDisplacement) {
    EXPECT_FLOAT_EQ(rippleDamageRadius(8, 1, 6.F), 22.F);
    EXPECT_FLOAT_EQ(rippleDamageRadius(8, 1, -1.F), 16.F);
    EXPECT_FLOAT_EQ(rippleDamageRadius(8, 1, 100.F), 48.F);
}

TEST(BlurDamage, RippleOutputReachIncludesWaveWidth) {
    EXPECT_FLOAT_EQ(rippleOutputReach(180.F, 24.F), 204.F);
    EXPECT_FLOAT_EQ(rippleOutputReach(10.25F, 2.25F), 13.F);
    EXPECT_FLOAT_EQ(rippleOutputReach(-1.F, -1.F), 0.F);
}

TEST(BlurDamage, WaterIncludesBoundedDisplacement) {
    EXPECT_FLOAT_EQ(waterDamageRadius(8, 1, 6.F), 22.F);
    EXPECT_FLOAT_EQ(waterDamageRadius(8, 1, -1.F), 16.F);
    EXPECT_FLOAT_EQ(waterDamageRadius(8, 1, 100.F), 48.F);
}

TEST(FluidJar, SimulationSizePreservesAspectAndBoundsParticles) {
    const auto wide = fluidJarSimulationSize({1920, 1080});
    EXPECT_EQ(wide, Vector2D(256, 144));
    EXPECT_LE(fluidJarParticleCapacity(wide), 2048);

    const auto square = fluidJarSimulationSize({1000, 1000});
    EXPECT_EQ(square, Vector2D(250, 250));
    EXPECT_LE(fluidJarParticleCapacity(square), 2048);
}

TEST(FluidJar, DamageIncludesBoundedRefraction) {
    EXPECT_FLOAT_EQ(fluidJarDamageRadius(8, 1), 24.F);
    EXPECT_FLOAT_EQ(fluidJarDamageRadius(8, 2), 56.F);
    EXPECT_FLOAT_EQ(fluidJarDamageRadius(8, 1, 0.F), 16.F);
    EXPECT_FLOAT_EQ(fluidJarDamageRadius(8, 1, 2.F), 32.F);
    EXPECT_FLOAT_EQ(fluidJarDamageRadius(8, 1, 4.F), 48.F);
    EXPECT_FLOAT_EQ(fluidJarDamageRadius(8, 1, 10.F), 96.F);
}

TEST(FluidJar, OutputTransformsMapFramebufferToLogicalCoordinates) {
    const std::array<SFluidJarOutputTransform, 8> expected = {
        SFluidJarOutputTransform{.xAxis = {1, 0}, .yAxis = {0, 1}, .offset = {0, 0}},   SFluidJarOutputTransform{.xAxis = {0, -1}, .yAxis = {1, 0}, .offset = {0, 1}},
        SFluidJarOutputTransform{.xAxis = {-1, 0}, .yAxis = {0, -1}, .offset = {1, 1}}, SFluidJarOutputTransform{.xAxis = {0, 1}, .yAxis = {-1, 0}, .offset = {1, 0}},
        SFluidJarOutputTransform{.xAxis = {-1, 0}, .yAxis = {0, 1}, .offset = {1, 0}},  SFluidJarOutputTransform{.xAxis = {0, 1}, .yAxis = {1, 0}, .offset = {0, 0}},
        SFluidJarOutputTransform{.xAxis = {1, 0}, .yAxis = {0, -1}, .offset = {0, 1}},  SFluidJarOutputTransform{.xAxis = {0, -1}, .yAxis = {-1, 0}, .offset = {1, 1}},
    };

    for (size_t i = 0; i < expected.size(); ++i) {
        const auto transform = fluidJarOutputTransform(sc<eTransform>(i));
        EXPECT_EQ(transform.xAxis, expected[i].xAxis);
        EXPECT_EQ(transform.yAxis, expected[i].yAxis);
        EXPECT_EQ(transform.offset, expected[i].offset);
    }
}

TEST(FluidJar, OutputTransformsKeepTheFloorAtLogicalBottom) {
    const std::array<Vector2D, 8> outputBottomCenters = {
        Vector2D{0.5, 1.0}, Vector2D{0.0, 0.5}, Vector2D{0.5, 0.0}, Vector2D{1.0, 0.5}, Vector2D{0.5, 1.0}, Vector2D{1.0, 0.5}, Vector2D{0.5, 0.0}, Vector2D{0.0, 0.5},
    };

    for (size_t i = 0; i < outputBottomCenters.size(); ++i) {
        const auto transform = fluidJarOutputTransform(sc<eTransform>(i));
        const auto point     = outputBottomCenters[i];
        const auto logical   = transform.xAxis * point.x + transform.yAxis * point.y + transform.offset;
        EXPECT_EQ(logical, Vector2D(0.5, 1.0));
    }
}

TEST(FluidJar, OutputTransformVectorsRoundTrip) {
    constexpr Vector2D LOGICAL_VECTOR = {0.25, -0.75};

    for (int i = 0; i < 8; ++i) {
        const auto     transform    = fluidJarOutputTransform(sc<eTransform>(i));
        const Vector2D outputVector = {
            transform.xAxis.x * LOGICAL_VECTOR.x + transform.xAxis.y * LOGICAL_VECTOR.y,
            transform.yAxis.x * LOGICAL_VECTOR.x + transform.yAxis.y * LOGICAL_VECTOR.y,
        };
        const auto logicalVector = transform.xAxis * outputVector.x + transform.yAxis * outputVector.y;
        EXPECT_EQ(logicalVector, LOGICAL_VECTOR);
    }
}

TEST(FluidJar, SimulationSizeRejectsEmptyExtents) {
    EXPECT_EQ(fluidJarSimulationSize({0, 100}), Vector2D());
    EXPECT_EQ(fluidJarSimulationSize({100, 0}), Vector2D());
}

TEST(FluidJar, PrecisionScalesSimulationAndClamps) {
    EXPECT_EQ(fluidJarSimulationSize({1920, 1080}, 0.5F), Vector2D(128, 72));
    EXPECT_EQ(fluidJarSimulationSize({1920, 1080}, 2.F), Vector2D(512, 288));
    EXPECT_EQ(fluidJarSimulationSize({1920, 1080}, 4.F), Vector2D(1024, 576));
    EXPECT_EQ(fluidJarSimulationSize({1920, 1080}, 8.F), Vector2D(2048, 1152));
    EXPECT_EQ(fluidJarSimulationSize({1920, 1080}, 0.1F), Vector2D(128, 72));
    EXPECT_EQ(fluidJarSimulationSize({1920, 1080}, 20.F), Vector2D(2048, 1152));
}

TEST(FluidJar, MaximumPrecisionBoundsParticleCapacity) {
    const auto size = fluidJarSimulationSize({4096, 4096}, 8.F);
    EXPECT_EQ(size, Vector2D(2048, 2048));
    EXPECT_EQ(fluidJarParticleCapacity(size), 131072);
}

TEST(FluidJar, FillAmountControlsInitialParticles) {
    const Vector2D size = {256, 144};
    EXPECT_EQ(fluidJarParticleCapacity(size), 1152);
    EXPECT_EQ(fluidJarInitialParticleCount(size, 0.4F), 460);
    EXPECT_EQ(fluidJarInitialParticleCount(size, -1.F), 0);
    EXPECT_EQ(fluidJarInitialParticleCount(size, 2.F), 1152);
}

TEST(FluidJar, ResizePreservesParticles) {
    EXPECT_EQ(fluidJarResizedParticleCount(460, {256, 144}), 460);
    EXPECT_EQ(fluidJarResizedParticleCount(460, {64, 64}), 460);
    EXPECT_EQ(fluidJarResizedParticleCount(64, {256, 144}), 64);
}

TEST(FluidJar, GeometryTransformPreservesWorldPositionDuringMove) {
    const auto transform = fluidJarGeometryTransform({100, 200, 800, 600}, {140, 220, 800, 600}, {200, 150}, {200, 150});
    EXPECT_EQ(transform.positionScale, Vector2D(1, 1));
    EXPECT_EQ(transform.positionOffset, Vector2D(-10, 5));
    EXPECT_EQ(transform.velocityScale, Vector2D(1, 1));
}

TEST(FluidJar, GeometryTransformPreservesStationaryResizeEdges) {
    const auto right = fluidJarGeometryTransform({0, 0, 800, 600}, {0, 0, 1000, 600}, {200, 150}, {250, 150});
    EXPECT_EQ(right.positionScale, Vector2D(1, 1));
    EXPECT_EQ(right.positionOffset, Vector2D(0, 0));

    const auto left = fluidJarGeometryTransform({0, 0, 800, 600}, {-200, 0, 1000, 600}, {200, 150}, {250, 150});
    EXPECT_EQ(left.positionScale, Vector2D(1, 1));
    EXPECT_EQ(left.positionOffset, Vector2D(50, 0));

    const auto bottom = fluidJarGeometryTransform({0, 0, 800, 600}, {0, 0, 800, 800}, {200, 150}, {200, 200});
    EXPECT_EQ(bottom.positionScale, Vector2D(1, 1));
    EXPECT_EQ(bottom.positionOffset, Vector2D(0, 50));
}

TEST(FluidJar, DiscontinuousGeometryFollowsContainer) {
    const auto transform = fluidJarGeometryTransform({0, 0, 800, 600}, {1200, 400, 1000, 800}, {200, 150}, {250, 200}, false);
    EXPECT_EQ(transform.positionScale, Vector2D(1.25, 4.0 / 3.0));
    EXPECT_EQ(transform.positionOffset, Vector2D(0, 0));
    EXPECT_EQ(transform.velocityScale, Vector2D(0, 0));
}

TEST(FluidJar, WallVelocityUsesSimulationCoordinates) {
    const auto moved = fluidJarWallVelocities({0, 0, 800, 600}, {6, 6, 800, 600}, {200, 150}, 1.F / 60.F);
    EXPECT_FLOAT_EQ(moved[0], 0.5F);
    EXPECT_FLOAT_EQ(moved[1], 0.5F);
    EXPECT_FLOAT_EQ(moved[2], -0.5F);

    const auto fasterSimulation = fluidJarWallVelocities({0, 0, 800, 600}, {6, 6, 800, 600}, {200, 150}, 1.F / 60.F, 2.F);
    EXPECT_FLOAT_EQ(fasterSimulation[0], 0.25F);
    EXPECT_FLOAT_EQ(fasterSimulation[1], 0.25F);
    EXPECT_FLOAT_EQ(fasterSimulation[2], -0.25F);

    const auto resized = fluidJarWallVelocities({0, 0, 800, 600}, {0, 0, 806, 600}, {200, 150}, 1.F / 60.F);
    EXPECT_FLOAT_EQ(resized[0], 0.F);
    EXPECT_NEAR(resized[1], 6.F * (200.F / 806.F) / 3.F, 0.0001F);
    EXPECT_FLOAT_EQ(resized[2], 0.F);
}

TEST(FluidJar, WallVelocityRejectsInvalidIntervalsAndClampsSpikes) {
    EXPECT_EQ(fluidJarWallVelocities({0, 0, 800, 600}, {10, 0, 800, 600}, {200, 150}, 0.F), (std::array<float, 4>{}));

    const auto velocity = fluidJarWallVelocities({0, 0, 800, 600}, {1000, 0, 800, 600}, {200, 150}, 1.F / 60.F);
    EXPECT_FLOAT_EQ(velocity[0], 1.25F);
    EXPECT_FLOAT_EQ(velocity[1], 1.25F);

    EXPECT_EQ(fluidJarWallVelocities({0, 0, 800, 600}, {10, 0, 800, 600}, {200, 150}, 1.F / 60.F, 0.F), (std::array<float, 4>{}));
}
