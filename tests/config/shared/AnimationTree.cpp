#include <config/shared/animation/AnimationTree.hpp>

#include <gtest/gtest.h>

TEST(AnimationTree, HasOverviewAnimations) {
    const auto& tree = Config::animationTree();

    EXPECT_TRUE(tree->nodeExists("overview"));
    EXPECT_TRUE(tree->nodeExists("overviewIn"));
    EXPECT_TRUE(tree->nodeExists("overviewOut"));
    EXPECT_TRUE(tree->nodeExists("overviewMove"));
    EXPECT_TRUE(tree->nodeExists("overviewFade"));
}
