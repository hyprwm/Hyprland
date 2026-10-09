#include <render/scene/SceneSelection.hpp>
#include <render/Context.hpp>
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

TEST(SceneSelection, ShellIncludesOnlyMappedNonHiddenFloatingPinnedWindowsOnTheirOwnerMonitor) {
    for (const bool mapped : {false, true}) {
        for (const bool hidden : {false, true}) {
            for (const bool floating : {false, true}) {
                for (const bool pinned : {false, true}) {
                    for (const bool ownerMonitor : {false, true}) {
                        const SSceneWindowState window{
                            .mapped         = mapped,
                            .hidden         = hidden,
                            .floating       = floating,
                            .pinned         = pinned,
                            .onOwnerMonitor = ownerMonitor,
                        };
                        EXPECT_EQ(sceneSelectsWindow(eSceneMode::WORKSPACE_WITH_SHELL, window), mapped && !hidden && floating && pinned && ownerMonitor);
                        EXPECT_FALSE(sceneSelectsWindow(eSceneMode::WORKSPACE_WINDOWS, window));
                        EXPECT_FALSE(sceneSelectsWindow(eSceneMode::MONITOR, window));
                    }
                }
            }
        }
    }
}

TEST(SceneSelection, PinnedWorkspaceMembersStillRequireMappingAndNonHiddenState) {
    for (const auto mode : {eSceneMode::WORKSPACE_WINDOWS, eSceneMode::WORKSPACE_WITH_SHELL}) {
        for (const bool mapped : {false, true}) {
            for (const bool hidden : {false, true}) {
                const SSceneWindowState window{
                    .mapped             = mapped,
                    .hidden             = hidden,
                    .belongsToWorkspace = true,
                    .floating           = true,
                    .pinned             = true,
                    .onOwnerMonitor     = true,
                };
                EXPECT_EQ(sceneSelectsWindow(mode, window), mapped && !hidden);
            }
        }
    }
}

TEST(SceneSelection, PinnedWindowAndPopupOriginsSurviveWorkspaceChanges) {
    for (const bool member : {false, true}) {
        EXPECT_TRUE(sceneSelectsFadeout(eSceneMode::WORKSPACE_WITH_SHELL, Desktop::eFadeoutSource::WINDOW, member, true));
        EXPECT_EQ(sceneSelectsFadeout(eSceneMode::WORKSPACE_WINDOWS, Desktop::eFadeoutSource::WINDOW, member, true), member);
        EXPECT_TRUE(sceneSelectsFadeout(eSceneMode::MONITOR, Desktop::eFadeoutSource::WINDOW, member, true));
        EXPECT_FALSE(sceneSelectsFadeout(eSceneMode::WORKSPACE_WITH_SHELL, Desktop::eFadeoutSource::UNKNOWN, member, true));
    }
}

TEST(SceneSelection, PinnedFadeoutsUseOnePassRegardlessOfSourceOrTargetFullscreenState) {
    for (const auto plane : {Desktop::FADEOUT_PLANE_WINDOW_FLOATING, Desktop::FADEOUT_PLANE_WINDOW_OVER_FULLSCREEN}) {
        for (const auto mode : {eSceneMode::WORKSPACE_WINDOWS, eSceneMode::WORKSPACE_WITH_SHELL}) {
            EXPECT_EQ(sceneFadeoutPlane(mode, plane, true), Desktop::FADEOUT_PLANE_WINDOW_PINNED);
            EXPECT_EQ(sceneFadeoutPlane(mode, plane, false), plane);
        }
        EXPECT_EQ(sceneFadeoutPlane(eSceneMode::MONITOR, plane, true), plane);
    }
    for (const auto mode : {eSceneMode::MONITOR, eSceneMode::WORKSPACE_WINDOWS, eSceneMode::WORKSPACE_WITH_SHELL}) {
        EXPECT_EQ(sceneFadeoutPlane(mode, Desktop::FADEOUT_PLANE_POPUP, true), Desktop::FADEOUT_PLANE_POPUP);
        EXPECT_EQ(sceneFadeoutPlane(mode, Desktop::FADEOUT_PLANE_WINDOW_TILED, true), Desktop::FADEOUT_PLANE_WINDOW_TILED);
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
    explicit CSourceTestFadeout(Desktop::eFadeoutPlane plane, const Desktop::SFadeoutSource& source = {}) : m_plane(plane) {
        m_source = source;
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

TEST(SceneSelection, StoredOriginPreservesPinnedWindowAndPopupSelectionWithoutALiveOwner) {
    for (const auto plane : {Desktop::FADEOUT_PLANE_WINDOW_FLOATING, Desktop::FADEOUT_PLANE_WINDOW_OVER_FULLSCREEN, Desktop::FADEOUT_PLANE_POPUP}) {
        const CSourceTestFadeout fadeout{plane, {.type = Desktop::eFadeoutSource::WINDOW, .pinned = true}};
        const auto               source = fadeout.source();
        EXPECT_EQ(source.type, Desktop::eFadeoutSource::WINDOW);
        EXPECT_TRUE(source.pinned);
        EXPECT_FALSE(source.workspace);
        EXPECT_TRUE(sceneSelectsFadeout(eSceneMode::WORKSPACE_WITH_SHELL, source.type, false, source.pinned));
        EXPECT_FALSE(sceneSelectsFadeout(eSceneMode::WORKSPACE_WINDOWS, source.type, false, source.pinned));
    }
}

TEST(SceneSelection, FadeoutPrivacyPersistsForWindowLayerAndPopupOrigins) {
    CRenderContext ctx;
    for (const auto type : {Desktop::eFadeoutSource::WINDOW, Desktop::eFadeoutSource::LAYER}) {
        for (const auto plane : {Desktop::FADEOUT_PLANE_WINDOW_FLOATING, Desktop::FADEOUT_PLANE_LAYER_TOP, Desktop::FADEOUT_PLANE_POPUP}) {
            for (const bool noScreenShare : {false, true}) {
                Desktop::SFadeoutSource  origin{.type = type, .noScreenShare = noScreenShare};
                const CSourceTestFadeout fadeout{plane, origin};
                origin = {};
                EXPECT_EQ(fadeout.source().type, type);
                EXPECT_EQ(fadeout.source().noScreenShare, noScreenShare);
                ctx.m_renderingCapture = true;
                EXPECT_EQ(ctx.shouldRenderContent(fadeout.source().noScreenShare), !noScreenShare);
                ctx.m_renderingCapture = false;
                EXPECT_TRUE(ctx.shouldRenderContent(fadeout.source().noScreenShare));
            }
        }
    }
}
