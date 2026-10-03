#include "SceneSelection.hpp"
#include "../../desktop/state/Fadeout.hpp"

bool Render::sceneIncludesShell(eSceneMode mode) {
    return mode != eSceneMode::WORKSPACE_WINDOWS;
}

bool Render::sceneSelectsWindow(eSceneMode mode, const SSceneWindowState& window) {
    if (mode == eSceneMode::MONITOR)
        return window.monitorVisible;

    return window.mapped && !window.hidden && window.belongsToWorkspace;
}

bool Render::sceneSelectsFadeout(eSceneMode mode, Desktop::eFadeoutSource source, bool belongsToWorkspace) {
    if (mode == eSceneMode::MONITOR)
        return true;

    switch (source) {
        case Desktop::eFadeoutSource::WINDOW: return belongsToWorkspace;
        case Desktop::eFadeoutSource::LAYER: return sceneIncludesShell(mode);
        default: return false;
    }
}
