#include <render/WindowRenderPresentation.hpp>
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
