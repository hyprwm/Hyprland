#pragma once

#include <cstdint>

namespace Desktop {
    enum class eFadeoutSource : uint8_t;
    enum eFadeoutPlane : uint8_t;
}

namespace Render {
    enum class eSceneMode : uint8_t {
        MONITOR,
        WORKSPACE_WINDOWS,
        WORKSPACE_WITH_SHELL,
    };

    struct SSceneWindowState {
        bool mapped             = false;
        bool hidden             = false;
        bool belongsToWorkspace = false;
        bool monitorVisible     = false;
        bool floating           = false;
        bool pinned             = false;
        bool onOwnerMonitor     = false;
    };

    struct SScenePopupState {
        bool mapped       = false;
        bool hasResource  = false;
        bool alphaVisible = false;
        bool inert        = false;
        bool acceptsInput = false;
    };

    bool                   sceneIncludesShell(eSceneMode mode);
    bool                   sceneSelectsWindow(eSceneMode mode, const SSceneWindowState& window);
    bool                   sceneSelectsPopup(bool workspaceScene, const SScenePopupState& popup);
    bool                   sceneSelectsFadeout(eSceneMode mode, Desktop::eFadeoutSource source, bool belongsToWorkspace, bool pinnedOnMonitor = false);
    Desktop::eFadeoutPlane sceneFadeoutPlane(eSceneMode mode, Desktop::eFadeoutPlane plane, bool pinned);
}
