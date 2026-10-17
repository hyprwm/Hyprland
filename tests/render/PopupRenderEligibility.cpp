#include <render/scene/SceneSelection.hpp>
#include <render/WindowRenderPresentation.hpp>

#include <gtest/gtest.h>

using namespace Render;

TEST(PopupRenderEligibility, WorkspaceRenderingDoesNotRequireInputEligibility) {
    const SScenePopupState popup{
        .mapped       = true,
        .hasResource  = true,
        .alphaVisible = true,
        .acceptsInput = false,
    };
    for (const auto mode : {eSceneMode::WORKSPACE_WINDOWS, eSceneMode::WORKSPACE_WITH_SHELL}) {
        const auto presentation = resolveWindowPresentation({.workspaceAlpha = 0.F, .hasWorkspacePresentation = true, .workspaceVisible = false}, mode);
        EXPECT_TRUE(presentation.workspaceScene);
        EXPECT_TRUE(sceneSelectsPopup(presentation.workspaceScene, popup));
    }
    const auto monitor = resolveWindowPresentation({});
    EXPECT_FALSE(monitor.workspaceScene);
    EXPECT_FALSE(sceneSelectsPopup(monitor.workspaceScene, popup));
}

TEST(PopupRenderEligibility, WorkspaceRenderingStillRequiresMappedResourceAlphaAndNonInertPopup) {
    for (const bool mapped : {false, true}) {
        for (const bool resource : {false, true}) {
            for (const bool alpha : {false, true}) {
                for (const bool inert : {false, true}) {
                    for (const bool input : {false, true}) {
                        const SScenePopupState popup{
                            .mapped       = mapped,
                            .hasResource  = resource,
                            .alphaVisible = alpha,
                            .inert        = inert,
                            .acceptsInput = input,
                        };
                        EXPECT_EQ(sceneSelectsPopup(true, popup), mapped && resource && alpha && !inert);
                        EXPECT_EQ(sceneSelectsPopup(false, popup), mapped && resource && alpha && input);
                    }
                }
            }
        }
    }
}

TEST(PopupRenderEligibility, DefaultPresentationKeepsMonitorPolicy) {
    EXPECT_FALSE(SWindowRenderPresentation{}.workspaceScene);
    EXPECT_FALSE(resolveWindowPresentation({}).workspaceScene);
    EXPECT_FALSE(resolveWindowPresentation({}, eSceneMode::MONITOR).workspaceScene);
}
