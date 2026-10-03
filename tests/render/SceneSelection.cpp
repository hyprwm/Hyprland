#include <render/scene/SceneSelection.hpp>
#include <desktop/state/Fadeout.hpp>

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

TEST(SceneSelection, WindowFadeoutsRequireMatchingWorkspaceInBothModes) {
    for (const auto mode : {eSceneMode::WORKSPACE_WINDOWS, eSceneMode::WORKSPACE_WITH_SHELL}) {
        EXPECT_TRUE(sceneSelectsFadeout(mode, Desktop::eFadeoutSource::WINDOW, true));
        EXPECT_FALSE(sceneSelectsFadeout(mode, Desktop::eFadeoutSource::WINDOW, false));
    }
}

TEST(SceneSelection, LayerFadeoutsRequireShellMode) {
    for (const bool member : {false, true}) {
        EXPECT_FALSE(sceneSelectsFadeout(eSceneMode::WORKSPACE_WINDOWS, Desktop::eFadeoutSource::LAYER, member));
        EXPECT_TRUE(sceneSelectsFadeout(eSceneMode::WORKSPACE_WITH_SHELL, Desktop::eFadeoutSource::LAYER, member));
    }
}

TEST(SceneSelection, UnknownFadeoutSourcesAreExcluded) {
    for (const auto mode : {eSceneMode::WORKSPACE_WINDOWS, eSceneMode::WORKSPACE_WITH_SHELL}) {
        for (const bool member : {false, true}) {
            EXPECT_FALSE(sceneSelectsFadeout(mode, Desktop::eFadeoutSource::UNKNOWN, member));
        }
    }
}

TEST(SceneSelection, MonitorFadeoutPolicyAddsNoRestrictions) {
    for (const auto source : {Desktop::eFadeoutSource::UNKNOWN, Desktop::eFadeoutSource::WINDOW, Desktop::eFadeoutSource::LAYER}) {
        for (const bool member : {false, true}) {
            EXPECT_TRUE(sceneSelectsFadeout(eSceneMode::MONITOR, source, member));
        }
    }
}

class CSourceTestFadeout : public Desktop::IFadeout {
  public:
    explicit CSourceTestFadeout(Desktop::eFadeoutPlane plane) : m_plane(plane) {
        ;
    }

    PHLMONITORREF monitor() const override {
        return {};
    }
    Desktop::eFadeoutPlane plane() const override {
        return m_plane;
    }
    int zIndex() const override {
        return 0;
    }
    CBox renderBox() const override {
        return {};
    }
    float alpha() const override {
        return 1.F;
    }
    bool done() const override {
        return false;
    }

  private:
    Desktop::eFadeoutPlane m_plane;
};

TEST(SceneSelection, UnownedWindowFadeoutsDoNotBecomeShell) {
    for (const auto plane : {Desktop::FADEOUT_PLANE_WINDOW_TILED, Desktop::FADEOUT_PLANE_WINDOW_FLOATING, Desktop::FADEOUT_PLANE_WINDOW_OVER_FULLSCREEN}) {
        const CSourceTestFadeout fadeout{plane};
        const auto               source = fadeout.source();
        EXPECT_EQ(source.type, Desktop::eFadeoutSource::WINDOW);
        EXPECT_FALSE(source.workspace);
        EXPECT_FALSE(sceneSelectsFadeout(eSceneMode::WORKSPACE_WITH_SHELL, source.type, false));
    }
}

TEST(SceneSelection, FadeoutPlanesDistinguishLayersFromUnclassifiedPopups) {
    for (const auto plane :
         {Desktop::FADEOUT_PLANE_LAYER_BACKGROUND, Desktop::FADEOUT_PLANE_LAYER_BOTTOM, Desktop::FADEOUT_PLANE_LAYER_TOP, Desktop::FADEOUT_PLANE_LAYER_OVERLAY}) {
        const CSourceTestFadeout fadeout{plane};
        EXPECT_EQ(fadeout.source().type, Desktop::eFadeoutSource::LAYER);
    }

    const CSourceTestFadeout popup{Desktop::FADEOUT_PLANE_POPUP};
    EXPECT_EQ(popup.source().type, Desktop::eFadeoutSource::UNKNOWN);
    EXPECT_FALSE(popup.source().workspace);
}
