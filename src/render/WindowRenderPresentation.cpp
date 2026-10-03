#include "WindowRenderPresentation.hpp"
#include "scene/SceneSelection.hpp"

Render::SWindowRenderPresentation Render::resolveWindowPresentation(const SWindowPresentationState& state) {
    return resolveWindowPresentation(state, eSceneMode::MONITOR);
}

Render::SWindowRenderPresentation Render::resolveWindowPresentation(const SWindowPresentationState& state, eSceneMode mode) {
    if (mode != eSceneMode::MONITOR) {
        const auto FADE = state.fade * state.fullscreen * state.layout;
        return {
            .floatingOffset = state.floatingOffsetFromWorkspace ? Hyprutils::Math::Vector2D{} : state.floatingOffset,
            .alpha          = state.active,
            .fadeAlpha      = FADE,
            .alphaVisible   = state.fade * state.active * state.fullscreen * state.layout != 0.F || state.alphaAnimating,
        };
    }

    const bool TRANSFERRING = state.movingFromMonitor && !state.workspaceVisible;
    const auto TOTAL_ALPHA  = state.fade * state.active * state.fullscreen * state.layout * state.moveToWorkspace * state.moveFromWorkspace;

    return {
        .workspaceOffset = state.hasWorkspacePresentation && !state.pinned ? state.workspaceOffset : Hyprutils::Math::Vector2D{},
        .floatingOffset  = state.hasWorkspacePresentation ? state.floatingOffset : Hyprutils::Math::Vector2D{},
        .workspaceAlpha  = state.hasWorkspacePresentation ? state.workspaceAlpha : 1.F,
        .alpha           = state.active,
        .fadeAlpha       = state.fade * state.fullscreen * state.layout * (!state.hasWorkspacePresentation || state.pinned || TRANSFERRING ? 1.F : state.workspaceAlpha) *
            (TRANSFERRING ? state.moveToWorkspace : 1.F) * state.moveFromWorkspace,
        .workspaceOffsetAnimating = state.hasWorkspacePresentation && state.workspaceOffsetAnimating,
        .alphaVisible             = TOTAL_ALPHA != 0.F || state.alphaAnimating,
    };
}
