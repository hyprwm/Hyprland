#include <overview/hyprland/scene/OverviewScene.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace Overview::Hyprland;

TEST(Overview, workspaceSearchMatchesASCIIInsensitiveSubstrings) {
    EXPECT_TRUE(matchesName("Alpha", "alp"));
    EXPECT_TRUE(matchesName("Alpha", "PHA"));
    EXPECT_TRUE(matchesName("workspace 10", "1"));
    EXPECT_TRUE(matchesName("anything", ""));
    EXPECT_FALSE(matchesName("Alpha", "beta"));
}

TEST(Overview, workspaceSearchLeavesNonASCIICaseSensitive) {
    EXPECT_TRUE(matchesName("Ärger", "Är"));
    EXPECT_FALSE(matchesName("Ärger", "är"));
}
