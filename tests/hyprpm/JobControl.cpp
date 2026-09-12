#include "../../hyprpm/src/helpers/JobControl.hpp"

#include <gtest/gtest.h>

#include <limits>

using namespace NJobControl;

TEST(HyprpmJobControl, ParsesOnlySupportedRange) {
    EXPECT_EQ(parse("1"), 1);
    EXPECT_EQ(parse("256"), 256);
    EXPECT_FALSE(parse(""));
    EXPECT_FALSE(parse("0"));
    EXPECT_FALSE(parse("-1"));
    EXPECT_FALSE(parse("+1"));
    EXPECT_FALSE(parse(" 1"));
    EXPECT_FALSE(parse("1 "));
    EXPECT_FALSE(parse("1x"));
    EXPECT_FALSE(parse("257"));
    EXPECT_FALSE(parse(std::to_string(std::numeric_limits<size_t>::max()) + "0"));
}

TEST(HyprpmJobControl, PreservesDefaultBuildCommand) {
    EXPECT_TRUE(buildEnvironment(0).empty());
    EXPECT_EQ(pluginBuildCommand("/tmp/plugin", "PKG_CONFIG_PATH=\"/tmp/pkgconfig\"", "first && second", 0),
              "cd /tmp/plugin && PKG_CONFIG_PATH=\"/tmp/pkgconfig\" first && second");
    EXPECT_EQ(headersInstallCommand("/tmp/source", "/tmp/headers", 0),
              "make -C '/tmp/source' installheaders && chmod -R 644 '/tmp/headers' && find '/tmp/headers' -type d -exec chmod a+x {} \\;");
}

TEST(HyprpmJobControl, AppliesLimitToEntireBuildStepAndHeaders) {
    const auto ENVIRONMENT = buildEnvironment(4) + "PKG_CONFIG_PATH=\"/tmp/pkgconfig\"";

    EXPECT_EQ(ENVIRONMENT, "export CMAKE_BUILD_PARALLEL_LEVEL=4 MAKEFLAGS=-j4; PKG_CONFIG_PATH=\"/tmp/pkgconfig\"");
    EXPECT_EQ(pluginBuildCommand("/tmp/plugin", ENVIRONMENT, "first && second", 4),
              "cd /tmp/plugin && (export CMAKE_BUILD_PARALLEL_LEVEL=4 MAKEFLAGS=-j4; PKG_CONFIG_PATH=\"/tmp/pkgconfig\" first && second)");
    EXPECT_EQ(headersInstallCommand("/tmp/source", "/tmp/headers", 4),
              "make -j4 -C '/tmp/source' installheaders && chmod -R 644 '/tmp/headers' && find '/tmp/headers' -type d -exec chmod a+x {} \\;");
}
