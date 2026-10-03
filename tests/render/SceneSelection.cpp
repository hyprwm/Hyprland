#include <render/scene/SceneSelection.hpp>

#include <gtest/gtest.h>

using namespace Render;

TEST(SceneSelection, ShellIsIndependentOfWorkspaceMembership) {
    EXPECT_TRUE(sceneIncludesShell(eSceneMode::MONITOR));
    EXPECT_FALSE(sceneIncludesShell(eSceneMode::WORKSPACE_WINDOWS));
    EXPECT_TRUE(sceneIncludesShell(eSceneMode::WORKSPACE_WITH_SHELL));
}

TEST(SceneSelection, WorkspaceModesRequireMappedNonHiddenMembers) {
    for (const auto mode : {eSceneMode::WORKSPACE_WINDOWS, eSceneMode::WORKSPACE_WITH_SHELL}) {
        for (const bool mapped : {false, true}) {
            for (const bool hidden : {false, true}) {
                for (const bool member : {false, true}) {
                    for (const bool visible : {false, true}) {
                        const SSceneWindowState window{
                            .mapped             = mapped,
                            .hidden             = hidden,
                            .belongsToWorkspace = member,
                            .monitorVisible     = visible,
                        };
                        EXPECT_EQ(sceneSelectsWindow(mode, window), mapped && !hidden && member)
                            << "shell=" << sceneIncludesShell(mode) << " mapped=" << mapped << " hidden=" << hidden << " member=" << member << " visible=" << visible;
                    }
                }
            }
        }
    }
}

TEST(SceneSelection, MonitorModePreservesVisibilitySelection) {
    for (const bool mapped : {false, true}) {
        for (const bool hidden : {false, true}) {
            for (const bool member : {false, true}) {
                for (const bool visible : {false, true}) {
                    const SSceneWindowState window{
                        .mapped             = mapped,
                        .hidden             = hidden,
                        .belongsToWorkspace = member,
                        .monitorVisible     = visible,
                    };
                    EXPECT_EQ(sceneSelectsWindow(eSceneMode::MONITOR, window), visible);
                }
            }
        }
    }
}

TEST(SceneSelection, MissingMembershipIsNotEligible) {
    for (const auto mode : {eSceneMode::WORKSPACE_WINDOWS, eSceneMode::WORKSPACE_WITH_SHELL}) {
        EXPECT_FALSE(sceneSelectsWindow(mode, {}));
        EXPECT_FALSE(sceneSelectsWindow(mode, {.mapped = true, .monitorVisible = true}));
    }
}
