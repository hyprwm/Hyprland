#include "SceneSelection.hpp"

bool Render::sceneIncludesShell(eSceneMode mode) {
    return mode != eSceneMode::WORKSPACE_WINDOWS;
}

bool Render::sceneSelectsWindow(eSceneMode mode, const SSceneWindowState& window) {
    if (mode == eSceneMode::MONITOR)
        return window.monitorVisible;

    return window.mapped && !window.hidden && window.belongsToWorkspace;
}
