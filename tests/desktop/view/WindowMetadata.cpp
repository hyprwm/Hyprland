#include <desktop/view/window/WindowMetadata.hpp>

#include <gtest/gtest.h>

using namespace Desktop::View;

TEST(WindowMetadata, FirstMapPreservesCurrentAppID) {
    CWindowMetadata metadata;

    EXPECT_TRUE(metadata.updateTitle("pre-map title"));
    EXPECT_TRUE(metadata.updateAppID("pre-map app"));

    metadata.initializeOnFirstMap("mapped title", "mapped app");

    EXPECT_EQ(metadata.title(), "mapped title");
    EXPECT_EQ(metadata.appID(), "pre-map app");
    EXPECT_EQ(metadata.initialTitle(), "mapped title");
    EXPECT_EQ(metadata.initialAppID(), "mapped app");
}

TEST(WindowMetadata, UpdatesReportChanges) {
    CWindowMetadata metadata;

    EXPECT_FALSE(metadata.updateTitle(""));
    EXPECT_TRUE(metadata.updateTitle("title"));
    EXPECT_FALSE(metadata.updateTitle("title"));
    EXPECT_EQ(metadata.title(), "title");

    EXPECT_FALSE(metadata.updateAppID(""));
    EXPECT_TRUE(metadata.updateAppID("app"));
    EXPECT_FALSE(metadata.updateAppID("app"));
    EXPECT_EQ(metadata.appID(), "app");
}

TEST(WindowMetadata, StableIDsAreUniqueAndSequential) {
    const CWindowMetadata first;
    const CWindowMetadata second;

    EXPECT_EQ(second.stableID(), first.stableID() + 1);
}
