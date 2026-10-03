#pragma once

#include <hyprutils/math/Vector2D.hpp>
#include <cstdint>

namespace Render {
    enum class eSceneMode : uint8_t;
    // Owned by draw data: no animation objects or references to live workspace state.
    struct SWindowRenderPresentation {
        Hyprutils::Math::Vector2D workspaceOffset;
        Hyprutils::Math::Vector2D floatingOffset;
        float                     workspaceAlpha = 1.F;
        float                     alpha = 1.F, fadeAlpha = 1.F;
        bool                      workspaceOffsetAnimating = false;
        bool                      alphaVisible             = true;

        bool                      operator==(const SWindowRenderPresentation&) const = default;
    };

    struct SWindowPresentationState {
        Hyprutils::Math::Vector2D workspaceOffset;
        Hyprutils::Math::Vector2D floatingOffset;
        float                     workspaceAlpha = 1.F;
        float                     fade = 1.F, active = 1.F, fullscreen = 1.F, layout = 1.F;
        float                     moveToWorkspace = 1.F, moveFromWorkspace = 1.F;
        bool                      hasWorkspacePresentation    = false;
        bool                      workspaceOffsetAnimating    = false;
        bool                      pinned                      = false;
        bool                      movingFromMonitor           = false;
        bool                      workspaceVisible            = false;
        bool                      alphaAnimating              = false;
        bool                      floatingOffsetFromWorkspace = false;
    };

    SWindowRenderPresentation resolveWindowPresentation(const SWindowPresentationState& state);
    SWindowRenderPresentation resolveWindowPresentation(const SWindowPresentationState& state, eSceneMode mode);
}
