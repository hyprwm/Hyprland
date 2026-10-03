#pragma once

#include <cstdint>

namespace Desktop {
    enum class eFadeoutSource : uint8_t;
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
    };

    bool sceneIncludesShell(eSceneMode mode);
    bool sceneSelectsWindow(eSceneMode mode, const SSceneWindowState& window);
    bool sceneSelectsFadeout(eSceneMode mode, Desktop::eFadeoutSource source, bool belongsToWorkspace);
}
