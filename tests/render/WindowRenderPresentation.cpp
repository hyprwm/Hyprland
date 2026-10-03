#include <render/WindowRenderPresentation.hpp>
#include <render/scene/SceneSelection.hpp>
#include <helpers/MotionBlur.hpp>
#include <render/pass/SurfacePassElement.hpp>
#include <render/pass/ShadowPassElement.hpp>
#include <render/pass/TransformedWindowPassElement.hpp>

#include <gtest/gtest.h>

using namespace Render;
using Hyprutils::Math::Vector2D;

TEST(WindowRenderPresentation, MonitorPreservesWorkspaceAndTransferAlphaRules) {
    for (const bool present : {false, true}) {
        for (const bool pinned : {false, true}) {
            for (const bool moving : {false, true}) {
                for (const bool visible : {false, true}) {
                    const SWindowPresentationState state{
                        .workspaceOffset          = {37, 23},
                        .floatingOffset           = {7, 11},
                        .workspaceAlpha           = 0.25F,
                        .fade                     = 0.8F,
                        .active                   = 0.75F,
                        .fullscreen               = 0.6F,
                        .layout                   = 0.5F,
                        .moveToWorkspace          = 0.4F,
                        .moveFromWorkspace        = 0.3F,
                        .hasWorkspacePresentation = present,
                        .workspaceOffsetAnimating = true,
                        .pinned                   = pinned,
                        .movingFromMonitor        = moving,
                        .workspaceVisible         = visible,
                    };
                    const auto result   = resolveWindowPresentation(state);
                    const bool transfer = moving && !visible;
                    EXPECT_EQ(result.workspaceOffset, present && !pinned ? state.workspaceOffset : Vector2D{});
                    EXPECT_EQ(result.floatingOffset, present ? state.floatingOffset : Vector2D{});
                    EXPECT_FLOAT_EQ(result.workspaceAlpha, present ? 0.25F : 1.F);
                    EXPECT_FLOAT_EQ(result.alpha, 0.75F);
                    EXPECT_FLOAT_EQ(result.fadeAlpha, 0.8F * 0.6F * 0.5F * (!present || pinned || transfer ? 1.F : 0.25F) * (transfer ? 0.4F : 1.F) * 0.3F);
                    EXPECT_EQ(result.workspaceOffsetAnimating, present);
                    EXPECT_TRUE(result.alphaVisible);
                }
            }
        }
    }
}

TEST(WindowRenderPresentation, MonitorRetainsEarlyWindowAlphaRejection) {
    SWindowPresentationState state;
    state.moveToWorkspace = 0.F;
    // The monitor guard includes transfer channels even when the draw's fade does not.
    auto result = resolveWindowPresentation(state);
    EXPECT_FALSE(result.alphaVisible);
    EXPECT_FLOAT_EQ(result.fadeAlpha, 1.F);
    state.alphaAnimating = true;
    EXPECT_TRUE(resolveWindowPresentation(state).alphaVisible);

    state  = {.workspaceAlpha = 0.F, .hasWorkspacePresentation = true};
    result = resolveWindowPresentation(state);
    EXPECT_TRUE(result.alphaVisible);
    EXPECT_FLOAT_EQ(result.fadeAlpha, 0.F);
}

TEST(WindowRenderPresentation, AbsentAndNeutralWorkspaceRetainDifferentDisplacement) {
    SWindowPresentationState state{.floatingOffset = {7, 11}};
    EXPECT_EQ(resolveWindowPresentation(state).floatingOffset, Vector2D{});
    state.hasWorkspacePresentation = true;
    EXPECT_EQ(resolveWindowPresentation(state).floatingOffset, state.floatingOffset);
}

TEST(WindowRenderPresentation, DeferredCarriersOwnResolvedValues) {
    SWindowPresentationState state{
        .workspaceOffset          = {37, 23},
        .floatingOffset           = {7, 11},
        .workspaceAlpha           = 0.25F,
        .hasWorkspacePresentation = true,
        .workspaceOffsetAnimating = true,
    };
    auto                                 presentation = resolveWindowPresentation(state);
    const auto                           original     = presentation;
    CSurfacePassElement::SRenderData     surface;
    CShadowPassElement::SShadowData      shadow;
    CTransformedWindowPassElement::SData transformed;
    surface.workspacePresentation     = presentation;
    shadow.presentation               = presentation;
    transformed.workspacePresentation = presentation;

    state        = {};
    presentation = resolveWindowPresentation(state);
    EXPECT_NE(presentation, original);
    EXPECT_EQ(surface.workspacePresentation, original);
    EXPECT_EQ(shadow.presentation, original);
    EXPECT_EQ(transformed.workspacePresentation, original);
}

TEST(WindowRenderPresentation, IsolatedModesIgnoreSwitchAndTransferState) {
    for (const auto mode : {eSceneMode::WORKSPACE_WINDOWS, eSceneMode::WORKSPACE_WITH_SHELL}) {
        for (const float workspaceAlpha : {0.F, 0.25F, 1.F}) {
            for (const float moveTo : {0.F, 0.4F, 1.F}) {
                for (const float moveFrom : {0.F, 0.3F, 1.F}) {
                    for (const bool visible : {false, true}) {
                        const SWindowPresentationState state{
                            .workspaceOffset             = {1920, -1080},
                            .floatingOffset              = {37, 23},
                            .workspaceAlpha              = workspaceAlpha,
                            .fade                        = 0.8F,
                            .active                      = 0.75F,
                            .fullscreen                  = 0.6F,
                            .layout                      = 0.5F,
                            .moveToWorkspace             = moveTo,
                            .moveFromWorkspace           = moveFrom,
                            .hasWorkspacePresentation    = true,
                            .workspaceOffsetAnimating    = true,
                            .movingFromMonitor           = true,
                            .workspaceVisible            = visible,
                            .floatingOffsetFromWorkspace = true,
                        };
                        const auto result = resolveWindowPresentation(state, mode);
                        EXPECT_EQ(result.workspaceOffset, Vector2D{});
                        EXPECT_EQ(result.floatingOffset, Vector2D{});
                        EXPECT_FLOAT_EQ(result.workspaceAlpha, 1.F);
                        EXPECT_FLOAT_EQ(result.alpha, 0.75F);
                        EXPECT_FLOAT_EQ(result.fadeAlpha, 0.8F * 0.6F * 0.5F);
                        EXPECT_FALSE(result.workspaceOffsetAnimating);
                        EXPECT_TRUE(result.alphaVisible);
                    }
                }
            }
        }
    }
}

TEST(WindowRenderPresentation, IsolatedModesRetainEachWindowAlphaChannel) {
    for (const auto mode : {eSceneMode::WORKSPACE_WINDOWS, eSceneMode::WORKSPACE_WITH_SHELL}) {
        for (const auto channel : {&SWindowPresentationState::fade, &SWindowPresentationState::active, &SWindowPresentationState::fullscreen, &SWindowPresentationState::layout}) {
            for (const float alpha : {0.F, 0.25F, 1.F}) {
                SWindowPresentationState state{
                    .workspaceAlpha           = 0.F,
                    .moveToWorkspace          = 0.F,
                    .moveFromWorkspace        = 0.F,
                    .hasWorkspacePresentation = true,
                };
                state.*channel    = alpha;
                const auto result = resolveWindowPresentation(state, mode);
                EXPECT_FLOAT_EQ(result.alpha * result.fadeAlpha, alpha);
                EXPECT_EQ(result.alphaVisible, alpha != 0.F);
                state.alphaAnimating = true;
                EXPECT_TRUE(resolveWindowPresentation(state, mode).alphaVisible);
            }
        }
    }
}

TEST(WindowRenderPresentation, IsolatedModesPreserveWindowDisplacementIncludingPinnedWindows) {
    for (const auto mode : {eSceneMode::WORKSPACE_WINDOWS, eSceneMode::WORKSPACE_WITH_SHELL}) {
        for (const bool pinned : {false, true}) {
            SWindowPresentationState state{
                .workspaceOffset          = {1920, 0},
                .floatingOffset           = {7, 11},
                .workspaceAlpha           = 0.25F,
                .hasWorkspacePresentation = true,
                .pinned                   = pinned,
            };
            EXPECT_EQ(resolveWindowPresentation(state, mode).floatingOffset, state.floatingOffset);
            state.floatingOffsetFromWorkspace = true;
            EXPECT_EQ(resolveWindowPresentation(state, mode).floatingOffset, Vector2D{});
            EXPECT_EQ(resolveWindowPresentation(state).floatingOffset, state.floatingOffset);
        }
    }
}

TEST(WindowRenderPresentation, IsolatedResolutionDoesNotChangeLivePresentation) {
    const SWindowPresentationState state{
        .workspaceOffset             = {37, 23},
        .floatingOffset              = {7, 11},
        .workspaceAlpha              = 0.25F,
        .active                      = 0.75F,
        .moveToWorkspace             = 0.F,
        .moveFromWorkspace           = 0.F,
        .hasWorkspacePresentation    = true,
        .workspaceOffsetAnimating    = true,
        .floatingOffsetFromWorkspace = true,
    };
    const auto before   = resolveWindowPresentation(state);
    const auto isolated = resolveWindowPresentation(state, eSceneMode::WORKSPACE_WINDOWS);
    EXPECT_NE(before, isolated);
    EXPECT_EQ(resolveWindowPresentation(state), before);
    EXPECT_EQ(resolveWindowPresentation(state, eSceneMode::WORKSPACE_WITH_SHELL), isolated);
}

TEST(WindowRenderPresentation, MotionBlurKeepsWindowMotionWithoutWorkspaceTranslation) {
    MotionBlur::CTracker tracker;
    tracker.record({100, 200, 300, 400}, {120, 210, 300, 400});
    const SWindowPresentationState state{
        .workspaceOffset          = {1920, 0},
        .floatingOffset           = {7, 11},
        .hasWorkspacePresentation = true,
    };
    const auto live           = resolveWindowPresentation(state);
    const auto isolated       = resolveWindowPresentation(state, eSceneMode::WORKSPACE_WINDOWS);
    const auto liveMotion     = tracker.state(8, live.workspaceOffset + live.floatingOffset, true);
    const auto isolatedMotion = tracker.state(8, isolated.workspaceOffset + isolated.floatingOffset, true);
    ASSERT_TRUE(liveMotion);
    ASSERT_TRUE(isolatedMotion);
    EXPECT_EQ(liveMotion->current.pos() - liveMotion->previous.pos(), isolatedMotion->current.pos() - isolatedMotion->previous.pos());
    EXPECT_EQ(isolatedMotion->previous, CBox(107, 211, 300, 400));
    EXPECT_EQ(isolatedMotion->current, CBox(127, 221, 300, 400));
    const auto after = tracker.state(8, live.workspaceOffset + live.floatingOffset, true);
    ASSERT_TRUE(after);
    EXPECT_EQ(after->previous, liveMotion->previous);
    EXPECT_EQ(after->current, liveMotion->current);
}
