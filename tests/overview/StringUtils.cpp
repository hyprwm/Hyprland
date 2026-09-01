#include <overview/hyprland/StringUtils.hpp>

#include <gtest/gtest.h>

using namespace Overview::Hyprland;
using namespace Overview::Hyprland::StringUtils;

TEST(Overview, searchMatchesASCIIInsensitiveSubstrings) {
    EXPECT_TRUE(matchesName("Alpha", "alp"));
    EXPECT_TRUE(matchesName("Alpha", "PHA"));
    EXPECT_TRUE(matchesName("workspace 10", "1"));
    EXPECT_TRUE(matchesName("anything", ""));
    EXPECT_FALSE(matchesName("Alpha", "beta"));
}

TEST(Overview, searchLeavesNonASCIICaseSensitive) {
    EXPECT_TRUE(matchesName("Ärger", "Är"));
    EXPECT_FALSE(matchesName("Ärger", "är"));
}

TEST(Overview, searchMatchesInsensitive) {
    EXPECT_TRUE(fullMatchCaseIns("pEeNoR", "PEENOR"));
    EXPECT_FALSE(fullMatchCaseIns("FUCKERS", "fucker"));
}
